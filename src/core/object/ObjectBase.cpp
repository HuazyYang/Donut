#include <donut/core/object/ObjectBase.h>
#include <deque>
#include <algorithm>
#include <stdio.h>

namespace donut {
namespace details {
FRESULT InterfaceTableQueryInterface(void *pThis, const INTERFACE_ENTRY *pTable, FREFIID riid, void **ppv) {
    if (riid == IID_IObject) {
        // first entry must be an offset
        if(ppv) {
            *ppv = (char *)pThis + pTable->data;
            ((IObject *)(*ppv))->AddRef();
        }
        return FS_OK;
    } else {
        FRESULT hr = FE_NOINTERFACE;

        while (pTable->pfnFinder) {
            if (!pTable->pIID || riid == *pTable->pIID) {
                if (pTable->pfnFinder == DONUT_ENTRY_IS_OFFSET) {
                    if(ppv) {
                        *ppv = (char *)pThis + pTable->data;
                        ((IObject *)(*ppv))->AddRef();
                    }
                    hr = FS_OK;
                    break;
                } else {
                    hr = pTable->pfnFinder(pThis, pTable->data, riid, ppv);

                    if (hr == FS_OK)
                        break;
                }
            }
            pTable++;
        }
        if (hr != FS_OK) {
            if (ppv) *ppv = 0;
        }
        return hr;
    }
}

class ObjectTracker {
public:
    static ObjectTracker s_Instance;

    void AddObject(IObject *pObj);

    bool RemoveObject(IObject *pObj);

    void Dump();

private:
    ObjectTracker();
    ~ObjectTracker();

    ObjectTracker(const ObjectTracker &) = delete;
    ObjectTracker &operator=(const ObjectTracker &) = delete;

    std::recursive_mutex m_Lock;
    std::deque<IObject *> m_AliveObjectContainer;
};

ObjectTracker ObjectTracker::s_Instance;

void ObjectTracker::AddObject(IObject *pObj) {
    std::lock_guard<std::recursive_mutex> lock(m_Lock);
    m_AliveObjectContainer.push_back(pObj);
}

bool ObjectTracker::RemoveObject(IObject *pObj) {
    std::lock_guard<std::recursive_mutex> lock(m_Lock);
    auto it = std::find(m_AliveObjectContainer.begin(), m_AliveObjectContainer.end(), pObj);
    if (it != m_AliveObjectContainer.end()) {
        m_AliveObjectContainer.erase(it);
        return true;
    }
    return false;
}

void ObjectTracker::Dump() {
    std::lock_guard<std::recursive_mutex> lock(m_Lock);
    if (!m_AliveObjectContainer.empty()) {
        printf("ObjectTracker::Dump() detect %llu alive objects:\n", m_AliveObjectContainer.size());
    }
}

ObjectTracker::ObjectTracker() {}

ObjectTracker::~ObjectTracker() { Dump(); }

void ObjectTrackerAddObject(IObject *pObj) { return ObjectTracker::s_Instance.AddObject(pObj); }

bool ObjectTrackerRemoveObject(IObject *pObj) { return ObjectTracker::s_Instance.RemoveObject(pObj); }

}

}  // namespace donut
