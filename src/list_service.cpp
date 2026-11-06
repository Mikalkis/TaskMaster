#include "list_service.hpp"

namespace tm {

ListService::ListService(TaskStorage& storage)
    : storage_(storage),
      active_list_name_(storage.get_active_name()),
      list_names_(storage.list_names()) {}

std::vector<std::string> ListService::get_active_tasks() const {
    return storage_.load_tasks(active_list_name_);
}

void ListService::save_active_tasks(const std::vector<std::string>& tasks) const {
    storage_.save_tasks(tasks, active_list_name_);
}

std::vector<std::string>& ListService::refresh_list_names() {
    list_names_ = storage_.list_names();
    return list_names_;
}

std::vector<std::string> ListService::switch_to(const std::string& new_list_name) {
    if (new_list_name == active_list_name_) {
        return get_active_tasks();
    }
    storage_.set_active_name(new_list_name);
    active_list_name_ = new_list_name;
    refresh_list_names();
    return storage_.load_tasks(new_list_name);
}

std::string ListService::create(const std::string& new_list_name) {
    const std::string created_name = storage_.create_list(new_list_name);
    refresh_list_names();
    return created_name;
}

}  // namespace tm
