#pragma once

// list_service.py 1:1: владеет активным списком; слой между хранением и GUI.

#include <string>
#include <vector>

#include "storage.hpp"

namespace tm {

class ListService {
public:
    explicit ListService(TaskStorage& storage);

    std::vector<std::string> get_active_tasks() const;
    void save_active_tasks(const std::vector<std::string>& tasks) const;

    const std::vector<std::string>& list_names() const { return list_names_; }
    std::vector<std::string>& refresh_list_names();
    const std::string& active_list_name() const { return active_list_name_; }

    // Переключает активный список и возвращает его задачи.
    std::vector<std::string> switch_to(const std::string& new_list_name);

    // Возвращает очищенное имя нового списка.
    std::string create(const std::string& new_list_name);

private:
    TaskStorage& storage_;
    std::string active_list_name_;
    std::vector<std::string> list_names_;
};

}  // namespace tm
