#include "util/nczblock.hpp"

#include <cstring>

namespace tin::nczblock
{
    bool HasZstdFrameMagic(const std::vector<std::uint8_t>& prefix)
    {
        if (prefix.size() < kZstdMagicSize)
            return false;

        std::uint32_t magic = 0;
        std::memcpy(&magic, prefix.data(), sizeof(magic));
        return magic == kZstdFrameMagic;
    }

    BlockDecodeMode DecideBlockDecodeMode(const std::uint8_t compressionType,
                                          const std::uint64_t compressedSize,
                                          const std::uint64_t expectedDecompressedSize,
                                          const std::vector<std::uint8_t>& prefix,
                                          const bool blockFinished)
    {
        if (compressionType != kTypeZstd)
            return BlockDecodeMode::Direct;

        if (HasZstdFrameMagic(prefix))
            return BlockDecodeMode::Zstd;

        if (!blockFinished && prefix.size() < kZstdMagicSize)
            return BlockDecodeMode::NeedMoreData;

        if (compressedSize == expectedDecompressedSize)
            return BlockDecodeMode::Direct;

        return BlockDecodeMode::Invalid;
    }

    const char* DescribeBlockDecodeMode(const BlockDecodeMode mode)
    {
        switch (mode)
        {
            case BlockDecodeMode::NeedMoreData:
                return "need_more_data";
            case BlockDecodeMode::Zstd:
                return "zstd";
            case BlockDecodeMode::Direct:
                return "direct";
            case BlockDecodeMode::Invalid:
                return "invalid";
        }

        return "unknown";
    }

    bool ValidateStreamCompletion(const StreamValidationState& state, std::string& error)
    {
        if (!state.headerParsed)
        {
            error = "stream ended before NCZBLOCK header was fully read";
            return false;
        }

        if (!state.blockSizesParsed)
        {
            error = "stream ended before NCZBLOCK block table was fully read";
            return false;
        }

        if (state.currentBlockOpen)
        {
            if (state.currentBlockReadOffset != state.currentBlockCompressedSize)
            {
                error = "stream ended before current NCZBLOCK block was fully read";
                return false;
            }
        }

        if (state.currentBlockIndex != state.blockCount)
        {
            error = "stream ended before all NCZBLOCK blocks were consumed";
            return false;
        }

        if (state.totalDecompressedWritten != state.expectedDecompressedSize)
        {
            error = "NCZBLOCK decompressed size mismatch";
            return false;
        }

        error.clear();
        return true;
    }
}
