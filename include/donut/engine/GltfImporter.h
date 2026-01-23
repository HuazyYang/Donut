/*
* Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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
#include <filesystem>

namespace donut::vfs
{
    class IBlob;
    class IFileSystem;
}

namespace donut::engine
{
    struct SceneImportResult;
    struct SceneLoadingStats;
    class TextureCache;
    class ThreadPool;
    class SceneGraphNode;
    class SceneTypeFactory;
    class SceneGraphAnimation;
}

namespace donut::engine
{
    class GltfImporter: public ObjectImpl<IObject>
    {   
    protected:
        AutoPtr<vfs::IFileSystem> m_fs;
        AutoPtr<SceneTypeFactory> m_SceneTypeFactory;
        
    public:
        explicit GltfImporter(vfs::IFileSystem* fs, SceneTypeFactory* sceneTypeFactory);

        bool Load(
            const std::filesystem::path& fileName,
            TextureCache& textureCache,
            SceneLoadingStats& stats,
            ThreadPool* threadPool,
            SceneImportResult& result) const;
    };
}
