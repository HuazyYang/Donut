#ifndef COMMON_H
#define COMMON_H
#include <donut/core/object/Object.h>

namespace donut {

constexpr FRESULT FS_OK = 0;
constexpr FRESULT FE_GENERIC_ERROR = -1u;
constexpr FRESULT FE_NOINTERFACE = -2;
constexpr FRESULT FE_NOT_IMPLEMENT = -3;
constexpr FRESULT FE_INVALID_ARGS = -4;
constexpr FRESULT FE_NOT_ALIVE_OBJECT = -4;
constexpr FRESULT FE_NOT_FOUND = -5;
constexpr FRESULT FE_WAIT_TIMEOUT = -6;

/// Binary data blob
// {F578FF0D-ABD2-4514-9D32-7CB454D4A73B}
DONUT_IID(IDataBlob, "f578ff0d-abd2-4514-9d32-7cb454d4a73b")
struct IDataBlob : public IObject {
    /// Sets the size of the internal data buffer
    virtual void Resize(size_t NewSize) = 0;

    /// Returns the size of the internal data buffer
    virtual size_t GetSize() const = 0;

    /// Returns the pointer to the internal data buffer
    virtual void *GetDataPtr() = 0;

    /// Returns const pointer to the internal data buffer
    virtual const void *GetConstDataPtr() const = 0;
};

FRESULT CreateBlob(size_t Size, IDataBlob **ppBlob);

FRESULT CreateStringBlob(size_t Size, IDataBlob **ppBlob);

FRESULT CreateProxyBlob(size_t Size, const void *pData, IDataBlob **ppBlob);

}

#endif /* COMMON_H */
