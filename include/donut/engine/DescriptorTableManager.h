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
#include <nvrhi/nvrhi.h>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace donut::engine
{
    class DescriptorTableManager;
    typedef int DescriptorIndex;

    // Stores a descriptor index in a descriptor table. Releases the descriptor when destroyed.
    class DescriptorHandle: public ObjectImpl<IObject>
    {
    private:
      friend class DescriptorTableManager;
      DescriptorHandle(DescriptorTableManager *managerPtr,
                       DescriptorIndex index);
      WeakPtr<DescriptorTableManager> m_Manager;
      DescriptorIndex m_DescriptorIndex;

    public:
        DescriptorHandle();
        ~DescriptorHandle();
        
        [[nodiscard]] bool IsValid() const { return m_DescriptorIndex >= 0 && m_Manager.IsValid(); }
        [[nodiscard]] DescriptorIndex Get() const { if (m_DescriptorIndex >= 0) assert(m_Manager.IsValid()); return m_DescriptorIndex; }

        DescriptorHandle &operator=(DescriptorHandle &&rhs) noexcept;
        DescriptorHandle(DescriptorHandle &&rhs) noexcept;
        
        // For ResourceDescriptorHeap Index instead of a table relative index. Call only
        // once all allocation is done: growing the table stales every index returned.
        [[nodiscard]] DescriptorIndex GetIndexInHeap() const;
        // Releases the descriptor and returns to the empty state.
        void Reset();

        // Movable but non-copyable
        DescriptorHandle(const DescriptorHandle&) = delete;
        DescriptorHandle& operator=(const DescriptorHandle&) = delete;
    };

    class DescriptorTableManager : public WeakableImpl<IWeakable>
    {
    protected:
        // Custom hasher that doesn't look at the binding slot
        struct BindingSetItemHasher
        {
            std::size_t operator()(const nvrhi::BindingSetItem& item) const
            {
                size_t hash = 0;
                nvrhi::hash_combine(hash, item.resourceHandle);
                nvrhi::hash_combine(hash, item.type);
                nvrhi::hash_combine(hash, item.format);
                nvrhi::hash_combine(hash, item.dimension);
                nvrhi::hash_combine(hash, item.overrideComponentMapping);
                nvrhi::hash_combine(hash, item.rawData[0]);
                nvrhi::hash_combine(hash, item.rawData[1]);
                return hash;
            }
        };

        // Custom equality tester that doesn't look at the binding slot
        struct BindingSetItemsEqual
        {
            bool operator()(const nvrhi::BindingSetItem& a, const nvrhi::BindingSetItem& b) const 
            {
                return a.resourceHandle == b.resourceHandle
                    && a.type == b.type
                    && a.format == b.format
                    && a.dimension == b.dimension
                    && a.overrideComponentMapping == b.overrideComponentMapping
                    && a.subresources == b.subresources;
            }
        };
        
        nvrhi::DeviceHandle m_Device;
        nvrhi::DescriptorTableHandle m_DescriptorTable;

        // Guards the allocation bookkeeping below and the m_DescriptorTable resizes
        // that grow it; descriptors are created and released from multiple threads.
        mutable std::mutex m_Mutex;

        std::vector<nvrhi::BindingSetItem> m_Descriptors;
        std::unordered_map<nvrhi::BindingSetItem, DescriptorIndex, BindingSetItemHasher, BindingSetItemsEqual> m_DescriptorIndexMap;
        // Doubles as the allocation bitmap: a slot is free exactly when its count is
        // zero. CreateDescriptor hands out an existing index for an equal item, so a
        // slot is torn down only once every handle sharing it has been released.
        std::vector<uint32_t> m_DescriptorRefCounts;
        int m_SearchStart = 0;
        uint32_t m_AllocatedCount = 0;
        
    public:
        DescriptorTableManager(nvrhi::IDevice* device, nvrhi::IBindingLayout* layout);
        ~DescriptorTableManager();
        
        nvrhi::IDescriptorTable* GetDescriptorTable() const { return m_DescriptorTable; }

        void ReserveCapacity(uint32_t capacity);

        // Coherent snapshot of table fullness, both fields read under one lock. Only
        // meaningful once allocation has quiesced; a concurrent create/release stales it.
        struct Usage { uint32_t allocated; uint32_t capacity; };
        Usage GetUsage() const;

        DescriptorIndex CreateDescriptor(nvrhi::BindingSetItem item);
        DescriptorHandle CreateDescriptorHandle(nvrhi::BindingSetItem item);
        nvrhi::BindingSetItem GetDescriptor(DescriptorIndex index);
        void ReleaseDescriptor(DescriptorIndex index);
    };
}
