#include "ebc/BitcodeRetriever.h"

#include "ebc/BitcodeArchive.h"
#include "ebc/BitcodeContainer.h"
#include "ebc/BitcodeMetadata.h"
#include "ebc/EbcError.h"

#include "ebc/util/Xar.h"

#include "llvm/ADT/Triple.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/MachO.h"
#include "llvm/Object/MachOUniversal.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

using namespace llvm;
using namespace llvm::object;

namespace ebc {

/// Strip path from file name.
static std::string GetFileName(std::string fileName) {
  auto pos = fileName.rfind('/');
  return pos == std::string::npos ? fileName : fileName.substr(pos + 1);
}

/// Internal EBC error that extends LLVM's ErrorInfo class. We use this to have
/// the implementation return llvm::Expected<> objects.
class InternalEbcError : public llvm::ErrorInfo<InternalEbcError> {
 public:
  static char ID;

  InternalEbcError(std::string msg) : _msg(std::move(msg)) {}
  ~InternalEbcError() {}

  const std::string &getMessage() const {
    return _msg;
  }

  std::error_code convertToErrorCode() const override;

  void log(llvm::raw_ostream &OS) const override;

 private:
  std::string _msg;
};

std::error_code InternalEbcError::convertToErrorCode() const {
  llvm_unreachable("Not implemented");
}

void InternalEbcError::log(llvm::raw_ostream &OS) const {
  OS << _msg;
}

char InternalEbcError::ID = 0;

/// Convert an LLVM error object to an EbcError. This works both for LLVM's
/// internal errors as well as for our custom InternalEbcError.
static EbcError ToEbcError(llvm::Error &&e) {
  std::string msg;
  handleAllErrors(std::move(e), [&msg](llvm::ErrorInfoBase &eib) { msg = eib.message(); });
  return EbcError(msg);
}

class BitcodeRetriever::Impl {
 public:
  Impl(std::string objectPath) : _objectPath(std::move(objectPath)) {}

  /// Perform the actual bitcode retrieval. Depending on the type of the object
  /// file the resulting list contains plain bitcode containers or bitcode
  /// archives.
  ///
  /// @return A list of bitcode containers.
  std::vector<BitcodeInfo> GetBitcodeInfo() {
    auto binaryOrErr = createBinary(_objectPath);
    if (!binaryOrErr) {
      throw ToEbcError(binaryOrErr.takeError());
    }

    auto bitcodeContainers = GetBitcodeInfo(*binaryOrErr->getBinary());
    if (!bitcodeContainers) {
      throw ToEbcError(bitcodeContainers.takeError());
    }

    return std::move(*bitcodeContainers);
  }

 private:
  /// Obtains all bitcode from an object. The method basically determines the
  /// kind of object and dispatches the actual work to the specialized method.
  ///
  /// @param binary The binary object.
  ///
  /// @return A list of bitcode containers.
  llvm::Expected<std::vector<BitcodeInfo>> GetBitcodeInfo(const llvm::object::Binary &binary) const {
    auto bitcodeContainers = std::vector<BitcodeInfo>();

    if (const auto *universalBinary = dyn_cast<MachOUniversalBinary>(&binary)) {
      // A fat binary consists either of Mach-O objects or static library (ar)
      // archives for different architectures.
      for (auto object : universalBinary->objects()) {
        Expected<std::unique_ptr<MachOObjectFile>> machOObject = object.getAsObjectFile();
        if (!machOObject) {
          llvm::consumeError(machOObject.takeError());
        } else {
          auto container = GetBitcodeInfoFromMachO(machOObject->get());

          if (!container) {
            return container.takeError();
          }

          bitcodeContainers.push_back(std::move(*container));
          continue;
        }

        Expected<std::unique_ptr<Archive>> archive = object.getAsArchive();
        if (!archive) {
          llvm::consumeError(archive.takeError());
        } else {
          auto containers = GetBitcodeInfoFromArchive(*archive->get());

          if (!containers) {
            return containers.takeError();
          }

          // We have to move all valid containers so we can move on to the next
          // architecture.
          bitcodeContainers.reserve(bitcodeContainers.size() + containers->size());
          std::copy(std::make_move_iterator(containers->begin()), std::make_move_iterator(containers->end()),
                    std::back_inserter(bitcodeContainers));
          continue;
        }
      }
    } else if (const auto machOObjectFile = dyn_cast<MachOObjectFile>(&binary)) {
      auto container = GetBitcodeInfoFromMachO(machOObjectFile);
      if (!container) {
        return container.takeError();
      }

      bitcodeContainers.push_back(std::move(*container));
    } else if (const auto object = dyn_cast<ObjectFile>(&binary)) {
      auto container = GetBitcodeInfoFromObject(object);
      if (!container) {
        return container.takeError();
      }

      bitcodeContainers.push_back(std::move(*container));
    } else if (const auto archive = dyn_cast<Archive>(&binary)) {
      // We can return early to prevent moving all containers in the vector.
      return GetBitcodeInfoFromArchive(*archive);
    } else {
      return llvm::make_error<InternalEbcError>("Unsupported binary");
    }

    return std::move(bitcodeContainers);
  }

