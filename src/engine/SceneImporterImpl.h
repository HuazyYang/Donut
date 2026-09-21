#pragma once
#include <donut/core/object/Foundation.h>
#include <donut/core/object/AutoPtr.h>
#include <donut/engine/SceneImporter.h>

namespace donut::engine {

class SceneTypeFactory;

DONUT_SCLSID(GltfImporter, "b2f04e32-4515-4367-8a7c-60a20211129b")
struct GltfImporter : ObjectImpl<ISceneImporter> {
    DONUT_DECLARE_UUID_TRAITS(GltfImporter)
    DONUT_BEGIN_INTERFACE_TABLE_INLINE(GltfImporter)
      DONUT_IMPLEMENTS_INTERFACE(GltfImporter)
      DONUT_IMPLEMENTS_INTERFACE(ISceneImporter)
      DONUT_IMPLEMENTS_INTERFACE(IObject)
    DONUT_END_INTERFACE_TABLE()

 protected:
    AutoPtr<vfs::IFileSystem> m_fs;
    AutoPtr<SceneTypeFactory> m_SceneTypeFactory;

 public:
    explicit GltfImporter(vfs::IFileSystem* fs, SceneTypeFactory* sceneTypeFactory);
    ~GltfImporter();

    FRESULT Load(const std::filesystem::path& fileName, TextureCache& textureCache, SceneLoadingStats& stats,
              ThreadPool* threadPool, SceneImportResult& result) override;
};

#if 0

DONUT_SCLSID(AssimpSceneImporter, "5c7d50ef-1a06-418f-8156-95421d70fc4c")
struct AssimpSceneImporter : ObjectImpl<ISceneImporter> {
    DONUT_DECLARE_UUID_TRAITS(AssimpSceneImporter)
    DONUT_BEGIN_INTERFACE_TABLE_INLINE(AssimpSceneImporter)
      DONUT_IMPLEMENTS_INTERFACE(AssimpSceneImporter)
      DONUT_IMPLEMENTS_INTERFACE(ISceneImporter)
      DONUT_IMPLEMENTS_INTERFACE(IObject)
    DONUT_END_INTERFACE_TABLE()
 protected:
    AutoPtr<vfs::IFileSystem> m_fs;
    AutoPtr<SceneTypeFactory> m_SceneTypeFactory;

 public:
    explicit AssimpSceneImporter(vfs::IFileSystem* fs, SceneTypeFactory* sceneTypeFactory);
    ~AssimpSceneImporter();

    FRESULT Load(const std::filesystem::path& fileName, TextureCache& textureCache, SceneLoadingStats& stats,
                 ThreadPool* threadPool, SceneImportResult& result) override;
};

#endif

}  // namespace donut::engine