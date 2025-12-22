#include "DataBlob.h"
#include <cstring>

namespace donut {

DONUT_BEGIN_INTERFACE_TABLE(DataBlobImpl)
DONUT_IMPLEMENTS_INTERFACE(DataBlobImpl)
DONUT_IMPLEMENTS_INTERFACE(IDataBlob)
DONUT_END_INTERFACE_TABLE()

DataBlobImpl::DataBlobImpl(size_t InitialSize, const void* pData)
      : m_DataBuff(InitialSize) {
    if (!m_DataBuff.empty() && pData != nullptr) {
        std::memcpy(m_DataBuff.data(), pData, InitialSize);
    }
}

/// Sets the size of the internal data buffer
void DataBlobImpl::Resize(size_t NewSize) { m_DataBuff.resize(NewSize); }

/// Returns the size of the internal data buffer
size_t DataBlobImpl::GetSize() const { return m_DataBuff.size(); }

/// Returns the pointer to the internal data buffer
void* DataBlobImpl::GetDataPtr() { return m_DataBuff.data(); }

/// Returns const pointer to the internal data buffer
const void* DataBlobImpl::GetConstDataPtr() const { return m_DataBuff.data(); }

DONUT_BEGIN_INTERFACE_TABLE(StringDataBlobImpl)
DONUT_IMPLEMENTS_INTERFACE(StringDataBlobImpl)
DONUT_IMPLEMENTS_INTERFACE(IDataBlob)
DONUT_END_INTERFACE_TABLE()

DONUT_BEGIN_INTERFACE_TABLE(ProxyDataBlobImpl)
DONUT_IMPLEMENTS_INTERFACE(ProxyDataBlobImpl)
DONUT_IMPLEMENTS_INTERFACE(IDataBlob)
DONUT_END_INTERFACE_TABLE()

FRESULT CreateBlob(size_t Size, IDataBlob **ppBlob) {
    auto blob = MAKE_RC_OBJ0(DataBlobImpl)(Size);
    if(ppBlob) {
        *ppBlob = blob;
        blob->AddRef();
    }
    blob->Release();
    return FS_OK;
}

FRESULT CreateStringBlob(size_t Size, IDataBlob **ppBlob) {
    auto blob = MAKE_RC_OBJ0(StringDataBlobImpl)(Size);
    if (ppBlob) {
        *ppBlob = blob;
        blob->AddRef();
    }
    blob->Release();
    return FS_OK;
}

FRESULT CreateProxyBlob(size_t Size, const void *pData, IDataBlob **ppBlob) {
    auto blob = MAKE_RC_OBJ0(ProxyDataBlobImpl)(Size, pData);
    if (ppBlob) {
        *ppBlob = blob;
        blob->AddRef();
    }
    blob->Release();
    return FS_OK;
}

}  // namespace donut
