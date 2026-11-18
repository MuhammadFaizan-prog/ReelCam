#pragma once
// ReelCamMediaSource: IMFMediaSource implementation for the ReelCam virtual camera.
// This COM object is loaded by consuming apps (Chrome, Zoom, etc.) via Frame Server.
// It owns one video stream that delivers NV12 frames.

#include <windows.h>
#include <mfidl.h>
#include <mfapi.h>
#include <mfobjects.h>
#include <mferror.h>

#include "ComPtr.h"

class ReelCamMediaStream; // forward

class ReelCamMediaSource
    : public IMFMediaSource
    , public IMFGetService
{
public:
    ReelCamMediaSource();

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

    // IMFMediaSource
    STDMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor** ppPD) override;
    STDMETHODIMP GetCharacteristics(DWORD* pdwCharacteristics) override;
    STDMETHODIMP Pause() override;
    STDMETHODIMP Shutdown() override;
    STDMETHODIMP Start(IMFPresentationDescriptor* pPD, const GUID* pguidTimeFormat,
                       const PROPVARIANT* pvarStartPosition) override;
    STDMETHODIMP Stop() override;

    // IMFGetService
    STDMETHODIMP GetService(REFGUID guidService, REFIID riid, LPVOID* ppvObject) override;

    // Internal
    HRESULT QueueNewStreamEvent(IMFPresentationDescriptor* pPD);

private:
    ~ReelCamMediaSource();

    CRITICAL_SECTION                       m_cs;
    long                                   m_cRef;
    bool                                   m_bShutdown;
    ReelCam::ComPtr<IMFMediaEventQueue>    m_pEventQueue;
    ReelCam::ComPtr<IMFPresentationDescriptor> m_pPresentationDescriptor;
    ReelCamMediaStream*                    m_pStream;      // Raw ptr, prevent circular ref
};
