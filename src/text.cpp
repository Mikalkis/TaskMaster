#include "text.hpp"

#include <windows.h>

namespace taskmaster {

std::wstring to_utf16(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()),
                        result.data(), length);
    return result;
}

std::string to_utf8(const std::wstring& utf16) {
    if (utf16.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(
        CP_UTF8, 0, utf16.c_str(), static_cast<int>(utf16.size()),
        nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), static_cast<int>(utf16.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

}  // namespace taskmaster
