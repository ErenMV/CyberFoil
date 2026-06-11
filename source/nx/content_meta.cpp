/*
Copyright (c) 2017-2018 Adubbz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "nx/content_meta.hpp"

#include <string.h>
#include "util/debug.h"
#include "util/error.hpp"

namespace nx::ncm
{
    ContentMeta::ContentMeta()
    {
        m_bytes.Resize(sizeof(PackagedContentMetaHeader));
    }

    ContentMeta::ContentMeta(u8* data, size_t size) :
        m_bytes(size)
    {
        if (size < sizeof(PackagedContentMetaHeader))
            THROW_FORMAT("Content meta data size is too small!");

        m_bytes.Resize(size);
        memcpy(m_bytes.GetData(), data, size);
    }

    PackagedContentMetaHeader ContentMeta::GetPackagedContentMetaHeader()
    {
        return m_bytes.Read<PackagedContentMetaHeader>(0);
    }

    NcmContentMetaKey ContentMeta::GetContentMetaKey()
    {
        NcmContentMetaKey metaRecord;
        PackagedContentMetaHeader contentMetaHeader = this->GetPackagedContentMetaHeader();

        memset(&metaRecord, 0, sizeof(NcmContentMetaKey));
        metaRecord.id = contentMetaHeader.title_id;
        metaRecord.version = contentMetaHeader.version;
        metaRecord.type = static_cast<NcmContentMetaType>(contentMetaHeader.type);

        return metaRecord;
    }

    // TODO: Cache this
    std::vector<NcmContentInfo> ContentMeta::GetContentInfos(bool includeDeltaFragments)
    {
        PackagedContentMetaHeader contentMetaHeader = this->GetPackagedContentMetaHeader();

        std::vector<NcmContentInfo> contentInfos;
        PackagedContentInfo* packagedContentInfos = (PackagedContentInfo*)(m_bytes.GetData() + sizeof(PackagedContentMetaHeader) + contentMetaHeader.extended_header_size);

        for (unsigned int i = 0; i < contentMetaHeader.content_count; i++)
        {
            PackagedContentInfo packagedContentInfo = packagedContentInfos[i];

            // Delta fragments should not be scheduled as standalone installs, but patch
            // metadata still needs to preserve their content records verbatim.
            if (includeDeltaFragments || static_cast<u8>(packagedContentInfo.content_info.content_type) <= NcmContentType_LegalInformation)
            {
                contentInfos.push_back(packagedContentInfo.content_info); 
            }
        }

        return contentInfos;
    }

    void ContentMeta::GetInstallContentMeta(tin::data::ByteBuffer& installContentMetaBuffer, NcmContentInfo& cnmtNcmContentInfo, bool ignoreReqFirmVersion)
    {
        PackagedContentMetaHeader packagedContentMetaHeader = this->GetPackagedContentMetaHeader();
        std::vector<NcmContentInfo> contentInfos = this->GetContentInfos(true);
        const size_t packagedContentInfosOffset = sizeof(PackagedContentMetaHeader) + packagedContentMetaHeader.extended_header_size;
        const size_t packagedContentInfosSize = sizeof(PackagedContentInfo) * packagedContentMetaHeader.content_count;
        const size_t packagedTailOffset = packagedContentInfosOffset + packagedContentInfosSize;
        const size_t packagedTailSize = (m_bytes.GetSize() > packagedTailOffset) ? (m_bytes.GetSize() - packagedTailOffset) : 0;

        // Setup the content meta header
        NcmContentMetaHeader contentMetaHeader;
        contentMetaHeader.extended_header_size = packagedContentMetaHeader.extended_header_size;
        contentMetaHeader.content_count = contentInfos.size() + 1; // Add one for the cnmt content record
        contentMetaHeader.content_meta_count = packagedContentMetaHeader.content_meta_count;
        contentMetaHeader.attributes = packagedContentMetaHeader.attributes;
        contentMetaHeader.storage_id = 0;

        installContentMetaBuffer.Append<NcmContentMetaHeader>(contentMetaHeader);

        // Setup the meta extended header
        LOG_DEBUG("Install content meta pre size: 0x%lx\n", installContentMetaBuffer.GetSize());
        installContentMetaBuffer.Resize(installContentMetaBuffer.GetSize() + contentMetaHeader.extended_header_size);
        LOG_DEBUG("Install content meta post size: 0x%lx\n", installContentMetaBuffer.GetSize());
        auto* extendedHeaderSourceBytes = m_bytes.GetData() + sizeof(PackagedContentMetaHeader);
        u8* installExtendedHeaderStart = installContentMetaBuffer.GetData() + sizeof(NcmContentMetaHeader);
        memcpy(installExtendedHeaderStart, extendedHeaderSourceBytes, contentMetaHeader.extended_header_size);

        // Optionally disable the required system version field
        if (ignoreReqFirmVersion && (packagedContentMetaHeader.type == NcmContentMetaType_Application || packagedContentMetaHeader.type == NcmContentMetaType_Patch))
        {
            installContentMetaBuffer.Write<u32>(0, sizeof(NcmContentMetaHeader) + 8);
        }

        // Setup cnmt content record
        installContentMetaBuffer.Append<NcmContentInfo>(cnmtNcmContentInfo);

        // Setup the content records
        for (auto& contentInfo : contentInfos)
        {
            installContentMetaBuffer.Append<NcmContentInfo>(contentInfo);
        }

        if (packagedTailSize > 0)
        {
            const size_t installTailOffset = installContentMetaBuffer.GetSize();
            installContentMetaBuffer.Resize(installTailOffset + packagedTailSize);
            memcpy(installContentMetaBuffer.GetData() + installTailOffset, m_bytes.GetData() + packagedTailOffset, packagedTailSize);
        }
    }
}
