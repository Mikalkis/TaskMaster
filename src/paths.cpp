#include "paths.hpp"

#include <windows.h>

namespace taskmaster {

std::filesystem::path get_base_dir() {
    WCHAR exe_path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    // Вскользь: PathRemoveFileSpecW тянуть не хотим, режем вручную.
    std::filesystem::path exe(exe_path);
    return exe.parent_path();
}

}  // namespace taskmaster
