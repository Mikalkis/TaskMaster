#include "window_state.hpp"

#include <algorithm>
#include <exception>
#include <map>
#include <string>
#include <utility>

#include "file_utils.hpp"

namespace taskmaster {

std::pair<int, int> load_window_size(const std::filesystem::path& state_file,
                                     int default_width, int default_height) {
    const auto fields = read_json_object(state_file);

    int width = default_width;
    int height = default_height;

    const auto width_field = fields.find("width");
    const auto height_field = fields.find("height");
    if (width_field != fields.end() && !width_field->second.is_string &&
        height_field != fields.end() && !height_field->second.is_string) {
        // Значения в JSON — десятичные числа; прочее (пусто, мусор) уже отброшено парсером?
        // Парсер числа допускает "-" без цифр, поэтому проверяем.
        const std::string& width_text = width_field->second.text;
        const std::string& height_text = height_field->second.text;
        const bool width_ok = !width_text.empty() &&
                              (width_text[0] == '-' ? width_text.size() > 1
                                                    : width_text[0] >= '0' &&
                                                        width_text[0] <= '9');
        const bool height_ok = !height_text.empty() &&
                               (height_text[0] == '-' ? height_text.size() > 1
                                                      : height_text[0] >= '0' &&
                                                            height_text[0] <= '9');
        if (width_ok && height_ok) {
            try {
                width = std::stoi(width_text);
                height = std::stoi(height_text);
            } catch (const std::exception&) {
                width = default_width;
                height = default_height;
            }
        }
    }

    return {std::max(width, default_width), std::max(height, default_height)};
}

void save_window_size(const std::filesystem::path& state_file, int width, int height) {
    std::map<std::string, JsonValue> fields;
    JsonValue width_value;
    width_value.text = std::to_string(width);
    fields["width"] = width_value;
    JsonValue height_value;
    height_value.text = std::to_string(height);
    fields["height"] = height_value;
    write_json_object(state_file, fields);
}

}  // namespace taskmaster
