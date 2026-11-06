#include "storage.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "file_utils.hpp"
#include "task_utils.hpp"
#include "text.hpp"

namespace tm {

namespace {

const char DEFAULT_LIST_NAME[] = "default";
const char LEGACY_TASKS_FILE[] = "tasks.txt";
const char LISTS_DIR[] = "lists";
const char STATE_FILE[] = "app_state.json";

bool is_illegal_name_char(char c) {
    return c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
           c == '<' || c == '>' || c == '|';
}

std::vector<std::string> read_lines_skip_empty(const std::filesystem::path& file) {
    std::vector<std::string> tasks;
    std::ifstream file_stream(file, std::ios::binary);
    if (!file_stream) {
        return tasks;
    }
    std::ostringstream buffer;
    buffer << file_stream.rdbuf();
    std::string content = buffer.str();

    std::vector<std::string> lines;
    split_lines(content, lines);
    for (const std::string& line : lines) {
        std::string task = line;
        // срезаем внешние пробелы, как в питоновской версии
        const size_t first = task.find_first_not_of(" \t\r");
        if (std::string::npos != first) {
            const size_t last = task.find_last_not_of(" \t\r");
            task = task.substr(first, last - first + 1);
            tasks.push_back(task);
        }
    }
    return tasks;
}

}  // namespace

std::string sanitize_list_name(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Имя списка не должно быть пустым.");
    }
    size_t first = 0;
    size_t last = name.size();
    while (first < name.size() && name[first] == ' ') {
        ++first;
    }
    while (last > first && name[last - 1] == ' ') {
        --last;
    }
    const std::string normalized = name.substr(first, last - first);
    if (normalized.empty()) {
        throw std::invalid_argument("Имя списка не должно быть пустым.");
    }
    for (const char c : normalized) {
        if (is_illegal_name_char(c)) {
            throw std::invalid_argument("Имя списка содержит недопустимые символы.");
        }
    }
    return normalized;
}

TaskStorage::TaskStorage(std::filesystem::path base_dir)
    : base_dir_(std::move(base_dir)),
      lists_dir_(base_dir_ / LISTS_DIR),
      legacy_tasks_file_(base_dir_ / LEGACY_TASKS_FILE),
      state_file_(base_dir_ / STATE_FILE) {}

std::filesystem::path TaskStorage::list_file(const std::string& name) const {
    const std::string safe_name = sanitize_list_name(name);
    return lists_dir_ / std::filesystem::path(to_utf16(safe_name + ".txt"));
}

void TaskStorage::ensure_ready() {
    std::error_code ignored;
    std::filesystem::create_directories(lists_dir_, ignored);

    // Уже есть списки — ничего не делаем (идемпотентно).
    bool has_lists = false;
    for (const auto& entry : std::filesystem::directory_iterator(lists_dir_)) {
        if (entry.path().extension() == L".txt") {
            has_lists = true;
            break;
        }
    }
    if (has_lists) {
        return;
    }

    // Миграция со старого одиночного файла tasks.txt.
    const std::filesystem::path default_file =
        lists_dir_ / std::filesystem::path(to_utf16(std::string(DEFAULT_LIST_NAME) + ".txt"));
    std::string content;
    if (std::filesystem::exists(legacy_tasks_file_)) {
        std::ifstream legacy(legacy_tasks_file_, std::ios::binary);
        std::ostringstream buffer;
        buffer << legacy.rdbuf();
        content = buffer.str();
    }
    write_text_atomic(default_file, content);
}

std::vector<std::string> TaskStorage::list_names() {
    ensure_ready();
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(lists_dir_)) {
        if (entry.path().extension() == L".txt") {
            names.push_back(to_utf8(entry.path().filename().replace_extension().wstring()));
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string TaskStorage::get_active_name() {
    ensure_ready();
    const std::vector<std::string> available = list_names();

    std::string active_name;
    const auto state = read_json_object(state_file_);
    const auto found = state.find("active_list");
    if (found != state.end() && found->second.is_string) {
        active_name = found->second.text;
    } else {
        active_name = DEFAULT_LIST_NAME;
    }

    const bool exists = std::find(available.begin(), available.end(), active_name) !=
                        available.end();
    if (!exists) {
        active_name = available.at(0);
        set_active_name(active_name);
    }
    return active_name;
}

void TaskStorage::set_active_name(const std::string& name) {
    ensure_ready();
    const std::string safe_name = sanitize_list_name(name);
    if (!std::filesystem::exists(list_file(safe_name))) {
        throw std::runtime_error("Список '" + safe_name + "' не найден.");
    }

    std::map<std::string, JsonValue> state;
    JsonValue value;
    value.is_string = true;
    value.text = safe_name;
    state["active_list"] = value;
    write_json_object(state_file_, state);
}

std::string TaskStorage::create_list(const std::string& name) {
    ensure_ready();
    const std::string safe_name = sanitize_list_name(name);
    const std::filesystem::path file = list_file(safe_name);
    if (std::filesystem::exists(file)) {
        throw std::invalid_argument("Список с таким именем уже существует.");
    }
    write_text_atomic(file, "");
    return safe_name;
}

std::vector<std::string> TaskStorage::load_tasks(const std::string& name) {
    ensure_ready();
    const std::string target = name.empty() ? get_active_name() : name;
    return read_lines_skip_empty(list_file(target));
}

void TaskStorage::save_tasks(const std::vector<std::string>& tasks, const std::string& name) {
    ensure_ready();
    const std::string target = name.empty() ? get_active_name() : name;
    std::string content;
    if (!tasks.empty()) {
        for (const std::string& task : tasks) {
            content += task;
            content += "\n";
        }
    }
    write_text_atomic(list_file(target), content);
}

}  // namespace tm
