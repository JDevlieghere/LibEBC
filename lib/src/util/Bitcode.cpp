#include "ebc/util/Bitcode.h"

#include "ebc/Config.h"

#include <fstream>
#include <iostream>

namespace ebc {
namespace util {
namespace bitcode {

BitcodeType GetBitcodeType(std::uint64_t data) {
  std::uint32_t version = data >> 32;
  std::uint32_t magic = data & 0xFFFFFFFF;

  if (magic == IR_MAGIC || magic == IR_CIGAM) {
    return BitcodeType::IR;
  } else if (magic == BC_MAGIC) {
    if (version == BC_VERSION) {
      return BitcodeType::BitcodeWrapper;
    }
    return BitcodeType::Bitcode;
  }
  return BitcodeType::Unknown;
}

BitcodeType GetBitcodeType(std::string file) {
  // Read first four bytes and check magic number.
  std::ifstream input(file, std::ifstream::binary);

  if (!input) {
    std::cerr << "Unable to open " << file << std::endl;
    return BitcodeType::Unknown;
  }

  char buffer[8];
  input.read(buffer, 8);
  return GetBitcodeType(*reinterpret_cast<std::uint64_t *>(buffer));
}

void WriteToFile(const char *data, std::size_t size, std::string file) {
  std::ofstream output(file, std::ofstream::binary);

  if (!output) {
    std::cerr << "Unable to open " << file << std::endl;
    return;
  }

  output.write(data, static_cast<std::streamsize>(size));
  output.close();
}

void AppendToFile(const char *data, std::size_t size, std::string filename) {
  std::ofstream file;
  file.open(filename, std::ios::app);
  if (file.bad())
  {
    std::cerr << "can not open file" << std::endl;
  }
  
  file.write(data, static_cast<std::streamsize>(size));
  file.close();
}

}  // namespace bitcode
}  // namespace util
}  // namespace ebc
