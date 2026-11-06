#pragma once

// Конвертация UTF-8 (формат данных на диске) <-> UTF-16 (QPushButton ... то есть Win32 API).

#include <string>

namespace tm {

std::wstring to_utf16(const std::string& utf8);
std::string to_utf8(const std::wstring& utf16);

}  // namespace tm
