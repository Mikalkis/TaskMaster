#pragma once

// window_state.py 1:1: сохранение геометрии окна между запусками с валидацией данных.

#include <filesystem>

namespace taskmaster {

struct WindowGeometry {
    int width = 0;    // 0 — данных в файле нет
    int height = 0;
    int x = 0;        // действительны только при has_position
    int y = 0;
    bool has_position = false;  // false — позицию не сохраняли/не разобрали
};

// Нечисловые и битые данные -> дефолт; меньше дефолта (он же минимум) -> дефолт.
WindowGeometry load_window_geometry(const std::filesystem::path& state_file,
                                    int default_width, int default_height);

void save_window_geometry(const std::filesystem::path& state_file, int width,
                          int height, int x, int y);

}  // namespace taskmaster
