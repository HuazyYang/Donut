#pragma once
#include <nvrhi/core/foundation.h>
#include <nvrhi/core/autoptr.h>
#include <donut/engine/SceneImporter.h>

namespace donut::engine {

class SceneTypeFactory;

NVRHI_SCLSID(GltfImporter, "b2f04e32-4515-4367-8a7c-60a20211129b")
struct GltfImporter : nvrhi::ObjectImpl<ISceneImporter> {
    NVRHI_DECLARE_UUID_TRAITS(GltfImporter)
    NVRHI_BEGIN_INTERFACE_TABLE_INLINE(GltfImporter)
      NVRHI_IMPLEMENTS_INTERFACE(ISceneImporter)
      NVRHI_IMPLEMENTS_CLASS(GltfImporter)
    NVRHI_END_INTERFACE_TABLE()

 protected:
    nvrhi::AutoPtr<vfs::IFileSystem> m_fs;
    nvrhi::AutoPtr<SceneTypeFactory> m_SceneTypeFactory;

 public:
    explicit GltfImporter(vfs::IFileSystem* fs, SceneTypeFactory* sceneTypeFactory);
    ~GltfImporter();

    nvrhi::FRESULT Load(const std::filesystem::path& fileName, TextureCache& textureCache, SceneLoadingStats& stats,
              ThreadPool* threadPool, SceneImportResult& result) override;
};

#if 0

NVRHI_SCLSID(AssimpSceneImporter, "5c7d50ef-1a06-418f-8156-95421d70fc4c")
struct AssimpSceneImporter : nvrhi::ObjectImpl<ISceneImporter> {
    NVRHI_DECLARE_UUID_TRAITS(AssimpSceneImporter)
    NVRHI_BEGIN_INTERFACE_TABLE_INLINE(AssimpSceneImporter)
      NVRHI_IMPLEMENTS_INTERFACE(ISceneImporter)
      NVRHI_IMPLEMENTS_CLASS(AssimpSceneImporter)
    NVRHI_END_INTERFACE_TABLE()
 protected:
    nvrhi::AutoPtr<vfs::IFileSystem> m_fs;
    nvrhi::AutoPtr<SceneTypeFactory> m_SceneTypeFactory;

 public:
    explicit AssimpSceneImporter(vfs::IFileSystem* fs, SceneTypeFactory* sceneTypeFactory);
    ~AssimpSceneImporter();

    nvrhi::FRESULT Load(const std::filesystem::path& fileName, TextureCache& textureCache, SceneLoadingStats& stats,
                 ThreadPool* threadPool, SceneImportResult& result) override;
};

#endif

}  // namespace donut::engine