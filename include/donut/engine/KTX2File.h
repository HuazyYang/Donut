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

#pragma once

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <optional>
#include <vector>

// The definitions live in KTX2File.cpp, which compiles to nothing when the
// option is off, so do not declare an API that would not link.
#if DONUT_WITH_KTX

namespace donut::engine
{
    struct TextureData;

    // Per-mip-level metadata from a KTX2 file (no pixel data). gpuBytes is the
    // block-packed (uncompressed) size that the level occupies on the GPU — the
    // quantity a memory budget is computed against.
    struct KTX2LevelInfo
    {
        uint32_t width  = 0;
        uint32_t height = 0;
        uint64_t fileOffset     = 0; // byteOffset of this level's (super)compressed data
        uint64_t compressedSize = 0; // byteLength in the file
        uint64_t gpuBytes       = 0; // uncompressedByteLength == block-packed footprint
    };

    // Header-only description of a KTX2 texture: everything needed to compute a
    // memory budget and pick a base mip without touching pixel data. Populated by
    // ReadKTX2Header from just the 80-byte header + level index.
    struct KTX2HeaderInfo
    {
        bool     supported = false; // 2D BCn, none/Zstandard supercompression
        uint32_t vkFormat  = 0;     // the raw header field
        nvrhi::Format format = nvrhi::Format::UNKNOWN; // vkFormat mapped, as DDSHeaderInfo reports it
        uint32_t width     = 0;     // mip-0 dimensions
        uint32_t height    = 0;
        uint32_t levelCount = 0;
        uint32_t bytesPerBlock = 0;
        uint32_t supercompressionScheme = 0;
        // The file's KTXswizzle metadata, if it carries one.
        std::optional<nvrhi::ComponentMapping> componentMapping;
        std::vector<KTX2LevelInfo> levels; // levels[0] = mip 0 (largest)
    };

    // Read only the KTX2 header + level index (no pixel data) from a buffer that
    // contains at least the first 80 + 24*levelCount bytes of the file (e.g. an
    // mmap of the whole file, or a small head read). Returns false (out.supported
    // = false) for malformed or unsupported inputs.
    bool ReadKTX2Header(const void* data, size_t size, const char* debugName, KTX2HeaderInfo& out);

    // Decode a KTX2 texture from textureInfo.data (the raw .ktx2 file blob).
    //
    // Supports the concrete block-compressed (BCn) vkFormats with the `none` and
    // `Zstandard` supercompression schemes.  Mip levels [baseMip, levelCount) are
    // inflated into a new contiguous blob that replaces textureInfo.data; the GPU
    // texture is described at the base level's dimensions (width>>baseMip etc.)
    // with (levelCount - baseMip) mips.  textureInfo.dataLayout is filled so
    // TextureCache::FinalizeTexture's per-mip writeTexture loop uploads it
    // unchanged (same contract as LoadDDSTextureFromMemory).
    //
    // loadOptions.baseMip lets a memory-budget pass drop the highest-resolution
    // mips; it is clamped to [0, levelCount-1].  sRGB-ness follows textureInfo.sRGBMode.
    //
    // Returns false (and leaves textureInfo.data null) on any unsupported or
    // malformed input; the caller logs.
    bool LoadKTX2TextureFromMemory(TextureData& textureInfo);
}

#endif // DONUT_WITH_KTX
