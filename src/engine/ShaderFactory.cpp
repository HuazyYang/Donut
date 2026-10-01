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

#include <donut/engine/ShaderFactory.h>
#include <donut/core/vfs/VFS.h>
#include <donut/core/log.h>
#include <donut/core/string_utils.h>
#include <ShaderToolBlob.h>
#if DONUT_WITH_AFTERMATH
#include <donut/app/AftermathCrashDump.h>
#endif

using namespace std;
using namespace donut::vfs;
using namespace donut::engine;

// Builds the "permutation not found" message using the two-call sizing convention
// of ShaderToolBlobFormatNotFoundMessage (the required length excludes the NUL,
// the capacity passed with a buffer must include it).
static std::string FormatShaderNotFoundMessage(const void* blob, size_t blobSize,
    const ShaderToolBlobConstant* constants, size_t numConstants)
{
    std::string message = "Couldn't find the required shader permutation in the blob.";

    size_t length = 0;
    if (ShaderToolBlobFormatNotFoundMessage(blob, blobSize, constants, numConstants, nullptr, 0, &length) == 0)
    {
        message.resize(length + 1);
        if (ShaderToolBlobFormatNotFoundMessage(blob, blobSize, constants, numConstants, &message[0], length + 1, nullptr) == 0)
            message.resize(length);
        else
            message = "Couldn't find the required shader permutation in the blob.";
    }

    return message;
}

ShaderFactory::ShaderFactory(nvrhi::IDevice *rendererInterface,
	IFileSystem* fs,
	const std::filesystem::path& basePath)
	: m_Device(rendererInterface)
	, m_fs(fs)
	, m_basePath(basePath)
{
#if DONUT_WITH_AFTERMATH
    if (m_Device->isAftermathEnabled())
        m_Device->getAftermathCrashDumpHelper().registerShaderBinaryLookupCallback(this, std::bind(&ShaderFactory::FindShaderFromHash, this, std::placeholders::_1, std::placeholders::_2));
#endif
}

ShaderFactory::~ShaderFactory()
{
#if DONUT_WITH_AFTERMATH
    if (m_Device->isAftermathEnabled())
        m_Device->getAftermathCrashDumpHelper().unRegisterShaderBinaryLookupCallback(this);
#endif
}

void ShaderFactory::ClearCache()
{
	m_BytecodeCache.clear();
}

nvrhi::AutoPtr<nvrhi::IDataBlob> ShaderFactory::GetBytecode(const char* fileName, const char* entryName)
{
    if (!m_fs)
        return nullptr;
        
    if (!entryName)
        entryName = "main";

    string adjustedName = fileName;
    {
        size_t pos = adjustedName.find(".hlsl");
        if (pos != string::npos)
            adjustedName.erase(pos, 5);

        if (entryName && strcmp(entryName, "main"))
            adjustedName += "_" + string(entryName);
    }

    std::filesystem::path shaderFilePath = m_basePath / (adjustedName + ".bin");

    auto& data = m_BytecodeCache[shaderFilePath.generic_string()];

    if (data)
        return data;

    if (NVRHI_FAILED(m_fs->readFile(shaderFilePath, &data)))
    {
        log::error("Couldn't read the binary file for shader %s from %s", fileName, shaderFilePath.generic_string().c_str());
        return nullptr;
    }

    return data;
}

nvrhi::ShaderHandle ShaderFactory::CreateShader(const char* fileName, const char* entryName, const vector<ShaderMacro>* pDefines, const nvrhi::ShaderDesc& desc)
{
    auto byteCode = GetBytecode(fileName, entryName);

    if(!byteCode)
        return nullptr;

    nvrhi::ShaderDesc descCopy = desc;
    descCopy.entryName = entryName;
    if (descCopy.debugName.empty())
        descCopy.debugName = fileName;

    return CreateStaticShader(StaticShader{ byteCode->GetDataPtr(), byteCode->GetSize() }, pDefines, descCopy);
}

nvrhi::ShaderHandle ShaderFactory::CreateShader(const char* fileName, const char* entryName, const vector<ShaderMacro>* pDefines, nvrhi::ShaderType shaderType)
{
    return CreateShader(fileName, entryName, pDefines, nvrhi::ShaderDesc().setShaderType(shaderType));
}

