#include "ReelCamMediaStream.h"
#include "ReelCamMediaSource.h"
#include "HResultHelpers.h"
#include "Logging.h"

#include <mfapi.h>
#include <new>
#include <string.h>

// NV12 layout: HEIGHT rows of Y (1 byte/pixel), then HEIGHT/2 rows of UV interleaved
static const UINT32 NV12_Y_STRIDE  = ReelCamMediaStream::VIDEO_WIDTH;
static const UINT32 NV12_UV_STRIDE = ReelCamMediaStream::VIDEO_WIDTH;
static const UINT32 NV12_FRAME_SIZE =
    NV12_Y_STRIDE * ReelCamMediaStream::VIDEO_HEIGHT +
    NV12_UV_STRIDE * (ReelCamMediaStream::VIDEO_HEIGHT / 2);

// SMPTE-ish color bars in YUV (BT.601):
// White, Yellow, Cyan, Green, Magenta, Red, Blue, Black
static const BYTE BAR_Y[] = { 235, 210, 170, 145, 106, 81, 41, 16 };
static const BYTE BAR_U[] = { 128, 16, 166, 54, 202, 90, 240, 128 };
static const BYTE BAR_V[] = { 128, 146, 16, 34, 222, 240, 110, 128 };
static const int  NUM_BARS = 8;

// ============================================================================

ReelCamMediaStream::ReelCamMediaStream(ReelCamMediaSource* pParent)
    : m_cRef(1)
    , m_bShutdown(false)
    , m_bActive(false)
    , m_pParent(pParent)
    , m_frameIndex(0)
{
    InitializeCriticalSection(&m_cs);
    QueryPerformanceFrequency(&m_qpcFreq);
    m_qpcStart.QuadPart = 0;
}

ReelCamMediaStream::~ReelCamMediaStream()
{
    DeleteCriticalSection(&m_cs);
}

HRESULT ReelCamMediaStream::Initialize()
{
    RETURN_IF_FAILED(MFCreateEventQueue(m_pEventQueue.ReleaseAndGetAddressOf()));
    RETURN_IF_FAILED(CreateStreamDescriptor());
    LOG_INFO("MediaStream", "Initialized %ux%u @ %u fps, NV12", VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS);
    return S_OK;
}

// --- IUnknown ---

STDMETHODIMP ReelCamMediaStream::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;

    if (IsEqualIID(riid, IID_IUnknown))
        *ppv = static_cast<IMFMediaStream*>(this);
    else if (IsEqualIID(riid, IID_IMFMediaEventGenerator))
        *ppv = static_cast<IMFMediaEventGenerator*>(this);
    else if (IsEqualIID(riid, IID_IMFMediaStream))
        *ppv = static_cast<IMFMediaStream*>(this);
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ReelCamMediaStream::AddRef()  { return InterlockedIncrement(&m_cRef); }
STDMETHODIMP_(ULONG) ReelCamMediaStream::Release()
{
    long cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) delete this;
    return cRef;
}

// --- IMFMediaEventGenerator ---

STDMETHODIMP ReelCamMediaStream::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* pState)
{
    EnterCriticalSection(&m_cs);
    HRESULT hr = m_bShutdown ? MF_E_SHUTDOWN : m_pEventQueue->BeginGetEvent(pCallback, pState);
    LeaveCriticalSection(&m_cs);
    return hr;
}

STDMETHODIMP ReelCamMediaStream::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    EnterCriticalSection(&m_cs);
    HRESULT hr = m_bShutdown ? MF_E_SHUTDOWN : m_pEventQueue->EndGetEvent(pResult, ppEvent);
    LeaveCriticalSection(&m_cs);
    return hr;
}

STDMETHODIMP ReelCamMediaStream::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    ReelCam::ComPtr<IMFMediaEventQueue> pQueue;
    {
        EnterCriticalSection(&m_cs);
        if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }
        pQueue = m_pEventQueue;
        LeaveCriticalSection(&m_cs);
    }
    return pQueue->GetEvent(dwFlags, ppEvent);
}

STDMETHODIMP ReelCamMediaStream::QueueEvent(
    MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    EnterCriticalSection(&m_cs);
    HRESULT hr = m_bShutdown ? MF_E_SHUTDOWN
                             : m_pEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
    LeaveCriticalSection(&m_cs);
    return hr;
}

// --- IMFMediaStream ---

