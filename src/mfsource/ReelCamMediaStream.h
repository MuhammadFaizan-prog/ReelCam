#pragma once
// ReelCamMediaStream: IMFMediaStream delivering NV12 video frames.
// Phase 1 spike: generates SMPTE-style color bar test pattern.
// Phase 2+: reads frames from shared memory ring buffer written by core process.

#include <windows.h>
#include <mfidl.h>
#include <mfapi.h>
#include <mfobjects.h>
#include <mferror.h>

#include "ComPtr.h"
#include "SharedMemory.h"

class ReelCamMediaSource; // forward

class ReelCamMediaStream
    : public IMFMediaStream
{
public:
    explicit ReelCamMediaStream(ReelCamMediaSource* pParent);

    HRESULT Initialize();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IMFMediaEventGenerator
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* pState) override;
    STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent) override;
    STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent) override;
    STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType,
                            HRESULT hrStatus, const PROPVARIANT* pvValue) override;

    // IMFMediaStream
    STDMETHODIMP GetMediaSource(IMFMediaSource** ppMediaSource) override;
    STDMETHODIMP GetStreamDescriptor(IMFStreamDescriptor** ppSD) override;
    STDMETHODIMP RequestSample(IUnknown* pToken) override;

    // Source-driven lifecycle
    HRESULT Start(const PROPVARIANT* pvarStartPosition);
    HRESULT Stop();
    HRESULT Pause();
    HRESULT Shutdown();

    // Retrieve stream descriptor (for building presentation descriptor)
    HRESULT GetStreamDescriptor(IMFStreamDescriptor** ppSD) const;

    // Video parameters
    static const UINT32 VIDEO_WIDTH  = 1920;
    static const UINT32 VIDEO_HEIGHT = 1080;
    static const UINT32 VIDEO_FPS    = 30;

private:
    ~ReelCamMediaStream();

    HRESULT CreateMediaType(IMFMediaType** ppType);
    HRESULT CreateStreamDescriptor();
    HRESULT GenerateTestPatternSample(IMFSample** ppSample);

    // Generates a single NV12 frame with SMPTE-like color bars
    void FillNV12ColorBars(BYTE* pData, UINT32 width, UINT32 height, UINT64 frameIndex);

    CRITICAL_SECTION                       m_cs;
    long                                   m_cRef;
    bool                                   m_bShutdown;
    bool                                   m_bActive;

    ReelCamMediaSource*                    m_pParent;      // weak ref, parent outlives us
    ReelCam::ComPtr<IMFMediaEventQueue>    m_pEventQueue;
    ReelCam::ComPtr<IMFStreamDescriptor>   m_pStreamDescriptor;

    UINT64                                 m_frameIndex;
    LARGE_INTEGER                          m_qpcFreq;
    LARGE_INTEGER                          m_qpcStart;
    ReelCam::SharedMemoryConsumer          m_shmConsumer;
};
