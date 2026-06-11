#include "util/nczblock.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    [[noreturn]] void Fail(const std::string& message)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }

    void Expect(bool condition, const std::string& message)
    {
        if (!condition)
            Fail(message);
    }

    std::vector<std::uint8_t> ZstdMagicPrefix()
    {
        const std::uint32_t magic = tin::nczblock::kZstdFrameMagic;
        return {
            static_cast<std::uint8_t>(magic & 0xFF),
            static_cast<std::uint8_t>((magic >> 8) & 0xFF),
            static_cast<std::uint8_t>((magic >> 16) & 0xFF),
            static_cast<std::uint8_t>((magic >> 24) & 0xFF),
        };
    }
}

int main()
{
    using tin::nczblock::BlockDecodeMode;
    using tin::nczblock::StreamValidationState;

    {
        const auto mode = tin::nczblock::DecideBlockDecodeMode(
            tin::nczblock::kTypeZstd,
            0x4000,
            0x4000,
            ZstdMagicPrefix(),
            false);
        Expect(mode == BlockDecodeMode::Zstd,
               "equal-size zstd block with zstd magic must decode as zstd");
    }

    {
        const auto mode = tin::nczblock::DecideBlockDecodeMode(
            tin::nczblock::kTypeZstd,
            0x4000,
            0x4000,
            {0x11, 0x22, 0x33, 0x44},
            true);
        Expect(mode == BlockDecodeMode::Direct,
               "equal-size block without zstd magic may fall back to direct");
    }

    {
        const auto mode = tin::nczblock::DecideBlockDecodeMode(
            tin::nczblock::kTypeZstd,
            0x3000,
            0x4000,
            {0x11, 0x22, 0x33, 0x44},
            true);
        Expect(mode == BlockDecodeMode::Invalid,
               "compressed block without zstd magic must be rejected");
    }

    {
        const auto mode = tin::nczblock::DecideBlockDecodeMode(
            tin::nczblock::kTypeZstd,
            0x3000,
            0x4000,
            {0x28, 0xB5},
            false);
        Expect(mode == BlockDecodeMode::NeedMoreData,
               "short prefix must request more data before decode decision");
    }

    {
        StreamValidationState state;
        state.headerParsed = true;
        state.blockSizesParsed = true;
        state.blockCount = 3;
        state.currentBlockIndex = 3;
        state.totalDecompressedWritten = 0x9000;
        state.expectedDecompressedSize = 0x9000;

        std::string error;
        Expect(tin::nczblock::ValidateStreamCompletion(state, error),
               "complete stream state should validate");
        Expect(error.empty(), "successful validation should clear error");
    }

    {
        StreamValidationState state;
        state.headerParsed = true;
        state.blockSizesParsed = true;
        state.blockCount = 3;
        state.currentBlockIndex = 2;
        state.totalDecompressedWritten = 0x9000;
        state.expectedDecompressedSize = 0x9000;

        std::string error;
        Expect(!tin::nczblock::ValidateStreamCompletion(state, error),
               "missing block consumption must fail validation");
        Expect(!error.empty(), "failed validation must set error");
    }

    {
        StreamValidationState state;
        state.headerParsed = true;
        state.blockSizesParsed = true;
        state.blockCount = 1;
        state.currentBlockIndex = 1;
        state.currentBlockOpen = true;
        state.currentBlockReadOffset = 0x1000;
        state.currentBlockCompressedSize = 0x2000;
        state.totalDecompressedWritten = 0x2000;
        state.expectedDecompressedSize = 0x2000;

        std::string error;
        Expect(!tin::nczblock::ValidateStreamCompletion(state, error),
               "partial current block must fail validation");
    }

    {
        StreamValidationState state;
        state.headerParsed = true;
        state.blockSizesParsed = true;
        state.blockCount = 1;
        state.currentBlockIndex = 1;
        state.totalDecompressedWritten = 0x1FFF;
        state.expectedDecompressedSize = 0x2000;

        std::string error;
        Expect(!tin::nczblock::ValidateStreamCompletion(state, error),
               "decompressed size mismatch must fail validation");
    }

    std::cout << "All NCZBLOCK tests passed\n";
    return 0;
}
