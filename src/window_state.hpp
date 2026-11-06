#pragma once

// window_state.py 1:1: сохранение размера окна между запусками с валидацией данных.

#include <filesystem>

namespace tm {

// Возвращает (width, height); нечисловые и битые данные -> дефолт.
// Меньше дефолта (он же минимум) -> дефолт.
std::pair<int, int> load_window_size(const std::filesystem::path& state_file,
                                     int default_width, int default_height);

void save_window_size(const std::filesystem::path& state_file, int width, int height);

}  // namespace tm
