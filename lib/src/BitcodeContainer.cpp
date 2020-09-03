#include "ebc/BitcodeContainer.h"

#include "ebc/BitcodeType.h"
#include "ebc/EmbeddedFile.h"
#include "ebc/EmbeddedFileFactory.h"
#include "ebc/util/Bitcode.h"
#include "ebc/util/UUID.h"

#include "llvm/IR/Module.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <streambuf>

using namespace llvm;
namespace ebc {

BitcodeContainer::BitcodeContainer(const char *data, std::size_t size)
    : _data(nullptr), _size(size), _commands(), _prefix() {
  SetData(data, size);
}

BitcodeContainer::BitcodeContainer(BitcodeContainer &&bitcodeContainer) noexcept
    : _data(nullptr)
    , _size(bitcodeContainer._size)
    , _commands(bitcodeContainer._commands)
    , _binaryMetadata(bitcodeContainer._binaryMetadata)
    , _prefix() {
  SetData(bitcodeContainer._data, bitcodeContainer._size);
  bitcodeContainer._data = nullptr;
}

BitcodeContainer::~BitcodeContainer() {
  if (_data != nullptr) {
    delete[] _data;
    _data = nullptr;
  }
}

bool BitcodeContainer::IsArchive() const {
  return false;
}

bool BitcodeContainer::IsEmpty() const {
  return _size == 0;
}

const std::vector<std::string> &BitcodeContainer::GetCommands() const {
  return _commands;
}

void BitcodeContainer::SetCommands(const std::vector<std::string> &commands) {
  _commands = commands;
}

void BitcodeContainer::SetData(const char *data, std::size_t size) noexcept {
  if (size > 0) {
    _data = new char[size];
    std::copy(data, data + size, _data);
  }
}

std::pair<const char *, std::size_t> BitcodeContainer::GetData() const {
  return std::make_pair(_data, _size);
}

BinaryMetadata &BitcodeContainer::GetBinaryMetadata() {
  return _binaryMetadata;
}

const BinaryMetadata &BitcodeContainer::GetBinaryMetadata() const {
  return _binaryMetadata;
}

std::vector<std::unique_ptr<EmbeddedFile>> BitcodeContainer::GetEmbeddedFiles() const {
  // Magic number + wrapper version is 8 bytes long. If less than 8 bytes are
  // available there is no valid bitcode present. Likely only a bitcode marker
  // was embedded.
  if (IsEmpty() || _size < 8) {
    return {};
  }

  std::vector<std::unique_ptr<EmbeddedFile>> files;
  auto offsets = GetEmbeddedFileOffsets();
  for (std::size_t i = 0; i < offsets.size() - 1; ++i) {
    std::size_t begin = offsets[i];
    std::size_t end = offsets[i + 1];
    std::size_t size = end - begin;

    auto fileName = _prefix + util::uuid::UuidToString(util::uuid::GenerateUUID());

    fileName = fileName + ".bc";
    util::bitcode::WriteToFile(_data + begin, size, fileName);  // 此处需要处理末尾的00

    if (verifyBC(fileName) != 0) {
      util::bitcode::WriteToFile(_data + begin, size - 4, fileName);

      if (verifyBC(fileName) != 0) {
        const char padding[4] = {0, 0, 0, 0};
        util::bitcode::AppendToFile(padding, 4, fileName);
        if (verifyBC(fileName) != 0) {
          std::cerr << "eroor" << std::endl;
        }
      }
    }

    auto embeddedFile = EmbeddedFileFactory::CreateEmbeddedFile(fileName);
    embeddedFile->SetCommands(_commands, EmbeddedFile::CommandSource::Clang);
    files.push_back(std::move(embeddedFile));
  }

  return files;
}

std::vector<std::size_t> BitcodeContainer::GetEmbeddedFileOffsets() const {
  std::vector<std::size_t> offsets;
  for (std::size_t i = 0; i <= (_size - 8); i++) {
    if (util::bitcode::GetBitcodeType(*reinterpret_cast<std::uint64_t *>(_data + i)) != BitcodeType::Unknown) {
      offsets.push_back(i);
    }
  }
  offsets.push_back(_size);
  return offsets;
}

const std::string &BitcodeContainer::GetPrefix() const {
  return _prefix;
}

void BitcodeContainer::SetPrefix(std::string prefix) {
  _prefix = std::move(prefix);
}

static void DiagnosticHandler(const DiagnosticInfo &DI, void *Context) {
    bool *HasError = static_cast<bool *>(Context);
    if (DI.getSeverity() == DS_Error)
        *HasError = true;

    DiagnosticPrinterRawOStream DP(errs());
    errs() << LLVMContext::getDiagnosticMessagePrefix(DI.getSeverity()) << ": ";
    DI.print(DP);
    errs() << "\n";
}

int BitcodeContainer::verifyBC(std::string filename) const {
    LLVMContext Context;
    Context.setDiscardValueNames(false);
    // Set a diagnostic handler that doesn't exit on the first error
    bool HasError = false;
    Context.setDiagnosticHandler(DiagnosticHandler, &HasError);

    //input module
    SMDiagnostic Err;
    std::unique_ptr<Module> theOrginal = parseIRFile(filename, Err, Context);
    if (!theOrginal) {
        std ::cerr << filename <<std::endl;
        std::cerr << "can not parse bitcode!" << Err.getMessage().str() <<"\n";

        return 1;
    }

    return 0;
}

}  // namespace ebc