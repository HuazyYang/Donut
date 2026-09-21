#pragma once
#include <donut/core/object/Types.h>
#include <filesystem>

namespace donut::vfs {
class IBlob;
class IFileSystem;
}  // namespace donut::vfs

namespace donut::engine {
struct SceneImportResult;
struct SceneLoadingStats;
class TextureCache;
class ThreadPool;
class SceneGraphNode;
class SceneTypeFactory;
class SceneGraphAnimation;
}  // namespace donut::engine

namespace donut::engine {

DONUT_IID(ISceneImporter, "b3fc47ab-f053-404e-a71a-e0a80291f9b7")
struct ISceneImporter : IObject {
    DONUT_DECLARE_UUID_TRAITS(ISceneImporter)

    virtual FRESULT Load(const std::filesystem::path& fileName, TextureCache& textureCache,
                         SceneLoadingStats& stats, ThreadPool* threadPool, SceneImportResult& result) = 0;
};

}  // namespace donut::engine