nvrhi::ShaderLibraryHandle ShaderFactory::CreateShaderLibrary(const char* fileName, const std::vector<ShaderMacro>* pDefines)
{
    auto byteCode = GetBytecode(fileName, nullptr);

    if (!byteCode)
        return nullptr;

    return CreateStaticShaderLibrary(StaticShader{ byteCode->GetDataPtr(), byteCode->GetSize() }, pDefines);
}

nvrhi::ShaderHandle ShaderFactory::CreateStaticShader(StaticShader shader, const std::vector<ShaderMacro>* pDefines, const nvrhi::ShaderDesc& desc)
{
    if (!shader.pBytecode || !shader.size)
        return nullptr;

    vector<ShaderToolBlobConstant> constants;
    if (pDefines)
    {
        for (const ShaderMacro& define : *pDefines)
            constants.push_back(ShaderToolBlobConstant{ define.name.c_str(), define.definition.c_str() });
    }

    const void* permutationBytecode = nullptr;
    size_t permutationSize = 0;
    if (ShaderToolBlobFindPermutation(shader.pBytecode, shader.size, constants.data(), constants.size(), &permutationBytecode, &permutationSize) != 0)
    {
        const std::string message = FormatShaderNotFoundMessage(shader.pBytecode, shader.size, constants.data(), constants.size());
        log::error("%s", message.c_str());

        return nullptr;
    }

    return m_Device->createShader(desc, permutationBytecode, permutationSize);
}

nvrhi::ShaderHandle ShaderFactory::CreateStaticShader(StaticShader shader, const std::vector<ShaderMacro>* pDefines, nvrhi::ShaderType shaderType)
{
    return CreateStaticShader(shader, pDefines, nvrhi::ShaderDesc().setShaderType(shaderType));
}

nvrhi::ShaderHandle ShaderFactory::CreateStaticPlatformShader(StaticShader dxbc, StaticShader dxil, StaticShader spirv, const std::vector<ShaderMacro>* pDefines, const nvrhi::ShaderDesc& desc)
{
    StaticShader shader;
    switch(m_Device->getGraphicsAPI())
    {
        case nvrhi::GraphicsAPI::D3D11:
            shader = dxbc;
            break;
        case nvrhi::GraphicsAPI::D3D12:
            shader = dxil;
            break;
        case nvrhi::GraphicsAPI::VULKAN:
            shader = spirv;
            break;
    }

    return CreateStaticShader(shader, pDefines, desc);
}

nvrhi::ShaderHandle ShaderFactory::CreateStaticPlatformShader(StaticShader dxbc, StaticShader dxil, StaticShader spirv, const std::vector<ShaderMacro>* pDefines, nvrhi::ShaderType shaderType)
{
    return CreateStaticPlatformShader(dxbc, dxil, spirv, pDefines, nvrhi::ShaderDesc().setShaderType(shaderType));
}

nvrhi::ShaderLibraryHandle ShaderFactory::CreateStaticShaderLibrary(StaticShader shader, const std::vector<ShaderMacro>* pDefines)
{
    if (!shader.pBytecode || !shader.size)
        return nullptr;

    vector<ShaderToolBlobConstant> constants;
    if (pDefines)
    {
        for (const ShaderMacro& define : *pDefines)
            constants.push_back(ShaderToolBlobConstant{ define.name.c_str(), define.definition.c_str() });
    }

    const void* permutationBytecode = nullptr;
    size_t permutationSize = 0;
    if (ShaderToolBlobFindPermutation(shader.pBytecode, shader.size, constants.data(), constants.size(), &permutationBytecode, &permutationSize) != 0)
    {
        const std::string message = FormatShaderNotFoundMessage(shader.pBytecode, shader.size, constants.data(), constants.size());
        log::error("%s", message.c_str());

        return nullptr;
    }

    return m_Device->createShaderLibrary(permutationBytecode, permutationSize);
}

nvrhi::ShaderLibraryHandle ShaderFactory::CreateStaticPlatformShaderLibrary(StaticShader dxil, StaticShader spirv, const std::vector<ShaderMacro>* pDefines)
{
    StaticShader shader;
    switch(m_Device->getGraphicsAPI())
    {
        case nvrhi::GraphicsAPI::D3D12:
            shader = dxil;
            break;
        case nvrhi::GraphicsAPI::VULKAN:
            shader = spirv;
            break;
        default:
            break;
    }

    return CreateStaticShaderLibrary(shader, pDefines);
}

