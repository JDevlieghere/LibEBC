#pragma once

#include <ebc/BitcodeType.h>

#include <cstdint>
#include <string>

namespace ebc {
namespace util {
namespace bitcode {

static constexpr auto BC_MAGIC = 0x0B17C0DEu;
static constexpr auto BC_VERSION = 0x0000000u;
static constexpr auto IR_MAGIC = 0x4243C0DEu;
static constexpr auto IR_CIGAM = 0xDEC04342u;

BitcodeType GetBitcodeType(std::uint64_t magic);

/// Returns true if the first four bytes fo the given file match the LLVM
/// bitcode magic number.
///
/// @param file Path to the file to check.
///
/// @return True if the magic number BC 0xC0DE is found.
BitcodeType GetBitcodeType(std::string file);

/// Write data to file with given file name.
///
/// @param data Bitcode binary data.
/// @param size Bitcode binary data size.
/// @param fileName The desired filename for the bitcode file.
void WriteToFile(const char *data, std::size_t size, std::string file);
void AppendToFile(const char *data, std::size_t size, std::string filename);
}
}
}
