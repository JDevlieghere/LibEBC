#pragma once

#include "ebc/BinaryMetadata.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ebc {
class EmbeddedFile;
class BitcodeContainer {
 public:
  BitcodeContainer(const char* data, std::size_t size);

  BitcodeContainer(BitcodeContainer&& bitcodeArchive) noexcept;

  virtual ~BitcodeContainer();

  /// Indicates whether the given bitcode container is a bitcode archive.
  ///
  /// @return False.
  virtual bool IsArchive() const;

  /// Indicates whether the given bitcode container is merely a placeholder.
  ///
  /// @return  True if data is empty.
  bool IsEmpty() const;

  /// Get the commands passed to the compiler front end.
  ///
  /// @return The commands passed to the compiler front end.
  const std::vector<std::string>& GetCommands() const;

  /// Set the commands passed to the compiler front end.
  ///
  /// @param cmd The commands passed to the compiler front end.
  void SetCommands(const std::vector<std::string>& cmd);

  /// Get the binary meta data associated with this bitcode container.
  ///
  /// @return The binary meta data.
  BinaryMetadata& GetBinaryMetadata();

  /// Set the binary meta data associated with this bitcode container.
  ///
  /// @return  The binary meta data.
  const BinaryMetadata& GetBinaryMetadata() const;

  /// Extract individual bitcode files from this container and return a vector
  /// of file names. This operation can be expensive as it decompresses each
  /// bitcode file.
  ///
  /// @return A vector of bitcode files.
  virtual std::vector<std::unique_ptr<EmbeddedFile>> GetEmbeddedFiles() const;

  const std::string& GetPrefix() const;
  void SetPrefix(std::string prefix);

 protected:
  void SetData(const char* data, std::size_t size) noexcept;
  std::pair<const char*, std::size_t> GetData() const;
  int verifyBC(std::string filename) const;

 private:
  std::vector<std::size_t> GetEmbeddedFileOffsets() const;

  char* _data;
  std::size_t _size;

  std::vector<std::string> _commands;
  BinaryMetadata _binaryMetadata;

  std::string _prefix;
};
}
