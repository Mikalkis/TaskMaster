#include "file_utils.hpp"

#include <windows.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "text.hpp"

namespace taskmaster {

namespace {

// JSON-экранирование строки (упрощенно: достаточно для имен списков).
std::string json_escape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 8);
    for (const char c : raw) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                // Остальные байты — UTF-8, выводим как есть.
                out += c;
        }
    }
    return out;
}

bool parse_hex4(const std::string& text, size_t at, unsigned& out) {
    if (at + 4 > text.size()) {
        return false;
    }
    unsigned value = 0;
    for (size_t i = 0; i < 4; ++i) {
        const char c = text[at + i];
        unsigned digit;
        if (c >= '0' && c <= '9') {
            digit = static_cast<unsigned>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<unsigned>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<unsigned>(c - 'A' + 10);
        } else {
            return false;
        }
        value = value * 16 + digit;
    }
    out = value;
    return true;
}

void append_utf8(std::string& out, unsigned code) {
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

std::string json_unescape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        const char c = raw[i];
        if (c != '\\') {
            out += c;
            continue;
        }
        if (++i >= raw.size()) {
            break;
        }
        switch (raw[i]) {
            case '"':  out += '"';  break;
            case '\\': out += '\\'; break;
            case '/':  out += '/';  break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case 'u': {
                unsigned high = 0;
                if (parse_hex4(raw, i + 1, high)) {
                    i += 4;
                    // Суррогатная пара.
                    if (high >= 0xD800 && high <= 0xDBFF && i + 6 < raw.size() &&
                        raw[i + 1] == '\\' && raw[i + 2] == 'u') {
                        unsigned low = 0;
                        if (parse_hex4(raw, i + 3, low) && low >= 0xDC00 && low <= 0xDFFF) {
                            const unsigned combined =
                                0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00);
                            append_utf8(out, combined);
                            i += 6;
                            break;
                        }
                    }
                    append_utf8(out, high);
                }
                break;
            }
            default: out += raw[i]; break;
        }
    }
    return out;
}

}  // namespace

std::map<std::string, JsonValue> read_json_object(const std::filesystem::path& path) {
    std::map<std::string, JsonValue> result;

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return result;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    size_t i = 0;
    const auto skip_space = [&text, &i]() {
        while (i < text.size() &&
               (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r')) {
            ++i;
        }
    };

    skip_space();
    if (i >= text.size() || text[i] != '{') {
        return result;
    }
    ++i;

    while (true) {
        skip_space();
        if (i < text.size() && text[i] == '}') {
            break;
        }
        // Пары "ключ": значение.
        if (i >= text.size() || text[i] != '"') {
            return {};
        }
        const size_t key_end = text.find('"', i + 1);
        if (key_end == std::string::npos) {
            return {};
        }
        const std::string key = json_unescape(text.substr(i + 1, key_end - i - 1));
        i = key_end + 1;
        skip_space();
        if (i >= text.size() || text[i] != ':') {
            return {};
        }
        ++i;
        skip_space();

        if (i < text.size() && text[i] == '"') {
            size_t j = i + 1;
            while (j < text.size()) {
                if (text[j] == '\\' && j + 1 < text.size()) {
                    j += 2;
                    continue;
                }
                if (text[j] == '"') {
                    break;
                }
                ++j;
            }
            if (j >= text.size()) {
                return {};
            }
            JsonValue value;
            value.is_string = true;
            value.text = json_unescape(text.substr(i + 1, j - i - 1));
            result[key] = value;
            i = j + 1;
        } else if (i < text.size() && (text[i] == '-' ||
                                       (text[i] >= '0' && text[i] <= '9'))) {
            const size_t start = i;
            ++i;
            while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
                ++i;
            }
            JsonValue value;
            value.text = text.substr(start, i - start);
            result[key] = value;
        } else {
            return {};
        }

        skip_space();
        if (i < text.size() && text[i] == ',') {
            ++i;
        } else if (!(i < text.size() && text[i] == '}')) {
            return {};
        }
    }

    return result;
}

void write_json_object(const std::filesystem::path& path,
                       const std::map<std::string, JsonValue>& fields) {
    std::ostringstream out;
    out << "{\n";
    bool first = true;
    for (const auto& [key, value] : fields) {
        if (!first) {
            out << ",\n";
        }
        first = false;
        out << "  \"" << json_escape(key) << "\": ";
        if (value.is_string) {
            out << "\"" << json_escape(value.text) << "\"";
        } else {
            out << value.text;
        }
    }
    out << "\n}";
    write_text_atomic(path, out.str());
}

void write_text_atomic(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());

    // Временный файл в том же каталоге; имя уникально по времени+процессу.
    const std::wstring temp_name =
        path.filename().wstring() + L".tmp" + std::to_wstring(GetCurrentProcessId()) +
        L"_" + std::to_wstring(GetTickCount());
    const std::filesystem::path temp_path = path.parent_path() / temp_name;

    {
        std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
        if (!file) {
            throw std::runtime_error("Не удалось открыть файл для записи: " + path.string());
        }
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        file.flush();
        if (!file) {
            throw std::runtime_error("Не удалось записать файл: " + path.string());
        }
    }

    if (!MoveFileExW(temp_path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        throw std::runtime_error("Не удалось заменить файл: " + path.string());
    }
}

}  // namespace taskmaster
