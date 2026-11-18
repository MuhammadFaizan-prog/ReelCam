#pragma once
// Minimal COM smart pointer. We use this instead of wrl::ComPtr to avoid
// WRL header dependency issues with older SDKs. Same RAII semantics.

#include <windows.h>

namespace ReelCam {

template<typename T>
class ComPtr {
public:
    ComPtr() : m_ptr(nullptr) {}
    ComPtr(T* p) : m_ptr(p) { if (m_ptr) m_ptr->AddRef(); }
    ComPtr(const ComPtr& other) : m_ptr(other.m_ptr) { if (m_ptr) m_ptr->AddRef(); }
    ComPtr(ComPtr&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }

    ~ComPtr() { Release(); }

    ComPtr& operator=(const ComPtr& other) {
        if (this != &other) {
            Release();
            m_ptr = other.m_ptr;
            if (m_ptr) m_ptr->AddRef();
        }
        return *this;
    }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Release();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    void Release() {
        if (m_ptr) {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }
    void Reset() { Release(); }

    T* Get() const { return m_ptr; }
    T** GetAddressOf() { return &m_ptr; }
    T** ReleaseAndGetAddressOf() { Release(); return &m_ptr; }

    T* operator->() const { return m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }

    bool operator==(std::nullptr_t) const { return m_ptr == nullptr; }
    bool operator!=(std::nullptr_t) const { return m_ptr != nullptr; }
    bool operator==(const ComPtr& other) const { return m_ptr == other.m_ptr; }
    bool operator!=(const ComPtr& other) const { return m_ptr != other.m_ptr; }

    // For passing to functions that take void**
    void** PutVoid() { Release(); return reinterpret_cast<void**>(&m_ptr); }

    // Attach without AddRef (takes ownership)
    void Attach(T* p) { Release(); m_ptr = p; }

    // Detach without Release (gives up ownership)
    T* Detach() { T* p = m_ptr; m_ptr = nullptr; return p; }

    HRESULT As(REFIID riid, void** ppv) const {
        if (!m_ptr) return E_POINTER;
        return m_ptr->QueryInterface(riid, ppv);
    }

private:
    T* m_ptr;
};

} // namespace ReelCam