  /// Obtains all bitcode from an object archive.
  ///
  /// @param archive The object archive.
  ///
  /// @return A list of bitcode containers.
  llvm::Expected<std::vector<BitcodeInfo>> GetBitcodeInfoFromArchive(const llvm::object::Archive &archive) const {
    Error err = Error::success();
    auto bitcodeContainers = std::vector<BitcodeInfo>();
    // Archives consist of object files.
    for (const auto &child : archive.children(err)) {
      if (err) {
        return std::move(err);
      }

      auto childOrErr = child.getAsBinary();
      if (!childOrErr) {
        return childOrErr.takeError();
      }

      auto containers = GetBitcodeInfo(*(*childOrErr));
      if (!containers) {
        return containers.takeError();
      }

      bitcodeContainers.reserve(bitcodeContainers.size() + containers->size());
      std::move(containers->begin(), containers->end(), std::back_inserter(bitcodeContainers));
    }

    // Don't forget to check error one last time, in case there were no
    // children and body of the for loop was never executed.
    if (err) {
      return std::move(err);
    }

    return std::move(bitcodeContainers);
  }

  /// Reads bitcode from a Mach O object file.
  ///
  /// @param objectFile The Mach O object file.
  ///
  /// @return The bitcode container.
  llvm::Expected<BitcodeInfo> GetBitcodeInfoFromMachO(const llvm::object::MachOObjectFile *objectFile) const {
    // For MachO return the correct arch tripple.
    const std::string arch = objectFile->getArchTriple(nullptr).getArchName().str();

    auto bitcodeContainer = GetBitcodeInfo(objectFile->section_begin(), objectFile->section_end());

    if (bitcodeContainer != nullptr) {
      // Set binary metadata
      bitcodeContainer->GetBinaryMetadata().SetFileName(GetFileName(objectFile->getFileName().str()));
      bitcodeContainer->GetBinaryMetadata().SetFileFormatName(objectFile->getFileFormatName().str());
      bitcodeContainer->GetBinaryMetadata().SetArch(arch);
      bitcodeContainer->GetBinaryMetadata().SetUuid(objectFile->getUuid().data());
    }

    return BitcodeInfo(arch, std::move(bitcodeContainer));
  }

  /// Reads bitcode from a plain object file.
  ///
  /// @param objectFile The Mach O object file.
  ///
  /// @return The bitcode container.
  llvm::Expected<BitcodeInfo> GetBitcodeInfoFromObject(const llvm::object::ObjectFile *objectFile) const {
    const auto arch = llvm::Triple::getArchTypeName(static_cast<Triple::ArchType>(objectFile->getArch()));

    auto bitcodeContainer = GetBitcodeInfo(objectFile->section_begin(), objectFile->section_end());

    if (bitcodeContainer != nullptr) {
      // Set binary metadata
      bitcodeContainer->GetBinaryMetadata().SetFileName(GetFileName(objectFile->getFileName().str()));
      bitcodeContainer->GetBinaryMetadata().SetFileFormatName(objectFile->getFileFormatName().str());
      bitcodeContainer->GetBinaryMetadata().SetArch(arch.str());
    }

    return BitcodeInfo(arch.str(), std::move(bitcodeContainer));
  }

  /// Obtains data from a section.
  ///
  /// @param section The section from which the data should be obtained.
  ///
  /// @return A pair with the data and the size of the data.
  static std::pair<const char *, std::size_t> GetSectionData(const llvm::object::SectionRef &section) {
    auto bytesStr = section.getContents().get();
    const char *sect = reinterpret_cast<const char *>(bytesStr.data());
    return {sect, bytesStr.size()};
  }

  /// Obtains compiler commands from a section. It automatically parses the
  /// data into a vector.
  ///
  /// @param section The sectio from which to read the compiler commands.
  ///
  /// @return A vector of compiler commands.
  static std::vector<std::string> GetCommands(const llvm::object::SectionRef &section) {
    auto data = GetSectionData(section);
    const char *p = data.first;
    const char *end = data.first + data.second;

    // Create list of strings from commands separated by null bytes.
    std::vector<std::string> cmds;
    do {
      // Creating a string from p consumes data until next null byte.
      cmds.push_back(std::string(p));
      // Continue after the null byte.
      p += cmds.back().size() + 1;
    } while (p < end);

    return cmds;
  }

  static std::unique_ptr<BitcodeContainer> GetBitcodeInfo(section_iterator begin, section_iterator end) {
    std::unique_ptr<BitcodeContainer> bitcodeContainer;
    std::vector<std::string> commands;

    for (auto it = begin; it != end; ++it) {
      StringRef sectName = it->getName().get();

      if (sectName == ".llvmbc" || sectName == "__bitcode") {
        assert(!bitcodeContainer && "Multiple bitcode sections!");
        auto data = GetSectionData(*it);
        bitcodeContainer = std::make_unique<BitcodeContainer>(data.first, data.second);
      } else if (sectName == "__bundle") {
        assert(!bitcodeContainer && "Multiple bitcode sections!");
        auto data = GetSectionData(*it);
        bitcodeContainer = std::unique_ptr<BitcodeContainer>(new BitcodeArchive(data.first, data.second));
      } else if (sectName == "__cmd" || sectName == "__cmdline" || sectName == ".llvmcmd") {
        assert(commands.empty() && "Multiple command sections!");
        commands = GetCommands(*it);
      }
    }

    if(!commands.empty()){
      assert(bitcodeContainer != nullptr && "Expected bitcode container!");
      bitcodeContainer->SetCommands(commands);
    }

    return bitcodeContainer;
  }

  std::string _objectPath;
};

BitcodeRetriever::BitcodeRetriever(std::string objectPath) : _impl(std::make_unique<Impl>(std::move(objectPath))) {}
BitcodeRetriever::~BitcodeRetriever() = default;

std::vector<BitcodeRetriever::BitcodeInfo> BitcodeRetriever::GetBitcodeInfo() {
  return _impl->GetBitcodeInfo();
}

}  // namespace ebc
