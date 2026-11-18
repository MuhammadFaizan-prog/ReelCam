#include "ReelCamMediaSource.h"
#include "ReelCamMediaStream.h"
#include "HResultHelpers.h"
#include "Logging.h"
#include <new>

// Forward from dllmain.cpp
void DllAddRef();
void DllRelease();

// --- Helper: Create a single-stream presentation descriptor for NV12 1080p30 ---
static HRESULT CreatePresentationDescriptor1080p30(
    IMFStreamDescriptor* pSD,
    IMFPresentationDescriptor** ppPD)
{
    RETURN_IF_FAILED(MFCreatePresentationDescriptor(1, &pSD, ppPD));
    RETURN_IF_FAILED((*ppPD)->SelectStream(0));
    return S_OK;
}

// ============================================================================
// ReelCamMediaSource implementation
// ============================================================================

ReelCamMediaSource::ReelCamMediaSource()
    : m_cRef(1)
    , m_bShutdown(false)
    , m_pStream(nullptr)
{
    InitializeCriticalSection(&m_cs);
    DllAddRef();
    LOG_INFO("MediaSource", "Created");
}

ReelCamMediaSource::~ReelCamMediaSource()
{
    Shutdown();
    DeleteCriticalSection(&m_cs);
    DllRelease();
    LOG_INFO("MediaSource", "Destroyed");
}

HRESULT ReelCamMediaSource::Initialize()
{
    EnterCriticalSection(&m_cs);

    // Create event queue
    HRESULT hr = MFCreateEventQueue(m_pEventQueue.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { LeaveCriticalSection(&m_cs); return hr; }

    // Create the video stream (test pattern generator)
    m_pStream = new (std::nothrow) ReelCamMediaStream(this);
    if (!m_pStream) { LeaveCriticalSection(&m_cs); return E_OUTOFMEMORY; }

    hr = m_pStream->Initialize();
    if (FAILED(hr)) {
        m_pStream->Release();
        m_pStream = nullptr;
        LeaveCriticalSection(&m_cs);
        return hr;
    }

    // Build presentation descriptor from stream descriptor
    ReelCam::ComPtr<IMFStreamDescriptor> pSD;
    hr = m_pStream->GetStreamDescriptor(pSD.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { LeaveCriticalSection(&m_cs); return hr; }

    hr = CreatePresentationDescriptor1080p30(pSD.Get(), m_pPresentationDescriptor.ReleaseAndGetAddressOf());

    LeaveCriticalSection(&m_cs);
    return hr;
}

// --- IUnknown ---

STDMETHODIMP ReelCamMediaSource::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown))
        *ppv = static_cast<IMFMediaSource*>(this);
    else if (IsEqualIID(riid, IID_IMFMediaEventGenerator))
        *ppv = static_cast<IMFMediaEventGenerator*>(this);
    else if (IsEqualIID(riid, IID_IMFMediaSource))
        *ppv = static_cast<IMFMediaSource*>(this);
    else if (IsEqualIID(riid, IID_IMFGetService))
        *ppv = static_cast<IMFGetService*>(this);
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ReelCamMediaSource::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) ReelCamMediaSource::Release()
{
    long cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) delete this;
    return cRef;
}

// --- IMFMediaEventGenerator ---

STDMETHODIMP ReelCamMediaSource::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* pState)
{
    EnterCriticalSection(&m_cs);
    HRESULT hr = m_bShutdown ? MF_E_SHUTDOWN : m_pEventQueue->BeginGetEvent(pCallback, pState);
    LeaveCriticalSection(&m_cs);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    EnterCriticalSection(&m_cs);
    HRESULT hr = m_bShutdown ? MF_E_SHUTDOWN : m_pEventQueue->EndGetEvent(pResult, ppEvent);
    LeaveCriticalSection(&m_cs);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    // GetEvent can block, so don't hold the CS
    ReelCam::ComPtr<IMFMediaEventQueue> pQueue;
    {
        EnterCriticalSection(&m_cs);
        if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }
        pQueue = m_pEventQueue;
        LeaveCriticalSection(&m_cs);
    }
    return pQueue->GetEvent(dwFlags, ppEvent);
}

