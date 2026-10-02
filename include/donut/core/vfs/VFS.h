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

#include <nvrhi/core/Foundation.h>
#include <nvrhi/core/DataBlob.h>
#include <string>
#include <filesystem>
#include <functional>
#include <vector>

/* 
Donut Virtual File System (VFS) main classes.

The VFS provides read and sometimes write access to entire files stored in a
real file system, mounted into a virtual tree, stored in archives or resources.
*/

namespace donut::vfs
{
    namespace status
    {
        constexpr int OK = 0;
        constexpr int Failed = -1;
        constexpr int PathNotFound = -2;
        constexpr int NotImplemented = -3;
    }

    typedef const std::function<void(std::string_view)>& enumerate_callback_t;

    inline std::function<void(std::string_view)> enumerate_to_vector(std::vector<std::string>& v)
    {
        return [&v](std::string_view s) { v.push_back(std::string(s)); };
    }

    // Basic interface for the virtual file system.
    class IFileSystem: public nvrhi::ObjectImpl<nvrhi::IObject>
    {
    public:
        NVRHI_BEGIN_INTERFACE_TABLE_INLINE(IFileSystem)
        NVRHI_IMPLEMENTS_INTERFACE(nvrhi::IObject)
        NVRHI_END_INTERFACE_TABLE()

        virtual ~IFileSystem() = default;

        // Test if a folder exists.
        virtual bool folderExists(const std::filesystem::path& name) = 0;

        // Test if a file exists.
        virtual bool fileExists(const std::filesystem::path& name) = 0;

        // Read the entire file.
        // Returns nullptr if the file cannot be read.
        virtual nvrhi::FRESULT readFile(const std::filesystem::path& name, nvrhi::IDataBlob** ppBlob) = 0;

        // Write the entire file.
        // Returns false if the file cannot be written.
        virtual bool writeFile(const std::filesystem::path& name, const void* data, size_t size) = 0;

        // Search for files with any of the provided 'extensions' in 'path'.
        // Extensions should not include any wildcard characters.
        // Returns the number of files found, or a negative number on errors - see donut::vfs::status.
        // The file names, relative to the 'path', are passed to 'callback' in no particular order.
        virtual int enumerateFiles(const std::filesystem::path& path, const std::vector<std::string>& extensions, enumerate_callback_t callback, bool allowDuplicates = false) = 0;

        // Search for directories in 'path'.
        // Returns the number of directories found, or a negative number on errors - see donut::vfs::status.
        // The directory names, relative to the 'path', are passed to 'callback' in no particular order.
        virtual int enumerateDirectories(const std::filesystem::path& path, enumerate_callback_t callback, bool allowDuplicates = false) = 0;
    };

    // An implementation of virtual file system that directly maps to the OS files.
    // Answers QueryInterface for its class ID, which callers use instead of dynamic_cast (nvrhi ADR 0006).
    NVRHI_CLASS_CLSID(NativeFileSystem, "ea646050-1e22-4749-b77c-54e11e099f7b")
    class NativeFileSystem : public IFileSystem
    {
    public:
        NVRHI_DECLARE_UUID_TRAITS(NativeFileSystem)
        NVRHI_BEGIN_INTERFACE_TABLE_INLINE(NativeFileSystem)
        NVRHI_IMPLEMENTS_INTERFACE(nvrhi::IObject)
        NVRHI_IMPLEMENTS_CLASS(NativeFileSystem)
        NVRHI_END_INTERFACE_TABLE()

		bool folderExists(const std::filesystem::path& name) override;
        bool fileExists(const std::filesystem::path& name) override;
        nvrhi::FRESULT readFile(const std::filesystem::path& name, nvrhi::IDataBlob** ppBlob) override;
        bool writeFile(const std::filesystem::path& name, const void* data, size_t size) override;
        int enumerateFiles(const std::filesystem::path& path, const std::vector<std::string>& extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        int enumerateDirectories(const std::filesystem::path& path, enumerate_callback_t callback, bool allowDuplicates = false) override;
    };

    // A layer that represents some path in the underlying file system as an entire FS.
    // Effectively, just prepends the provided base path to every file name
    // and passes the requests to the underlying FS.
    class RelativeFileSystem : public IFileSystem
    {
        NVRHI_INHERIT_INTERFACE_TABLE()
    private:
        IFileSystem *m_UnderlyingFS;
        std::filesystem::path m_BasePath;
    public:
        RelativeFileSystem(IFileSystem *fs, const std::filesystem::path& basePath);
        ~RelativeFileSystem();

        [[nodiscard]] std::filesystem::path const& GetBasePath() const { return m_BasePath; }

        bool folderExists(const std::filesystem::path& name) override;
        bool fileExists(const std::filesystem::path& name) override;
        nvrhi::FRESULT readFile(const std::filesystem::path& name, nvrhi::IDataBlob **ppBlob) override;
        bool writeFile(const std::filesystem::path& name, const void* data, size_t size) override;
        int enumerateFiles(const std::filesystem::path& path, const std::vector<std::string>& extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        int enumerateDirectories(const std::filesystem::path& path, enumerate_callback_t callback, bool allowDuplicates = false) override;
    };

    // A virtual file system that allows mounting, or attaching, other VFS objects to paths.
    // Does not have any file systems by default, all of them must be mounted first.
    class RootFileSystem : public IFileSystem
    {
        NVRHI_INHERIT_INTERFACE_TABLE()
    private:
        std::vector<std::pair<std::string, IFileSystem *>> m_MountPoints;

        bool findMountPoint(const std::filesystem::path& path, std::filesystem::path* pRelativePath, IFileSystem** ppFS);
    public:
        ~RootFileSystem();
        void mount(const std::filesystem::path& path, IFileSystem *fs);
        void mount(const std::filesystem::path& path, const std::filesystem::path& nativePath);
        bool unmount(const std::filesystem::path& path);

		bool folderExists(const std::filesystem::path& name) override;
        bool fileExists(const std::filesystem::path& name) override;
        nvrhi::FRESULT readFile(const std::filesystem::path& name,
                                        nvrhi::IDataBlob** ppBlob) override;
        bool writeFile(const std::filesystem::path& name, const void* data, size_t size) override;
        int enumerateFiles(const std::filesystem::path& path, const std::vector<std::string>& extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        int enumerateDirectories(const std::filesystem::path& path, enumerate_callback_t callback, bool allowDuplicates = false) override;
    };

    std::string getFileSearchRegex(const std::filesystem::path& path, const std::vector<std::string>& extensions);
}