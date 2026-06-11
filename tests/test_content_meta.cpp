#include "nx/content_meta.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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

    void ExpectBytesEqual(const void* actual, const void* expected, std::size_t size, const std::string& message)
    {
        if (std::memcmp(actual, expected, size) != 0)
            Fail(message);
    }

    NcmContentId MakeContentId(std::uint8_t seed)
    {
        NcmContentId id{};
        for (std::size_t i = 0; i < sizeof(id.c); i++)
            id.c[i] = static_cast<std::uint8_t>(seed + i);
        return id;
    }

    NcmContentInfo MakeContentInfo(std::uint8_t seed, std::uint8_t type, std::uint64_t size)
    {
        NcmContentInfo info{};
        info.content_id = MakeContentId(seed);
        ncmU64ToContentInfoSize(size, &info);
        info.content_type = type;
        info.id_offset = static_cast<std::uint8_t>(seed ^ 0x5A);
        return info;
    }
}

int main()
{
    nx::ncm::PackagedContentMetaHeader packagedHeader{};
    packagedHeader.title_id = 0x0100F43008C44000ULL;
    packagedHeader.version = 0x1234;
    packagedHeader.type = NcmContentMetaType_Patch;
    packagedHeader.extended_header_size = sizeof(NcmPatchMetaExtendedHeader);
    packagedHeader.content_count = 2;
    packagedHeader.content_meta_count = 1;
    packagedHeader.attributes = NcmContentMetaAttribute_Compacted;

    NcmPatchMetaExtendedHeader patchHeader{};
    patchHeader.application_id = 0x0100F43008C44800ULL;
    patchHeader.required_system_version = 0x44556677;
    patchHeader.extended_data_size = 4;

    nx::ncm::PackagedContentInfo packagedInfos[2]{};
    std::memset(packagedInfos[0].hash, 0x11, sizeof(packagedInfos[0].hash));
    std::memset(packagedInfos[1].hash, 0x22, sizeof(packagedInfos[1].hash));
    packagedInfos[0].content_info = MakeContentInfo(0x10, NcmContentType_Program, 0x123456);
    packagedInfos[1].content_info = MakeContentInfo(0x20, NcmContentType_DeltaFragment, 0x654321);

    NcmContentMetaInfo metaInfo{};
    metaInfo.id = 0x0100F43008C44880ULL;
    metaInfo.version = 0xCAFE;
    metaInfo.type = NcmContentMetaType_Delta;
    metaInfo.attr = 0x7F;

    const std::array<std::uint8_t, 4> patchExtendedData{0xDE, 0xAD, 0xBE, 0xEF};

    std::vector<std::uint8_t> packagedBytes(
        sizeof(packagedHeader) +
        sizeof(patchHeader) +
        sizeof(packagedInfos) +
        sizeof(metaInfo) +
        patchExtendedData.size());

    std::size_t offset = 0;
    std::memcpy(packagedBytes.data() + offset, &packagedHeader, sizeof(packagedHeader));
    offset += sizeof(packagedHeader);
    std::memcpy(packagedBytes.data() + offset, &patchHeader, sizeof(patchHeader));
    offset += sizeof(patchHeader);
    std::memcpy(packagedBytes.data() + offset, &packagedInfos, sizeof(packagedInfos));
    offset += sizeof(packagedInfos);
    std::memcpy(packagedBytes.data() + offset, &metaInfo, sizeof(metaInfo));
    offset += sizeof(metaInfo);
    std::memcpy(packagedBytes.data() + offset, patchExtendedData.data(), patchExtendedData.size());

    nx::ncm::ContentMeta meta(packagedBytes.data(), packagedBytes.size());

    const auto installableInfos = meta.GetContentInfos();
    Expect(installableInfos.size() == 1, "default content info view must still skip delta fragments");
    Expect(installableInfos[0].content_type == NcmContentType_Program,
           "installable content records must keep the non-delta entry");

    const auto allInfos = meta.GetContentInfos(true);
    Expect(allInfos.size() == 2, "metadata view must keep delta fragment records");
    Expect(allInfos[1].content_type == NcmContentType_DeltaFragment,
           "delta fragment record must remain present for patch metadata");

    NcmContentInfo cnmtContentInfo = MakeContentInfo(0x30, NcmContentType_Meta, 0x1111);
    tin::data::ByteBuffer installContentMeta;
    meta.GetInstallContentMeta(installContentMeta, cnmtContentInfo, false);

    Expect(installContentMeta.GetSize() ==
               sizeof(NcmContentMetaHeader) +
               sizeof(NcmPatchMetaExtendedHeader) +
               sizeof(NcmContentInfo) * 3 +
               sizeof(metaInfo) +
               patchExtendedData.size(),
           "install content meta size must preserve the packaged tail");

    const auto* installHeader = reinterpret_cast<const NcmContentMetaHeader*>(installContentMeta.GetData());
    Expect(installHeader->extended_header_size == sizeof(NcmPatchMetaExtendedHeader),
           "install header must keep patch extended header size");
    Expect(installHeader->content_count == 3,
           "install header must include cnmt plus all packaged content records");
    Expect(installHeader->content_meta_count == 1,
           "install header must keep content meta count");
    Expect(installHeader->attributes == NcmContentMetaAttribute_Compacted,
           "install header must keep metadata attributes");

    const auto* installPatchHeader = reinterpret_cast<const NcmPatchMetaExtendedHeader*>(
        installContentMeta.GetData() + sizeof(NcmContentMetaHeader));
    ExpectBytesEqual(installPatchHeader, &patchHeader, sizeof(patchHeader),
                     "patch extended header must be copied verbatim");

    const auto* installInfos = reinterpret_cast<const NcmContentInfo*>(
        installContentMeta.GetData() +
        sizeof(NcmContentMetaHeader) +
        sizeof(NcmPatchMetaExtendedHeader));
    ExpectBytesEqual(&installInfos[0], &cnmtContentInfo, sizeof(NcmContentInfo),
                     "cnmt content record must be inserted first");
    ExpectBytesEqual(&installInfos[1], &packagedInfos[0].content_info, sizeof(NcmContentInfo),
                     "program content record must be preserved");
    ExpectBytesEqual(&installInfos[2], &packagedInfos[1].content_info, sizeof(NcmContentInfo),
                     "delta fragment record must remain in patch metadata");

    const std::size_t installTailOffset =
        sizeof(NcmContentMetaHeader) +
        sizeof(NcmPatchMetaExtendedHeader) +
        sizeof(NcmContentInfo) * 3;
    ExpectBytesEqual(installContentMeta.GetData() + installTailOffset, &metaInfo, sizeof(metaInfo),
                     "metadata tail must preserve content meta info bytes");
    ExpectBytesEqual(installContentMeta.GetData() + installTailOffset + sizeof(metaInfo),
                     patchExtendedData.data(),
                     patchExtendedData.size(),
                     "metadata tail must preserve patch extended data bytes");

    std::cout << "All content meta tests passed\n";
    return 0;
}
