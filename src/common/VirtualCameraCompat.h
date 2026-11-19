#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <devpropdef.h>

// ReelCam Media Source CLSID: {E7B1A280-9A23-4560-BD92-358F58A009C1}
// {E7B1A280-9A23-4560-BD92-358F58A009C1}
static const GUID CLSID_ReelCamMediaSource = 
{ 0xe7b1a280, 0x9a23, 0x4560, { 0xbd, 0x92, 0x35, 0x8f, 0x58, 0xa0, 0x9, 0xc1 } };

#define REELCAM_CLSID_STRING L"{E7B1A280-9A23-4560-BD92-358F58A009C1}"
#define REELCAM_DEVICE_FRIENDLY_NAME L"ReelCam"

// KSCATEGORY_VIDEO_CAMERA: {E5323777-F976-4F5B-9B55-B94699C46E44}
static const GUID KSCATEGORY_VIDEO_CAMERA_GUID = 
{ 0xe5323777, 0xf976, 0x4f5b, { 0x9b, 0x55, 0xb9, 0x46, 0x99, 0xc4, 0x6e, 0x44 } };

// KSCATEGORY_CAPTURE: {65E8773D-8F56-11D0-A3B9-00A0C9223196}
static const GUID KSCATEGORY_CAPTURE_GUID = 
{ 0x65e8773d, 0x8f56, 0x11d0, { 0xa3, 0xb9, 0x00, 0xa0, 0xc9, 0x22, 0x31, 0x96 } };

// Windows 11 Virtual Camera Media Foundation Definitions
#ifndef MF_VIRTUALCAMERA_DEFINED
#define MF_VIRTUALCAMERA_DEFINED

typedef enum _MFVirtualCameraType {
    MFVirtualCameraType_SoftwareCameraSource = 0
} MFVirtualCameraType;

typedef enum _MFVirtualCameraLifetime {
    MFVirtualCameraLifetime_Session = 0,
    MFVirtualCameraLifetime_System  = 1
} MFVirtualCameraLifetime;

#define MFVirtualCameraLifetime_User MFVirtualCameraLifetime_System

typedef enum _MFVirtualCameraAccess {
    MFVirtualCameraAccess_CurrentUser = 0,
    MFVirtualCameraAccess_AllUsers    = 1
} MFVirtualCameraAccess;

typedef ULONG DEVPROP_TYPE;

// IMFVirtualCamera interface definition (Windows 11 Build 22000+)
// UUID: 1C08A864-EF6C-4C75-AF59-5F2D68DA9563
MIDL_INTERFACE("1C08A864-EF6C-4C75-AF59-5F2D68DA9563")
IMFVirtualCamera : public IMFAttributes
{
public:
    virtual HRESULT STDMETHODCALLTYPE AddDeviceSourceInfo(
        /* [in] */ LPCWSTR symbolicName) = 0;

    virtual HRESULT STDMETHODCALLTYPE AddProperty(
        /* [in] */ const DEVPROPKEY *pKey,
        /* [in] */ DEVPROP_TYPE Type,
        /* [in] */ const BYTE *pbData,
        /* [in] */ UINT32 cbData) = 0;

    virtual HRESULT STDMETHODCALLTYPE AddRegistryEntry(
        /* [in] */ LPCWSTR EntryName,
        /* [in] */ LPCWSTR SubkeyPath,
        /* [in] */ DWORD dwRegType,
        /* [in] */ const BYTE *pbData,
        /* [in] */ DWORD cbData) = 0;

    virtual HRESULT STDMETHODCALLTYPE Start(
        /* [in] */ IMFAsyncCallback *pCallback) = 0;

    virtual HRESULT STDMETHODCALLTYPE Stop(void) = 0;

    virtual HRESULT STDMETHODCALLTYPE Remove(void) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetMediaSource(
        /* [out] */ IMFMediaSource **ppMediaSource) = 0;

    virtual HRESULT STDMETHODCALLTYPE SendCameraProperty(
        /* [in] */ REFGUID guidPropertySet,
        /* [in] */ ULONG nPropertyID,
        /* [in] */ ULONG nFlags,
        /* [in] */ void *pData,
        /* [in] */ ULONG nDataLength,
        /* [in] */ void *pQualifier,
        /* [in] */ ULONG nQualifierLength) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateSyncEvent(
        /* [in] */ REFGUID guidEvent,
        /* [out] */ HANDLE *phEvent) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateSyncSemaphore(
        /* [in] */ REFGUID guidSemaphore,
        /* [out] */ HANDLE *phSemaphore,
        /* [out] */ LONG *plInitialCount,
        /* [out] */ LONG *plMaximumCount) = 0;

    virtual HRESULT STDMETHODCALLTYPE Shutdown(void) = 0;
};

typedef HRESULT (STDAPICALLTYPE *PFN_MFCreateVirtualCamera)(
    MFVirtualCameraType type,
    MFVirtualCameraLifetime lifetime,
    MFVirtualCameraAccess access,
    LPCWSTR pszFriendlyName,
    LPCWSTR pszSourceId,
    const GUID *pCategories,
    ULONG cCategories,
    IMFVirtualCamera **ppVirtualCamera
);

typedef HRESULT (STDAPICALLTYPE *PFN_MFIsVirtualCameraTypeSupported)(
    MFVirtualCameraType type,
    BOOL *pIsSupported
);

#endif // MF_VIRTUALCAMERA_DEFINED
