#pragma once

// Каталог данных приложения: рядом с exe (как в питоновском paths.py при сборке PyInstaller).

#include <filesystem>

namespace tm {

std::filesystem::path get_base_dir();

}  // namespace tm
