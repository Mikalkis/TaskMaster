#pragma once

// Атомарная запись текста и JSON: сначала временный файл в том же каталоге, потом replace.
// Гарантия как в питоновском file_utils.py: сбой записи не портит старый файл.

#include <filesystem>
#include <map>
#include <string>

namespace tm {

struct JsonValue {
    bool is_string = false;   // false — число
    std::string text;         // значение: строка без кавычек или число в десятичном виде
};

// Читает плоский JSON-объект {"key": value}. Неизвестные ключи сохраняются как есть.
// Значения поддерживаются: строка и целое число. Иначе — пустой объект.
std::map<std::string, JsonValue> read_json_object(const std::filesystem::path& path);

// Пишет плоский JSON-объект с отступом 2 (как json.dumps(..., indent=2)).
void write_json_object(const std::filesystem::path& path,
                       const std::map<std::string, JsonValue>& fields);

void write_text_atomic(const std::filesystem::path& path, const std::string& content);

}  // namespace tm
