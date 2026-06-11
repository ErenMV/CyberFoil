#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tin::nczblock
{
    constexpr std::uint8_t kTypeZstd = 0x01;
    constexpr std::uint32_t kZstdFrameMagic = 0xFD2FB528u;
    constexpr std::size_t kZstdMagicSize = sizeof(kZstdFrameMagic);

    enum class BlockDecodeMode
    {
        NeedMoreData,
        Zstd,
        Direct,
        Invalid,
    };

    struct StreamValidationState
    {
        bool headerParsed = false;
        bool blockSizesParsed = false;
        std::uint32_t blockCount = 0;
        std::uint32_t currentBlockIndex = 0;
        bool currentBlockOpen = false;
        std::uint64_t currentBlockReadOffset = 0;
        std::uint64_t currentBlockCompressedSize = 0;
        std::uint64_t totalDecompressedWritten = 0;
        std::uint64_t expectedDecompressedSize = 0;
    };

    bool HasZstdFrameMagic(const std::vector<std::uint8_t>& prefix);

    BlockDecodeMode DecideBlockDecodeMode(std::uint8_t compressionType,
                                          std::uint64_t compressedSize,
                                          std::uint64_t expectedDecompressedSize,
                                          const std::vector<std::uint8_t>& prefix,
                                          bool blockFinished);

    const char* DescribeBlockDecodeMode(BlockDecodeMode mode);

    bool ValidateStreamCompletion(const StreamValidationState& state, std::string& error);
}
