#pragma once
// COM Class Factory for the ReelCam Media Source.

#include <windows.h>
#include <unknwn.h>

class ReelCamClassFactory : public IClassFactory
{
public:
    ReelCamClassFactory();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

private:
    ~ReelCamClassFactory();
    long m_cRef;
};