nvrhi::ShaderHandle ShaderFactory::CreateAutoShader(const char* fileName, const char* entryName, StaticShader dxbc, StaticShader dxil, StaticShader spirv, const std::vector<ShaderMacro>* pDefines, const nvrhi::ShaderDesc& desc)
{
    nvrhi::ShaderDesc descCopy = desc;
    descCopy.entryName = entryName;
    if (descCopy.debugName.empty())
        descCopy.debugName = fileName;

    nvrhi::ShaderHandle shader = CreateStaticPlatformShader(dxbc, dxil, spirv, pDefines, descCopy);
    if (shader)
        return shader;
        
    return CreateShader(fileName, entryName, pDefines, desc);
}

nvrhi::ShaderHandle ShaderFactory::CreateAutoShader(const char* fileName, const char* entryName, StaticShader dxbc, StaticShader dxil, StaticShader spirv, const std::vector<ShaderMacro>* pDefines, nvrhi::ShaderType shaderType)
{
    return CreateAutoShader(fileName, entryName, dxbc, dxil, spirv, pDefines, nvrhi::ShaderDesc().setShaderType(shaderType));
}

nvrhi::ShaderLibraryHandle ShaderFactory::CreateAutoShaderLibrary(const char* fileName, StaticShader dxil, StaticShader spirv, const std::vector<ShaderMacro>* pDefines)
{
    nvrhi::ShaderLibraryHandle shader = CreateStaticPlatformShaderLibrary(dxil, spirv, pDefines);
    if (shader)
        return shader;

    return CreateShaderLibrary(fileName, pDefines);
}

std::pair<const void*, size_t> donut::engine::ShaderFactory::FindShaderFromHash(uint64_t hash, std::function<uint64_t(std::pair<const void*, size_t>, nvrhi::GraphicsAPI)> hashGenerator)
{
    for (auto& entry : m_BytecodeCache)
    {
        const void* shaderBytes = entry.second->GetDataPtr();
        size_t shaderSize = entry.second->GetSize();

        // the bytecode could contain multiple permutations
        std::vector<std::string> permutations;
        auto enumerateCallback = [](void* context, const char* key, size_t keyLength, const void* /*binary*/, size_t /*binarySize*/) -> int
        {
            auto& keys = *static_cast<std::vector<std::string>*>(context);
            if (keyLength == 0)
                keys.push_back("<default>");
            else
                keys.push_back(std::string(key, keyLength));
            return 0;
        };
        ShaderToolBlobEnumeratePermutations(shaderBytes, shaderSize, enumerateCallback, &permutations);

        if (permutations.size() > 1)
        {
            std::vector<ShaderToolBlobConstant> permutationConstants;
            std::unordered_map<std::string, std::string> permutationDefines;
            for (auto permutation : permutations)
            {
                permutationConstants.clear();
                permutationDefines.clear();
                // split the string by spaces to get individual defines
                std::vector<std::string> permutationStrings = donut::string_utils::split(permutation, " ");
                for (auto& s : permutationStrings)
                {
                    std::vector<std::string> keyValue = donut::string_utils::split(s, "=");
                    // Well-formed blob keys are always NAME=VALUE pairs; skip anything
                    // else (e.g. a "<default>" entry from a corrupted blob) instead of
                    // indexing out of bounds.
                    if (keyValue.size() == 2)
                        permutationDefines[keyValue[0]] = keyValue[1];
                }
                // now that we have processed all defines in this permutation, can create the shader constants
                for (const auto& [key, value] : permutationDefines)
                {
                    permutationConstants.push_back(ShaderToolBlobConstant{ key.c_str(), value.c_str() });
                }
                const void* permutationBytecode = nullptr;
                size_t permutationSize = 0;
                if (ShaderToolBlobFindPermutation(shaderBytes, shaderSize, permutationConstants.data(),
                    permutationConstants.size(), &permutationBytecode, &permutationSize) == 0)
                {
                    uint64_t entryHash = hashGenerator(std::make_pair(permutationBytecode, permutationSize), m_Device->getGraphicsAPI());
                    if (entryHash == hash)
                    {
                        return std::make_pair(permutationBytecode, permutationSize);
                    }
                }
            }
        }
        else
        {
            uint64_t entryHash = hashGenerator(std::make_pair(shaderBytes, shaderSize), m_Device->getGraphicsAPI());
            if (entryHash == hash)
            {
                return std::make_pair(shaderBytes, shaderSize);
            }
        }
    }
    return std::make_pair(nullptr, 0);
}