STDMETHODIMP ReelCamMediaStream::GetMediaSource(IMFMediaSource** ppMediaSource)
{
    if (!ppMediaSource) return E_POINTER;
    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }
    *ppMediaSource = static_cast<IMFMediaSource*>(m_pParent);
    (*ppMediaSource)->AddRef();
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP ReelCamMediaStream::GetStreamDescriptor(IMFStreamDescriptor** ppSD)
{
    if (!ppSD) return E_POINTER;
    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }
    *ppSD = m_pStreamDescriptor.Get();
    (*ppSD)->AddRef();
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP ReelCamMediaStream::RequestSample(IUnknown* pToken)
{
    EnterCriticalSection(&m_cs);
    if (m_bShutdown) { LeaveCriticalSection(&m_cs); return MF_E_SHUTDOWN; }
    if (!m_bActive) { LeaveCriticalSection(&m_cs); return MF_E_INVALIDREQUEST; }

    // Generate a test pattern frame
    ReelCam::ComPtr<IMFSample> pSample;
    HRESULT hr = GenerateTestPatternSample(pSample.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("MediaStream", "Failed to generate sample: 0x%08lX", hr);
        m_pEventQueue->QueueEventParamVar(MEError, GUID_NULL, hr, nullptr);
        LeaveCriticalSection(&m_cs);
        return hr;
    }

    // Attach the token if provided
    if (pToken) {
        pSample->SetUnknown(MFSampleExtension_Token, pToken);
    }

    // Queue MEMediaSample event with the sample
    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_UNKNOWN;
    var.punkVal = pSample.Get();
    var.punkVal->AddRef();

    hr = m_pEventQueue->QueueEventParamVar(MEMediaSample, GUID_NULL, S_OK, &var);
    PropVariantClear(&var);

    LeaveCriticalSection(&m_cs);
    return hr;
}

// --- Source-driven lifecycle ---

HRESULT ReelCamMediaStream::Start(const PROPVARIANT* /*pvarStartPosition*/)
{
    EnterCriticalSection(&m_cs);
    m_bActive = true;
    m_frameIndex = 0;
    QueryPerformanceCounter(&m_qpcStart);

    // Notify consumers the stream is started
    m_pEventQueue->QueueEventParamVar(MEStreamStarted, GUID_NULL, S_OK, nullptr);
    LeaveCriticalSection(&m_cs);
    LOG_INFO("MediaStream", "Started");
    return S_OK;
}

HRESULT ReelCamMediaStream::Stop()
{
    EnterCriticalSection(&m_cs);
    m_bActive = false;
    m_pEventQueue->QueueEventParamVar(MEStreamStopped, GUID_NULL, S_OK, nullptr);
    LeaveCriticalSection(&m_cs);
    LOG_INFO("MediaStream", "Stopped");
    return S_OK;
}

HRESULT ReelCamMediaStream::Pause()
{
    EnterCriticalSection(&m_cs);
    m_bActive = false;
    m_pEventQueue->QueueEventParamVar(MEStreamPaused, GUID_NULL, S_OK, nullptr);
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

HRESULT ReelCamMediaStream::Shutdown()
{
    EnterCriticalSection(&m_cs);
    m_bShutdown = true;
    m_bActive = false;
    if (m_pEventQueue) m_pEventQueue->Shutdown();
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

// --- Internal helpers ---

HRESULT ReelCamMediaStream::CreateMediaType(IMFMediaType** ppType)
{
    ReelCam::ComPtr<IMFMediaType> pType;
    RETURN_IF_FAILED(MFCreateMediaType(pType.ReleaseAndGetAddressOf()));
    RETURN_IF_FAILED(pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    RETURN_IF_FAILED(pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12));
    RETURN_IF_FAILED(pType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
    RETURN_IF_FAILED(MFSetAttributeSize(pType.Get(), MF_MT_FRAME_SIZE, VIDEO_WIDTH, VIDEO_HEIGHT));
    RETURN_IF_FAILED(MFSetAttributeRatio(pType.Get(), MF_MT_FRAME_RATE, VIDEO_FPS, 1));
    RETURN_IF_FAILED(MFSetAttributeRatio(pType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1));

    // Set default stride (NV12: width bytes for Y plane)
    RETURN_IF_FAILED(pType->SetUINT32(MF_MT_DEFAULT_STRIDE, VIDEO_WIDTH));
    RETURN_IF_FAILED(pType->SetUINT32(MF_MT_SAMPLE_SIZE, NV12_FRAME_SIZE));
    RETURN_IF_FAILED(pType->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE));
    RETURN_IF_FAILED(pType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE));

    *ppType = pType.Detach();
    return S_OK;
}

HRESULT ReelCamMediaStream::CreateStreamDescriptor()
{
    ReelCam::ComPtr<IMFMediaType> pType;
    RETURN_IF_FAILED(CreateMediaType(pType.ReleaseAndGetAddressOf()));

    IMFMediaType* pTypes[] = { pType.Get() };
    RETURN_IF_FAILED(MFCreateStreamDescriptor(0, 1, pTypes, m_pStreamDescriptor.ReleaseAndGetAddressOf()));

    // Set the current media type on the type handler
    ReelCam::ComPtr<IMFMediaTypeHandler> pHandler;
    RETURN_IF_FAILED(m_pStreamDescriptor->GetMediaTypeHandler(pHandler.ReleaseAndGetAddressOf()));
    RETURN_IF_FAILED(pHandler->SetCurrentMediaType(pType.Get()));

    // Required by Windows Camera Frame Server for video capture streams
    static const GUID GUID_PINNAME_VIDEO_CAPTURE = 
        { 0xfb6c4281, 0x0353, 0x11d1, { 0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba } };
    m_pStreamDescriptor->SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, GUID_PINNAME_VIDEO_CAPTURE);
    m_pStreamDescriptor->SetUINT32(MF_DEVICESTREAM_STREAM_ID, 0);
    m_pStreamDescriptor->SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, 1);

    return S_OK;
}

