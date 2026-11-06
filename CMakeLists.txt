# TaskMaster — Windows GUI-приложение (Win32 + RichEdit), без внешних зависимостей.

cmake_minimum_required(VERSION 3.15)
project(TaskMaster LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(TaskMaster WIN32
    src/main.cpp
    src/list_service.cpp
    src/storage.cpp
    src/task_utils.cpp
    src/window_state.cpp
    src/file_utils.cpp
    src/paths.cpp
    src/text.cpp
    resources.rc
)

# Программная запись UTF-8-литералов; MSVC по умолчанию читает исходники в локальной кодировке.
target_compile_definitions(TaskMaster PRIVATE UNICODE _UNICODE)
if(MSVC)
    target_compile_options(TaskMaster PRIVATE /utf-8 /W4)
endif()

target_link_libraries(TaskMaster PRIVATE user32 gdi32 comctl32)

# Консольный прогон логики без GUI (замена питоновских тестов).
add_executable(test_core
    src/test_core.cpp
    src/storage.cpp
    src/task_utils.cpp
    src/window_state.cpp
    src/file_utils.cpp
    src/paths.cpp
    src/text.cpp
)
if(MSVC)
    target_compile_options(test_core PRIVATE /utf-8 /W4)
endif()
