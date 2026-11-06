#pragma once

// storage.py 1:1: каталог lists/ + app_state.json; все операции падают исключением при некорректных данных.

#include <filesystem>
#include <string>
#include <vector>

namespace taskmaster {

std::string sanitize_list_name(const std::string& name);   // бросает std::invalid_argument

class TaskStorage {
public:
    explicit TaskStorage(std::filesystem::path base_dir);

    // Создает структуру каталогов и default-список; безопасно вызывать повторно.
    void ensure_ready();

    std::vector<std::string> list_names();
    std::string get_active_name();
    void set_active_name(const std::string& name);          // бросает std::runtime_error, если списка нет
    std::string create_list(const std::string& name);       // бросает std::invalid_argument при дубле

    // Имя списка пустое ("") — работаем с активным списком.
    std::vector<std::string> load_tasks(const std::string& name = "");
    void save_tasks(const std::vector<std::string>& tasks, const std::string& name = "");

private:
    std::filesystem::path list_file(const std::string& name) const;

    std::filesystem::path base_dir_;
    std::filesystem::path lists_dir_;
    std::filesystem::path legacy_tasks_file_;
    std::filesystem::path state_file_;
};

}  // namespace taskmaster
