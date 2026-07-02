#pragma once

#include <windows.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

inline std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], n);
    return ws;
}

inline std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

inline std::string EscapeJson(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:   r += c;
        }
    }
    return r;
}

inline int ClampVolume(int volume) {
    return (std::max)(0, (std::min)(100, volume));
}

inline int ScalarToVolume(float scalar) {
    return ClampVolume(static_cast<int>(std::round(scalar * 100.0f)));
}

inline float VolumeToScalar(int volume) {
    return ClampVolume(volume) / 100.0f;
}

inline bool IsSameGuid(const GUID& a, const GUID& b) {
    return IsEqualGUID(a, b) != 0;
}

inline std::string FormatHResult(HRESULT hr) {
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
        << static_cast<unsigned long>(static_cast<unsigned int>(hr));
    return oss.str();
}

inline std::string HResultMessage(const std::string& message, HRESULT hr) {
    return message + "（" + FormatHResult(hr) + "）";
}

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    explicit ComPtr(T* ptr) : ptr_(ptr) {}
    ~ComPtr() { reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T* get() const { return ptr_; }
    T** put() {
        reset();
        return &ptr_;
    }
    T* detach() {
        T* ptr = ptr_;
        ptr_ = nullptr;
        return ptr;
    }
    void reset(T* ptr = nullptr) {
        if (ptr_) ptr_->Release();
        ptr_ = ptr;
    }
    T* operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

private:
    T* ptr_ = nullptr;
};

inline const GUID kAudioFluxVolumeContext =
    {0x4f23ec26, 0x66ba, 0x4c9f, {0x96, 0x27, 0x7d, 0x97, 0xf2, 0x57, 0xd9, 0x5b}};