HRESULT ReelCamMediaStream::GenerateTestPatternSample(IMFSample** ppSample)
{
    // Create a memory buffer for NV12 frame
    ReelCam::ComPtr<IMFMediaBuffer> pBuffer;
    RETURN_IF_FAILED(MFCreateMemoryBuffer(NV12_FRAME_SIZE, pBuffer.ReleaseAndGetAddressOf()));

    BYTE* pData = nullptr;
    DWORD maxLen = 0;
    RETURN_IF_FAILED(pBuffer->Lock(&pData, &maxLen, nullptr));

    bool frameFromShm = false;
    if (m_shmConsumer.IsActive()) {
        uint32_t w = 0, h = 0, strideY = 0, strideUV = 0;
        int64_t pts = 0;
        if (m_shmConsumer.ReadLatestFrame(pData, maxLen, &w, &h, &strideY, &strideUV, &pts)) {
            frameFromShm = true;
        }
    }

    if (!frameFromShm) {
        // Fallback: procedural SMPTE color bars with live scanning line
        FillNV12ColorBars(pData, VIDEO_WIDTH, VIDEO_HEIGHT, m_frameIndex);
    }

    RETURN_IF_FAILED(pBuffer->Unlock());
    RETURN_IF_FAILED(pBuffer->SetCurrentLength(NV12_FRAME_SIZE));

    // Create sample and add buffer
    ReelCam::ComPtr<IMFSample> pSample;
    RETURN_IF_FAILED(MFCreateSample(pSample.ReleaseAndGetAddressOf()));
    RETURN_IF_FAILED(pSample->AddBuffer(pBuffer.Get()));

    // Set presentation timestamp (100-nanosecond units)
    LONGLONG duration = 10000000LL / VIDEO_FPS; // duration of one frame
    LONGLONG pts = static_cast<LONGLONG>(m_frameIndex) * duration;
    pSample->SetSampleTime(pts);
    pSample->SetSampleDuration(duration);

    m_frameIndex++;

    *ppSample = pSample.Detach();
    return S_OK;
}

void ReelCamMediaStream::FillNV12ColorBars(
    BYTE* pData, UINT32 width, UINT32 height, UINT64 frameIndex)
{
    // --- Y plane ---
    BYTE* pY = pData;
    UINT32 barWidth = width / NUM_BARS;

    for (UINT32 y = 0; y < height; y++) {
        for (UINT32 x = 0; x < width; x++) {
            int barIdx = x / barWidth;
            if (barIdx >= NUM_BARS) barIdx = NUM_BARS - 1;
            pY[y * width + x] = BAR_Y[barIdx];
        }
    }

    // Add a moving vertical line to show the stream is live (1 Hz cycle)
    UINT32 lineX = static_cast<UINT32>(
        (frameIndex % static_cast<UINT64>(VIDEO_FPS)) * width / VIDEO_FPS);
    for (UINT32 y = 0; y < height; y++) {
        for (UINT32 dx = 0; dx < 4 && (lineX + dx) < width; dx++) {
            pY[y * width + lineX + dx] = 235; // bright white line
        }
    }

    // --- UV plane (interleaved U, V) ---
    BYTE* pUV = pData + width * height;
    UINT32 uvHeight = height / 2;

    for (UINT32 y = 0; y < uvHeight; y++) {
        for (UINT32 x = 0; x < width; x += 2) {
            int barIdx = x / barWidth;
            if (barIdx >= NUM_BARS) barIdx = NUM_BARS - 1;
            UINT32 uvOffset = y * width + x;
            pUV[uvOffset]     = BAR_U[barIdx];
            pUV[uvOffset + 1] = BAR_V[barIdx];
        }
    }
}
