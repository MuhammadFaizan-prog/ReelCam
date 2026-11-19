#include "ReelCamMediaSource.h"
#include "ReelCamMediaStream.h"
#include "HResultHelpers.h"
#include "Logging.h"
#include <new>

// Forward from dllmain.cpp
void DllAddRef();
void DllRelease();

static const GUID GUID_IID_IKsControl = 
    { 0x28f54685, 0x06fd, 0x11d2, { 0xb2, 0x7a, 0x00, 0xa0, 0xc9, 0x22, 0x31, 0x96 } };

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

    // Create IMFAttributes store for IMFActivate delegation
    HRESULT hr = MFCreateAttributes(m_pAttributes.ReleaseAndGetAddressOf(), 16);
    if (FAILED(hr)) { LeaveCriticalSection(&m_cs); return hr; }

    m_pAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, L"ReelCam");
    m_pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    m_pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, GUID{ 0xe5323777, 0xf976, 0x4f5b, { 0x9b, 0x55, 0xb9, 0x46, 0x99, 0xc4, 0x6e, 0x44 } });
    m_pAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, L"\\\\?\\ReelCam#VirtualCamera");
    static const GUID GUID_MF_VIRTUALCAMERA_PROVIDE_ASSOCIATED_CAMERA_SOURCES = { 0xf0273718, 0x4a4d, 0x4ac5, { 0xa1, 0x5d, 0x30, 0x5e, 0xb5, 0xe9, 0x06, 0x67 } };
    m_pAttributes->SetUINT32(GUID_MF_VIRTUALCAMERA_PROVIDE_ASSOCIATED_CAMERA_SOURCES, 0);

    // Create event queue
    hr = MFCreateEventQueue(m_pEventQueue.ReleaseAndGetAddressOf());
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

    WCHAR iidStr[64] = {};
    StringFromGUID2(riid, iidStr, 64);

    if (IsEqualIID(riid, IID_IUnknown))
        *ppv = static_cast<IMFMediaSourceEx*>(this);
    else if (IsEqualIID(riid, IID_IMFMediaEventGenerator))
        *ppv = static_cast<IMFMediaEventGenerator*>(this);
    else if (IsEqualIID(riid, IID_IMFMediaSource))
        *ppv = static_cast<IMFMediaSource*>(this);
    else if (IsEqualIID(riid, IID_IMFMediaSourceEx))
        *ppv = static_cast<IMFMediaSourceEx*>(this);
    else if (IsEqualIID(riid, IID_IMFGetService))
        *ppv = static_cast<IMFGetService*>(this);
    else if (IsEqualIID(riid, GUID_IID_IKsControl))
        *ppv = static_cast<IKsControl*>(this);
    else if (IsEqualIID(riid, IID_IMFActivate))
        *ppv = static_cast<IMFActivate*>(this);
    else if (IsEqualIID(riid, IID_IMFAttributes))
        *ppv = static_cast<IMFAttributes*>(this);
    else {
        LOG_INFO("MediaSource", "QueryInterface REJECTED: %ls", iidStr);
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    LOG_INFO("MediaSource", "QueryInterface ACCEPTED: %ls", iidStr);
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
    LOG_INFO("MediaSource", "GetCharacteristics called");
    if (!pdwCharacteristics) return E_POINTER;
    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }
    *pdwCharacteristics = MFMEDIASOURCE_IS_LIVE;
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP ReelCamMediaSource::CreatePresentationDescriptor(IMFPresentationDescriptor** ppPD)
{
    LOG_INFO("MediaSource", "CreatePresentationDescriptor called");
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

// --- IMFMediaSourceEx ---

STDMETHODIMP ReelCamMediaSource::GetSourceAttributes(IMFAttributes** ppAttributes)
{
    LOG_INFO("MediaSource", "GetSourceAttributes called");
    if (!ppAttributes) return E_POINTER;
    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }
    *ppAttributes = m_pAttributes.Get();
    if (*ppAttributes) (*ppAttributes)->AddRef();
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP ReelCamMediaSource::GetStreamAttributes(DWORD dwStreamIdentifier, IMFAttributes** ppAttributes)
{
    LOG_INFO("MediaSource", "GetStreamAttributes called: streamId=%u", dwStreamIdentifier);
    if (!ppAttributes) return E_POINTER;
    if (dwStreamIdentifier != 0) return MF_E_INVALIDSTREAMNUMBER;

    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }
    if (!m_pStream) { LeaveCriticalSection(&m_cs); return E_UNEXPECTED; }

    ReelCam::ComPtr<IMFStreamDescriptor> pSD;
    HRESULT hr = m_pStream->GetStreamDescriptor(pSD.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { LeaveCriticalSection(&m_cs); return hr; }

    hr = pSD->QueryInterface(IID_IMFAttributes, reinterpret_cast<void**>(ppAttributes));
    LeaveCriticalSection(&m_cs);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::SetD3DManager(IUnknown* /*pManager*/)
{
    LOG_INFO("MediaSource", "SetD3DManager called");
    return S_OK;
}

// --- IMFGetService ---

STDMETHODIMP ReelCamMediaSource::GetService(REFGUID guidService, REFIID riid, LPVOID* ppvObject)
{
    WCHAR svcStr[64] = {}, iidStr[64] = {};
    StringFromGUID2(guidService, svcStr, 64);
    StringFromGUID2(riid, iidStr, 64);
    LOG_INFO("MediaSource", "GetService called: svc=%ls riid=%ls", svcStr, iidStr);
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    // No additional services provided in Phase 1
    return MF_E_UNSUPPORTED_SERVICE;
}

// --- IKsControl ---

STDMETHODIMP ReelCamMediaSource::KsProperty(
    PKSPROPERTY /*Property*/, ULONG /*PropertyLength*/,
    LPVOID /*PropertyData*/, ULONG /*DataLength*/, ULONG* BytesReturned)
{
    if (BytesReturned) *BytesReturned = 0;
    return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
}

STDMETHODIMP ReelCamMediaSource::KsMethod(
    PKSMETHOD /*Method*/, ULONG /*MethodLength*/,
    LPVOID /*MethodData*/, ULONG /*DataLength*/, ULONG* BytesReturned)
{
    if (BytesReturned) *BytesReturned = 0;
    return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
}

STDMETHODIMP ReelCamMediaSource::KsEvent(
    PKSEVENT /*Event*/, ULONG /*EventLength*/,
    LPVOID /*EventData*/, ULONG /*DataLength*/, ULONG* BytesReturned)
{
    if (BytesReturned) *BytesReturned = 0;
    return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
}

// --- IMFActivate ---

STDMETHODIMP ReelCamMediaSource::ActivateObject(REFIID riid, void** ppv)
{
    LOG_INFO("MediaSource", "ActivateObject called");
    return QueryInterface(riid, ppv);
}

STDMETHODIMP ReelCamMediaSource::ShutdownObject()
{
    LOG_INFO("MediaSource", "ShutdownObject called");
    return Shutdown();
}

STDMETHODIMP ReelCamMediaSource::DetachObject()
{
    LOG_INFO("MediaSource", "DetachObject called");
    return S_OK;
}

static void LogAttrQuery(const char* method, REFGUID guidKey, HRESULT hr)
{
    WCHAR guidStr[64] = {};
    StringFromGUID2(guidKey, guidStr, 64);
    LOG_INFO("Attributes", "%s: %ls -> 0x%08lX", method, guidStr, hr);
}

// --- IMFAttributes (all delegate to m_pAttributes) ---

STDMETHODIMP ReelCamMediaSource::GetItem(REFGUID guidKey, PROPVARIANT* pValue)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetItem(guidKey, pValue) : MF_E_SHUTDOWN;
    LogAttrQuery("GetItem", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetItemType(REFGUID guidKey, MF_ATTRIBUTE_TYPE* pType)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetItemType(guidKey, pType) : MF_E_SHUTDOWN;
    LogAttrQuery("GetItemType", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::CompareItem(REFGUID guidKey, REFPROPVARIANT Value, BOOL* pbResult)
{ return m_pAttributes ? m_pAttributes->CompareItem(guidKey, Value, pbResult) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::Compare(IMFAttributes* pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL* pbResult)
{ return m_pAttributes ? m_pAttributes->Compare(pTheirs, MatchType, pbResult) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::GetUINT32(REFGUID guidKey, UINT32* punValue)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetUINT32(guidKey, punValue) : MF_E_SHUTDOWN;
    LogAttrQuery("GetUINT32", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetUINT64(REFGUID guidKey, UINT64* punValue)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetUINT64(guidKey, punValue) : MF_E_SHUTDOWN;
    LogAttrQuery("GetUINT64", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetDouble(REFGUID guidKey, double* pfValue)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetDouble(guidKey, pfValue) : MF_E_SHUTDOWN;
    LogAttrQuery("GetDouble", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetGUID(REFGUID guidKey, GUID* pguidValue)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetGUID(guidKey, pguidValue) : MF_E_SHUTDOWN;
    LogAttrQuery("GetGUID", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetStringLength(REFGUID guidKey, UINT32* pcchLength)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetStringLength(guidKey, pcchLength) : MF_E_SHUTDOWN;
    LogAttrQuery("GetStringLength", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetString(REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32* pcchLength)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetString(guidKey, pwszValue, cchBufSize, pcchLength) : MF_E_SHUTDOWN;
    LogAttrQuery("GetString", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetAllocatedString(REFGUID guidKey, LPWSTR* ppwszValue, UINT32* pcchLength)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetAllocatedString(guidKey, ppwszValue, pcchLength) : MF_E_SHUTDOWN;
    LogAttrQuery("GetAllocatedString", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetBlobSize(REFGUID guidKey, UINT32* pcbBlobSize)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetBlobSize(guidKey, pcbBlobSize) : MF_E_SHUTDOWN;
    LogAttrQuery("GetBlobSize", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetBlob(REFGUID guidKey, UINT8* pBuf, UINT32 cbBufSize, UINT32* pcbBlobSize)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetBlob(guidKey, pBuf, cbBufSize, pcbBlobSize) : MF_E_SHUTDOWN;
    LogAttrQuery("GetBlob", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetAllocatedBlob(REFGUID guidKey, UINT8** ppBuf, UINT32* pcbSize)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetAllocatedBlob(guidKey, ppBuf, pcbSize) : MF_E_SHUTDOWN;
    LogAttrQuery("GetAllocatedBlob", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::GetUnknown(REFGUID guidKey, REFIID riid, LPVOID* ppv)
{
    HRESULT hr = m_pAttributes ? m_pAttributes->GetUnknown(guidKey, riid, ppv) : MF_E_SHUTDOWN;
    LogAttrQuery("GetUnknown", guidKey, hr);
    return hr;
}

STDMETHODIMP ReelCamMediaSource::SetItem(REFGUID guidKey, REFPROPVARIANT Value)
{ return m_pAttributes ? m_pAttributes->SetItem(guidKey, Value) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::DeleteItem(REFGUID guidKey)
{ return m_pAttributes ? m_pAttributes->DeleteItem(guidKey) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::DeleteAllItems()
{ return m_pAttributes ? m_pAttributes->DeleteAllItems() : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::SetUINT32(REFGUID guidKey, UINT32 unValue)
{ return m_pAttributes ? m_pAttributes->SetUINT32(guidKey, unValue) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::SetUINT64(REFGUID guidKey, UINT64 unValue)
{ return m_pAttributes ? m_pAttributes->SetUINT64(guidKey, unValue) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::SetDouble(REFGUID guidKey, double fValue)
{ return m_pAttributes ? m_pAttributes->SetDouble(guidKey, fValue) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::SetGUID(REFGUID guidKey, REFGUID guidValue)
{ return m_pAttributes ? m_pAttributes->SetGUID(guidKey, guidValue) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::SetString(REFGUID guidKey, LPCWSTR wszValue)
{ return m_pAttributes ? m_pAttributes->SetString(guidKey, wszValue) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::SetBlob(REFGUID guidKey, const UINT8* pBuf, UINT32 cbBufSize)
{ return m_pAttributes ? m_pAttributes->SetBlob(guidKey, pBuf, cbBufSize) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::SetUnknown(REFGUID guidKey, IUnknown* pUnknown)
{ return m_pAttributes ? m_pAttributes->SetUnknown(guidKey, pUnknown) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::LockStore()
{ return m_pAttributes ? m_pAttributes->LockStore() : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::UnlockStore()
{ return m_pAttributes ? m_pAttributes->UnlockStore() : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::GetCount(UINT32* pcItems)
{ return m_pAttributes ? m_pAttributes->GetCount(pcItems) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::GetItemByIndex(UINT32 unIndex, GUID* pguidKey, PROPVARIANT* pValue)
{ return m_pAttributes ? m_pAttributes->GetItemByIndex(unIndex, pguidKey, pValue) : MF_E_SHUTDOWN; }

STDMETHODIMP ReelCamMediaSource::CopyAllItems(IMFAttributes* pDest)
{ return m_pAttributes ? m_pAttributes->CopyAllItems(pDest) : MF_E_SHUTDOWN; }

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
