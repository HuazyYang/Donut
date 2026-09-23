/*
* Copyright (c) 2014-2026, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#include <donut/engine/KTX2File.h>

#if DONUT_WITH_KTX

#include <donut/engine/TextureCache.h>
#include <donut/core/vfs/VFS.h>
#include <donut/core/log.h>

#include <zstd.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>

using namespace donut::vfs;

namespace donut::engine
{
    namespace
    {
        // KTX2 spec §3: the 12-byte file identifier.
        const uint8_t c_KTX2Identifier[12] = {
            0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
        };

        // The Khronos Data Format / Vulkan vkFormat values used by KTX2.  Only the
        // concrete block-compressed (BCn) formats are listed; this loader does not
        // handle uncompressed or Basis (ETC1S/UASTC, vkFormat==UNDEFINED) payloads.
        enum VkFormatBCn : uint32_t
        {
            VK_FORMAT_UNDEFINED          = 0,
            VK_FORMAT_BC1_RGB_UNORM      = 131,
            VK_FORMAT_BC1_RGB_SRGB       = 132,
            VK_FORMAT_BC1_RGBA_UNORM     = 133,
            VK_FORMAT_BC1_RGBA_SRGB      = 134,
            VK_FORMAT_BC2_UNORM          = 135,
            VK_FORMAT_BC2_SRGB           = 136,
            VK_FORMAT_BC3_UNORM          = 137,
            VK_FORMAT_BC3_SRGB           = 138,
            VK_FORMAT_BC4_UNORM          = 139,
            VK_FORMAT_BC4_SNORM          = 140,
            VK_FORMAT_BC5_UNORM          = 141,
            VK_FORMAT_BC5_SNORM          = 142,
            VK_FORMAT_BC6H_UFLOAT        = 143,
            VK_FORMAT_BC6H_SFLOAT        = 144,
            VK_FORMAT_BC7_UNORM          = 145,
            VK_FORMAT_BC7_SRGB           = 146,
        };

        enum SupercompressionScheme : uint32_t
        {
            KTX_SS_NONE     = 0,
            KTX_SS_BASIS_LZ = 1,
            KTX_SS_ZSTD     = 2,
            KTX_SS_ZLIB     = 3,
        };

#pragma pack(push, 1)
        struct KTX2Header
        {
            uint8_t  identifier[12];
            uint32_t vkFormat;
            uint32_t typeSize;
            uint32_t pixelWidth;
            uint32_t pixelHeight;
            uint32_t pixelDepth;
            uint32_t layerCount;
            uint32_t faceCount;
            uint32_t levelCount;
            uint32_t supercompressionScheme;
            // Index
            uint32_t dfdByteOffset;
            uint32_t dfdByteLength;
            uint32_t kvdByteOffset;
            uint32_t kvdByteLength;
            uint64_t sgdByteOffset;
            uint64_t sgdByteLength;
        };

        struct KTX2LevelIndex
        {
            uint64_t byteOffset;
            uint64_t byteLength;
            uint64_t uncompressedByteLength;
        };
#pragma pack(pop)

        static_assert(sizeof(KTX2Header) == 80, "KTX2Header must be tightly packed (level index starts at byte 80)");
        static_assert(sizeof(KTX2LevelIndex) == 24, "KTX2LevelIndex must be tightly packed");

        // bytesPerBlock for a supported BCn vkFormat, or 0 if unsupported.
        uint32_t BCnBytesPerBlock(uint32_t vkFormat)
        {
            switch (vkFormat)
            {
            case VK_FORMAT_BC1_RGB_UNORM:
            case VK_FORMAT_BC1_RGB_SRGB:
            case VK_FORMAT_BC1_RGBA_UNORM:
            case VK_FORMAT_BC1_RGBA_SRGB:
            case VK_FORMAT_BC4_UNORM:
            case VK_FORMAT_BC4_SNORM:
                return 8;
            case VK_FORMAT_BC2_UNORM:
            case VK_FORMAT_BC2_SRGB:
            case VK_FORMAT_BC3_UNORM:
            case VK_FORMAT_BC3_SRGB:
            case VK_FORMAT_BC5_UNORM:
            case VK_FORMAT_BC5_SNORM:
            case VK_FORMAT_BC6H_UFLOAT:
            case VK_FORMAT_BC6H_SFLOAT:
            case VK_FORMAT_BC7_UNORM:
            case VK_FORMAT_BC7_SRGB:
                return 16;
            default:
                return 0;
            }
        }

        // Map a KTX2 vkFormat to the nvrhi format; UNKNOWN for anything unsupported.
        // vkFormat always states the transfer function, so FromFile needs no fallback.
        nvrhi::Format VkFormatToNvrhi(uint32_t vkFormat)
        {
            switch (vkFormat)
            {
            case VK_FORMAT_BC1_RGB_UNORM:
            case VK_FORMAT_BC1_RGBA_UNORM:
                return nvrhi::Format::BC1_UNORM;
            case VK_FORMAT_BC1_RGB_SRGB:
            case VK_FORMAT_BC1_RGBA_SRGB:
                return nvrhi::Format::BC1_UNORM_SRGB;
            case VK_FORMAT_BC2_UNORM:
                return nvrhi::Format::BC2_UNORM;
            case VK_FORMAT_BC2_SRGB:
                return nvrhi::Format::BC2_UNORM_SRGB;
            case VK_FORMAT_BC3_UNORM:
                return nvrhi::Format::BC3_UNORM;
            case VK_FORMAT_BC3_SRGB:
                return nvrhi::Format::BC3_UNORM_SRGB;
            case VK_FORMAT_BC4_UNORM:
                return nvrhi::Format::BC4_UNORM;
            case VK_FORMAT_BC4_SNORM:
                return nvrhi::Format::BC4_SNORM;
            case VK_FORMAT_BC5_UNORM:
                return nvrhi::Format::BC5_UNORM;
            case VK_FORMAT_BC5_SNORM:
                return nvrhi::Format::BC5_SNORM;
            case VK_FORMAT_BC6H_UFLOAT:
                return nvrhi::Format::BC6H_UFLOAT;
            case VK_FORMAT_BC6H_SFLOAT:
                return nvrhi::Format::BC6H_SFLOAT;
            case VK_FORMAT_BC7_UNORM:
                return nvrhi::Format::BC7_UNORM;
            case VK_FORMAT_BC7_SRGB:
                return nvrhi::Format::BC7_UNORM_SRGB;
            default:
                return nvrhi::Format::UNKNOWN;
            }
        }

        bool SwizzleCharToComponent(char c, nvrhi::ComponentSwizzle& out)
        {
            switch (c)
            {
            case 'r': out = nvrhi::ComponentSwizzle::R;    return true;
            case 'g': out = nvrhi::ComponentSwizzle::G;    return true;
            case 'b': out = nvrhi::ComponentSwizzle::B;    return true;
            case 'a': out = nvrhi::ComponentSwizzle::A;    return true;
            case '0': out = nvrhi::ComponentSwizzle::Zero; return true;
            case '1': out = nvrhi::ComponentSwizzle::One;  return true;
            default:  return false;
            }
        }

        // Find KTXswizzle in the key/value data (KTX2 spec §3.11, §5.4) and turn it into
        // a component mapping.  `mappedSize` bounds the readable region: the KVD block can
        // sit past the end of a partially mapped file, which just means no mapping.
        std::optional<nvrhi::ComponentMapping> ParseKVDSwizzle(
            const char* fileBytes, size_t mappedSize, uint32_t kvdByteOffset, uint32_t kvdByteLength)
        {
            // Subtract rather than add, so a crafted offset cannot wrap past the check.
            if (kvdByteLength == 0 || kvdByteOffset > mappedSize ||
                kvdByteLength > mappedSize - kvdByteOffset)
                return std::nullopt;

            static const char c_Key[] = "KTXswizzle";
            constexpr size_t c_KeyLen = sizeof(c_Key) - 1;

            size_t p = kvdByteOffset;
            const size_t end = size_t(kvdByteOffset) + kvdByteLength;
            while (p + 4 <= end)
            {
                uint32_t kvLength = 0;
                std::memcpy(&kvLength, fileBytes + p, sizeof(kvLength));
                p += 4;
                if (kvLength > end - p)
                    return std::nullopt; // malformed: the entry runs past the block

                const char* kv = fileBytes + p;
                size_t keyLen = 0;
                while (keyLen < kvLength && kv[keyLen] != '\0')
                    ++keyLen;

                // Value is 4 characters plus its own NUL.
                if (keyLen == c_KeyLen && keyLen + 1 + 5 <= kvLength &&
                    std::memcmp(kv, c_Key, c_KeyLen) == 0)
                {
                    const char* value = kv + keyLen + 1;
                    nvrhi::ComponentMapping mapping;
                    nvrhi::ComponentSwizzle* dst[4] = { &mapping.r, &mapping.g, &mapping.b, &mapping.a };
                    for (int i = 0; i < 4; ++i)
                        if (!SwizzleCharToComponent(value[i], *dst[i]))
                            return std::nullopt;
                    return mapping;
                }

                p += kvLength;
                p += (4 - (kvLength & 3)) & 3; // valuePadding to the next 4-byte boundary
                if (p > end)
                    break;
            }
            return std::nullopt;
        }

        // Sentinel for ParseKTX2's fileSize: the body is not mapped, so skip its
        // bounds check.
        constexpr uint64_t c_UnknownFileSize = ~0ull;

        // Parsed, validated KTX2 layout shared by ReadKTX2Header and the loader.
        struct ParsedKTX2
        {
            uint32_t vkFormat = 0;
            uint32_t scheme   = 0;
            uint32_t width    = 1;
            uint32_t height   = 1;
            uint32_t levelCount    = 0;
            uint32_t bytesPerBlock = 0;
            struct Level
            {
                uint32_t w = 1, h = 1;
                uint64_t fileOffset     = 0;
                uint64_t compressedSize = 0;
                size_t   rowPitch   = 0;
                size_t   depthPitch = 0;
                size_t   sizeBytes  = 0; // block-packed, == uncompressedByteLength
            };
            std::vector<Level> levels;
            std::optional<nvrhi::ComponentMapping> componentMapping; // KTXswizzle, if present
        };

        // Validate + parse the header and level index of a 2D BCn KTX2 (no pixel
        // data touched beyond the level index).  Logs and returns false for
        // malformed/unsupported inputs.  `fileSize` bounds-checks the level data
        // ranges; pass c_UnknownFileSize when only the header region is mapped.
        bool ParseKTX2(const char* fileBytes, size_t headerRegionSize, uint64_t fileSize,
                       const char* debugName, ParsedKTX2& out)
        {
            const char* name = debugName ? debugName : "<ktx2>";

            if (headerRegionSize < sizeof(KTX2Header))
                return false;

            KTX2Header header;
            std::memcpy(&header, fileBytes, sizeof(header));

            if (std::memcmp(header.identifier, c_KTX2Identifier, sizeof(c_KTX2Identifier)) != 0)
                return false;

            if (header.vkFormat == VK_FORMAT_UNDEFINED)
            {
                log::warning("KTX2 '%s' has vkFormat=UNDEFINED (Basis/UASTC) which requires a "
                             "transcoder; not supported", name);
                return false;
            }
            if (header.pixelDepth > 1 || header.faceCount > 1 || header.layerCount > 1)
            {
                log::warning("KTX2 '%s' is array/cube/3D (depth=%u face=%u layer=%u); only 2D "
                             "single-layer textures are supported",
                             name, header.pixelDepth, header.faceCount, header.layerCount);
                return false;
            }
            if (header.supercompressionScheme != KTX_SS_NONE &&
                header.supercompressionScheme != KTX_SS_ZSTD)
            {
                log::warning("KTX2 '%s' uses unsupported supercompressionScheme=%u (only none "
                             "and Zstandard are supported)", name, header.supercompressionScheme);
                return false;
            }

            const uint32_t bytesPerBlock = BCnBytesPerBlock(header.vkFormat);
            if (bytesPerBlock == 0)
            {
                log::warning("KTX2 '%s' has unsupported vkFormat=%u", name, header.vkFormat);
                return false;
            }

            const uint32_t numLevels = std::max(1u, header.levelCount);
            if (headerRegionSize < sizeof(KTX2Header) + sizeof(KTX2LevelIndex) * size_t(numLevels))
                return false;

            std::vector<KTX2LevelIndex> levelIndex(numLevels);
            std::memcpy(levelIndex.data(), fileBytes + sizeof(KTX2Header),
                        sizeof(KTX2LevelIndex) * size_t(numLevels));

            const uint32_t width  = std::max(1u, header.pixelWidth);
            const uint32_t height = std::max(1u, header.pixelHeight);

            out.vkFormat      = header.vkFormat;
            out.scheme        = header.supercompressionScheme;
            out.width         = width;
            out.height        = height;
            out.levelCount    = numLevels;
            out.bytesPerBlock = bytesPerBlock;
            out.componentMapping = ParseKVDSwizzle(fileBytes, headerRegionSize,
                                                   header.kvdByteOffset, header.kvdByteLength);
            out.levels.resize(numLevels);

            for (uint32_t mip = 0; mip < numLevels; ++mip)
            {
                const uint32_t mipW = std::max(1u, width  >> mip);
                const uint32_t mipH = std::max(1u, height >> mip);
                const size_t blocksWide = std::max<size_t>(1, (size_t(mipW) + 3) / 4);
                const size_t blocksHigh = std::max<size_t>(1, (size_t(mipH) + 3) / 4);

                ParsedKTX2::Level& L = out.levels[mip];
                L.w = mipW;
                L.h = mipH;
                L.rowPitch   = blocksWide * bytesPerBlock;
                L.depthPitch = L.rowPitch * blocksHigh;
                L.sizeBytes  = L.depthPitch;
                L.fileOffset     = levelIndex[mip].byteOffset;
                L.compressedSize = levelIndex[mip].byteLength;

                // The level index records the uncompressed size of the whole level;
                // for a 2D single-layer/face BC image that must equal the block size.
                if (levelIndex[mip].uncompressedByteLength != L.sizeBytes)
                {
                    log::warning("KTX2 '%s' mip %u uncompressedByteLength=%llu disagrees with the "
                                 "computed block-packed size %zu", name, mip,
                                 (unsigned long long)levelIndex[mip].uncompressedByteLength, L.sizeBytes);
                    return false;
                }
                // Subtract rather than add, so a crafted offset cannot wrap past the check.
                if (fileSize != c_UnknownFileSize &&
                    (L.fileOffset > fileSize || L.compressedSize > fileSize - L.fileOffset))
                {
                    log::warning("KTX2 '%s' mip %u data range exceeds file size", name, mip);
                    return false;
                }
            }

            return true;
        }
    } // anonymous namespace

    bool ReadKTX2Header(const void* data, size_t size, const char* debugName, KTX2HeaderInfo& out)
    {
        out = KTX2HeaderInfo{};

        ParsedKTX2 parsed;
        // Only the header region is required; skip the body bounds check.
        if (!ParseKTX2(static_cast<const char*>(data), size, c_UnknownFileSize, debugName, parsed))
            return false;

        out.supported    = true;
        out.vkFormat      = parsed.vkFormat;
        out.format        = VkFormatToNvrhi(parsed.vkFormat);
        out.width         = parsed.width;
        out.height        = parsed.height;
        out.levelCount    = parsed.levelCount;
        out.bytesPerBlock = parsed.bytesPerBlock;
        out.supercompressionScheme = parsed.scheme;
        out.componentMapping = parsed.componentMapping;
        out.levels.resize(parsed.levelCount);
        for (uint32_t mip = 0; mip < parsed.levelCount; ++mip)
        {
            out.levels[mip].width          = parsed.levels[mip].w;
            out.levels[mip].height         = parsed.levels[mip].h;
            out.levels[mip].fileOffset     = parsed.levels[mip].fileOffset;
            out.levels[mip].compressedSize = parsed.levels[mip].compressedSize;
            out.levels[mip].gpuBytes       = parsed.levels[mip].sizeBytes;
        }
        return true;
    }

    bool LoadKTX2TextureFromMemory(TextureData& textureInfo)
    {
        const char* fileBytes = static_cast<const char*>(textureInfo.data->GetDataPtr());
        const size_t fileSize = textureInfo.data->GetSize();

        ParsedKTX2 parsed;
        if (!ParseKTX2(fileBytes, fileSize, fileSize, textureInfo.path.c_str(), parsed))
            return false;

        const nvrhi::Format format =
            ApplySRGBOverride(VkFormatToNvrhi(parsed.vkFormat), textureInfo.loadOptions.sRGBMode);
        if (format == nvrhi::Format::UNKNOWN)
            return false; // ParseKTX2 already validated, but keep the contract explicit

        // Clamp the requested base mip into range; never drop everything.
        const uint32_t baseMip = std::min(textureInfo.loadOptions.baseMip, parsed.levelCount - 1u);
        const uint32_t numKept = parsed.levelCount - baseMip;

        // Allocate the decompressed destination for the kept levels [baseMip, end).
        // A checked total also bounds the dstCursor and dataOffset walks below, which
        // step through the same sizes.
        size_t totalBytes = 0;
        for (uint32_t mip = baseMip; mip < parsed.levelCount; ++mip)
        {
            const size_t levelBytes = parsed.levels[mip].sizeBytes;
            if (levelBytes > std::numeric_limits<size_t>::max() - totalBytes)
            {
                log::warning("KTX2 '%s' decompressed size overflows", textureInfo.path.c_str());
                return false;
            }
            totalBytes += levelBytes;
        }

        AutoPtr<IDataBlob> dstBlob;
        if (FFAILED(CreateBlob(totalBytes, &dstBlob)))
            return false;
        char* dst = static_cast<char*>(dstBlob->GetDataPtr());

        size_t dstCursor = 0;
        for (uint32_t mip = baseMip; mip < parsed.levelCount; ++mip)
        {
            const ParsedKTX2::Level& L = parsed.levels[mip];
            const char* src = fileBytes + L.fileOffset;
            char*       out = dst + dstCursor;

            if (parsed.scheme == KTX_SS_ZSTD)
            {
                const size_t got = ZSTD_decompress(out, L.sizeBytes, src, size_t(L.compressedSize));
                if (ZSTD_isError(got) || got != L.sizeBytes)
                {
                    log::warning("KTX2 '%s' mip %u Zstandard inflation failed: %s",
                                 textureInfo.path.c_str(), mip,
                                 ZSTD_isError(got) ? ZSTD_getErrorName(got) : "size mismatch");
                    return false;
                }
            }
            else // KTX_SS_NONE
            {
                if (L.compressedSize != L.sizeBytes)
                    return false;
                std::memcpy(out, src, L.sizeBytes);
            }
            dstCursor += L.sizeBytes;
        }

        // Replace the raw .ktx2 blob with the decompressed kept-mip chain.  The GPU
        // texture is described at the base (kept) level's dimensions.
        textureInfo.data      = dstBlob;
        textureInfo.format    = format;
        textureInfo.width     = parsed.levels[baseMip].w;
        textureInfo.height    = parsed.levels[baseMip].h;
        textureInfo.depth     = 1;
        textureInfo.arraySize = 1;
        textureInfo.mipLevels = numKept;
        textureInfo.dimension = nvrhi::TextureDimension::Texture2D;
        textureInfo.originalBitsPerPixel = (parsed.bytesPerBlock * 8) / 16; // BCn bits/texel
        textureInfo.fileComponentMapping = parsed.componentMapping;

        textureInfo.dataLayout.resize(1);
        std::vector<TextureSubresourceData>& slice = textureInfo.dataLayout[0];
        slice.resize(numKept);
        size_t off = 0;
        for (uint32_t i = 0; i < numKept; ++i)
        {
            const ParsedKTX2::Level& L = parsed.levels[baseMip + i];
            TextureSubresourceData& sub = slice[i];
            sub.dataOffset = ptrdiff_t(off);
            sub.dataSize   = L.sizeBytes;
            sub.rowPitch   = L.rowPitch;
            sub.depthPitch = L.depthPitch;
            off += L.sizeBytes;
        }

        return true;
    }
}

#endif // DONUT_WITH_KTX
