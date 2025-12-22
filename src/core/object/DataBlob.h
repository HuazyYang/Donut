#ifndef CORE_OBJECT_DATABLOB_H
#define CORE_OBJECT_DATABLOB_H
/// Implementation for the IDataBlob interface

#include <donut/core/object/ObjectBase.h>
#include <donut/core/object/Common.h>
#include <vector>

namespace donut {

/// Base interface for a data blob
DONUT_CLSID(DataBlobImpl, "405202ca-4daa-459c-9da8-6996ca3fb1d4")
class DataBlobImpl final : public ObjectImpl<IDataBlob> {
 public:
    DONUT_DECLARE_UUID_TRAITS(DataBlobImpl)
    DONUT_DECLARE_INTERFACE_TABLE()

    /// Sets the size of the internal data buffer
    virtual void Resize(size_t NewSize) override;

    /// Returns the size of the internal data buffer
    virtual size_t GetSize() const override;

    /// Returns the pointer to the internal data buffer
    virtual void *GetDataPtr() override;

    /// Returns const pointer to the internal data buffer
    virtual const void *GetConstDataPtr() const override;

    explicit DataBlobImpl(size_t InitialSize = 0, const void *pData = nullptr);

 private:
    std::vector<uint8_t> m_DataBuff;
};

/// String data blob implementation.
DONUT_CLSID(StringDataBlobImpl, "2bf21355-9bf0-4ed4-b2e9-e5a45a25cfa2")
class StringDataBlobImpl : public ObjectImpl<IDataBlob> {
    DONUT_DECLARE_UUID_TRAITS(StringDataBlobImpl)
 public:
    DONUT_DECLARE_INTERFACE_TABLE()

    /// Sets the size of the internal data buffer
    virtual void Resize(size_t NewSize) override { m_String.resize(NewSize); }

    /// Returns the size of the internal data buffer
    virtual size_t GetSize() const override { return m_String.length(); }

    /// Returns the pointer to the internal data buffer
    virtual void *GetDataPtr() override { return &m_String[0]; }

    /// Returns the pointer to the internal data buffer
    virtual const void *GetConstDataPtr() const override { return &m_String[0]; }

    StringDataBlobImpl(size_t Size, const char *pData = nullptr) {
        m_String.resize(Size);
        if (pData) std::strncpy((char *)m_String.data(), pData, Size);
    }

 private:
    std::string m_String;
};

DONUT_CLSID(ProxyDataBlobImpl, "d1373bc6-c59a-40c5-ac46-56d299206d43")
struct ProxyDataBlobImpl : ObjectImpl<IDataBlob> {
    DONUT_DECLARE_INTERFACE_TABLE()

    virtual void Resize(size_t NewSize) override {
        DONUT_VERIFY(false, "Operation forbidden");
    }

    virtual size_t GetSize() const override { return m_Size; }

    virtual void *GetDataPtr() override {
        DONUT_VERIFY(false, "Operation forbidden");
        return nullptr;
    }

    virtual const void *GetConstDataPtr() const override { return m_pData; }

    ProxyDataBlobImpl(size_t Size, const void *pData)
    : m_pData{pData}, m_Size{Size} {}

 private:
    const void *const m_pData;
    const size_t m_Size;
};

}  // namespace donut

#endif /* CORE_OBJECT_DATABLOB_H */
