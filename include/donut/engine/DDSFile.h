/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
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
#include <donut/core/object/Foundation.h>
#include <donut/core/object/AutoPtr.h>
#include <donut/engine/SceneTypes.h>
#include <nvrhi/nvrhi.h>
#include <vector>

namespace donut::vfs
{
    class IBlob;
}

namespace donut::engine
{
    struct TextureData;

    // Per-mip-level metadata from a DDS file (no pixel data). gpuBytes is the
    // block-packed footprint of one array slice of that level -- the quantity a
    // memory budget is computed against.
    struct DDSLevelInfo
    {
        uint32_t width  = 0;
        uint32_t height = 0;
        uint64_t gpuBytes = 0;
    };

    // Header-only description of a DDS texture, the counterpart of KTX2HeaderInfo.
    // DDS stores no level index, so the sizes are computed from the format and the
    // mip dimensions rather than read out of the file.
    struct DDSHeaderInfo
    {
        bool          supported = false;
        nvrhi::Format format    = nvrhi::Format::UNKNOWN;
        uint32_t      width     = 0;   // mip-0 dimensions
        uint32_t      height    = 0;
        uint32_t      depth     = 1;
        uint32_t      arraySize = 1;
        uint32_t      levelCount = 0;
        nvrhi::TextureDimension dimension = nvrhi::TextureDimension::Unknown;
        std::vector<DDSLevelInfo> levels; // levels[0] = mip 0 (largest)
    };

    // Read only the DDS header(s) -- no pixel data -- from a buffer holding at
    // least the magic plus DDS_HEADER (and the DX10 header when present), i.e. 148
    // bytes covers every case. Returns false (out.supported = false) for malformed
    // or unsupported inputs.
    bool ReadDDSHeader(const void* data, size_t size, const char* debugName, DDSHeaderInfo& out);

    // Initialized the TextureInfo from the 'data' array, which must be populated with DDS data.
    // loadOptions.baseMip drops that many of the highest-resolution mips (clamped to
    // [0, mipMapCount-1]); the texture is then described at the base level's dimensions.
    // Unlike KTX2, the skipped levels stay in the blob -- only GPU memory is saved.
    bool LoadDDSTextureFromMemory(TextureData& textureInfo);

    // Creates a texture based on DDS data in memory
    nvrhi::TextureHandle CreateDDSTextureFromMemory(nvrhi::IDevice* device, nvrhi::ICommandList* commandList, IDataBlob* data, const char* debugName = nullptr, const TextureLoadOptions& loadOptions = TextureLoadOptions());

    // Back-compat overload for callers written against the older `bool forceSRGB`.
    [[deprecated("Pass a TextureLoadOptions instead")]]
    inline nvrhi::TextureHandle CreateDDSTextureFromMemory(nvrhi::IDevice* device, nvrhi::ICommandList* commandList, IDataBlob* data, const char* debugName, bool forceSRGB)
    {
        return CreateDDSTextureFromMemory(device, commandList, data, debugName, TextureLoadOptions{ SRGBModeFromBool(forceSRGB) });
    }

    AutoPtr<IDataBlob> SaveStagingTextureAsDDS(nvrhi::IDevice* device, nvrhi::IStagingTexture* stagingTexture);
}