STDMETHODIMP ReelCamMediaSource::QueueEvent(MediaEventType met, REFGUID guidExtendedType,
                                             HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    EnterCriticalSection(&m_cs);
    HRESULT hr = m_bShutdown ? MF_E_SHUTDOWN
                             : m_pEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
    LeaveCriticalSection(&m_cs);
    return hr;
}

// --- IMFMediaSource ---

STDMETHODIMP ReelCamMediaSource::GetCharacteristics(DWORD* pdwCharacteristics)
{
    if (!pdwCharacteristics) return E_POINTER;
    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }
    *pdwCharacteristics = MFMEDIASOURCE_IS_LIVE;
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP ReelCamMediaSource::CreatePresentationDescriptor(IMFPresentationDescriptor** ppPD)
{
    if (!ppPD) return E_POINTER;
    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }
    HRESULT hr = m_pPresentationDescriptor->Clone(ppPD);
    LeaveCriticalSection(&m_cs);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::Start(
    IMFPresentationDescriptor* pPD,
    const GUID* pguidTimeFormat,
    const PROPVARIANT* pvarStartPosition)
{
    if (!pPD) return E_INVALIDARG;

    // Only default time format supported
    if (pguidTimeFormat && !IsEqualGUID(*pguidTimeFormat, GUID_NULL)) {
        return MF_E_UNSUPPORTED_TIME_FORMAT;
    }

    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }

    LOG_INFO("MediaSource", "Start() called");

    // Queue MENewStream or MEUpdatedStream
    HRESULT hr = QueueNewStreamEvent(pPD);
    if (FAILED(hr)) { LeaveCriticalSection(&m_cs); return hr; }

    // Tell the stream to start delivering frames
    hr = m_pStream->Start(pvarStartPosition);
    if (FAILED(hr)) { LeaveCriticalSection(&m_cs); return hr; }

    // Queue MESourceStarted
    hr = m_pEventQueue->QueueEventParamVar(MESourceStarted, GUID_NULL, S_OK, pvarStartPosition);

    LeaveCriticalSection(&m_cs);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::Stop()
{
    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }

    LOG_INFO("MediaSource", "Stop() called");

    if (m_pStream) m_pStream->Stop();
    m_pEventQueue->QueueEventParamVar(MESourceStopped, GUID_NULL, S_OK, nullptr);

    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP ReelCamMediaSource::Pause()
{
    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }

    if (m_pStream) m_pStream->Pause();
    m_pEventQueue->QueueEventParamVar(MESourcePaused, GUID_NULL, S_OK, nullptr);

    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP ReelCamMediaSource::Shutdown()
{
    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return S_OK; }

    LOG_INFO("MediaSource", "Shutdown()");
    m_bShutdown = true;

    if (m_pStream) {
        m_pStream->Shutdown();
        m_pStream->Release();
        m_pStream = nullptr;
    }

    if (m_pEventQueue) {
        m_pEventQueue->Shutdown();
    }

    LeaveCriticalSection(&m_cs);
    return S_OK;
}

// --- IMFGetService ---

STDMETHODIMP ReelCamMediaSource::GetService(REFGUID /*guidService*/, REFIID /*riid*/, LPVOID* ppvObject)
{
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    // No additional services provided in Phase 1
    return MF_E_UNSUPPORTED_SERVICE;
}

// --- Internal helpers ---

HRESULT ReelCamMediaSource::QueueNewStreamEvent(IMFPresentationDescriptor* /*pPD*/)
{
    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_UNKNOWN;
    var.punkVal = static_cast<IMFMediaStream*>(m_pStream);
    var.punkVal->AddRef();

    HRESULT hr = m_pEventQueue->QueueEventParamVar(MENewStream, GUID_NULL, S_OK, &var);
    PropVariantClear(&var);
    return hr;
}
