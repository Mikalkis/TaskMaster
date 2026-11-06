#pragma once

// GUI на Win32 (аналог gui.py TodoApp на Tkinter); main.cpp — точка входа и вся реализация.
// Поля открытые намеренно: модуль один файл, логика реализована свободными функциями main.cpp.

#include <memory>
#include <string>
#include <vector>
#include <windows.h>
#include <richedit.h>

namespace tm {

class TaskStorage;
class ListService;

struct TodoApp {
public:
    TodoApp(HINSTANCE instance);
    ~TodoApp();

    // Показывает окно и крутит цикл сообщений; возвращает код выхода.
    int run(int show_command);

    std::unique_ptr<TaskStorage> storage;
    std::unique_ptr<ListService> list_service;
    std::vector<std::string> tasks;           // сохраненное состояние активного списка

    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HWND list_combo = nullptr;
    HWND tasks_edit = nullptr;
    HWND title_label = nullptr;
    HWND tooltip_control = nullptr;
    HACCEL accelerator = nullptr;
    HFONT title_font = nullptr;
    HFONT edit_font = nullptr;
    HFONT emoji_button_font = nullptr;
    HFONT plus_button_font = nullptr;

    CHARRANGE random_range{}; // текущая подсветка случайной задачи

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam,
                                        LPARAM lparam);
    LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam);
};

}  // namespace tm
