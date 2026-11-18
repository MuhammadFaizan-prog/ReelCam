#include "ClassFactory.h"
#include "ReelCamMediaSource.h"
#include "Logging.h"
#include <new>

// Defined in dllmain.cpp
void DllAddRef();
void DllRelease();

ReelCamClassFactory::ReelCamClassFactory() : m_cRef(1)
{
    DllAddRef();
}

ReelCamClassFactory::~ReelCamClassFactory()
{
    DllRelease();
}

STDMETHODIMP ReelCamClassFactory::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ReelCamClassFactory::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) ReelCamClassFactory::Release()
{
    long cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) {
        delete this;
    }
    return cRef;
}

STDMETHODIMP ReelCamClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    // No aggregation support
    if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

    LOG_INFO("ClassFactory", "Creating ReelCamMediaSource instance");

    auto* pSource = new (std::nothrow) ReelCamMediaSource();
    if (!pSource) return E_OUTOFMEMORY;

    HRESULT hr = pSource->Initialize();
    if (FAILED(hr)) {
        pSource->Release();
        return hr;
    }

    hr = pSource->QueryInterface(riid, ppv);
    pSource->Release(); // QI AddRef'd if successful
    return hr;
}

STDMETHODIMP ReelCamClassFactory::LockServer(BOOL fLock)
{
    if (fLock) DllAddRef();
    else       DllRelease();
    return S_OK;
}
