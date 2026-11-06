#include "window_state.hpp"

#include <algorithm>
#include <exception>
#include <map>
#include <string>
#include <utility>

#include "file_utils.hpp"

namespace taskmaster {

namespace {

// Читает число из поля JSON; false, если поля нет, оно строка или мусор.
bool read_int(const std::map<std::string, JsonValue>& fields, const char* key,
              int& out) {
    const auto found = fields.find(key);
    if (found == fields.end() || found->second.is_string) {
        return false;
    }
    const std::string& text = found->second.text;
    if (text.empty() || (text[0] == '-' && text.size() == 1)) {
        return false;
    }
    try {
        out = std::stoi(text);
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

}  // namespace

WindowGeometry load_window_geometry(const std::filesystem::path& state_file,
                                    int default_width, int default_height) {
    WindowGeometry result;
    const auto fields = read_json_object(state_file);

    read_int(fields, "width", result.width);
    read_int(fields, "height", result.height);
    if (result.width <= 0 || result.height <= 0 ||
        result.width < default_width || result.height < default_height) {
        result.width = default_width;
        result.height = default_height;
    }

    int x = 0;
    int y = 0;
    if (read_int(fields, "x", x) && read_int(fields, "y", y)) {
        result.x = x;
        result.y = y;
        result.has_position = true;
    }
    return result;
}

void save_window_geometry(const std::filesystem::path& state_file, int width,
                          int height, int x, int y) {
    std::map<std::string, JsonValue> fields;
    JsonValue value;
    value.text = std::to_string(width);
    fields["width"] = value;
    value.text = std::to_string(height);
    fields["height"] = value;
    value.text = std::to_string(x);
    fields["x"] = value;
    value.text = std::to_string(y);
    fields["y"] = value;
    write_json_object(state_file, fields);
}

}  // namespace taskmaster
