#pragma once
// ReelCam HRESULT helpers — RAII and error-checking macros for COM/MF code.
// Every COM call in this project must be checked. No silent failures.

#include <windows.h>
#include <stdio.h>

// Return from function on failure
#define RETURN_IF_FAILED(hr)                                        \
    do {                                                            \
        HRESULT _hr = (hr);                                         \
        if (FAILED(_hr)) {                                          \
            ReelCamLogHR(_hr, __FILE__, __LINE__);                  \
            return _hr;                                             \
        }                                                           \
    } while (0)

// Break from loop on failure, setting hrOut
#define BREAK_IF_FAILED(hr, hrOut)                                  \
    if (FAILED(hr)) { (hrOut) = (hr); break; }

// Log HRESULT details to stderr (debug builds) and log file
inline void ReelCamLogHR(HRESULT hr, const char* file, int line)
{
    char msg[512];
    snprintf(msg, sizeof(msg),
        "[ReelCam] HRESULT 0x%08lX at %s:%d\n",
        static_cast<unsigned long>(hr), file, line);
    OutputDebugStringA(msg);
    fprintf(stderr, "%s", msg);
}
