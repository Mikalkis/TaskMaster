// TaskMaster: точка входа и вся GUI-логика (аналог gui.py на Tkinter из питоновской версии).

#include "gui.hpp"

#include <commctrl.h>
#include <richedit.h>

#include <algorithm>
#include <optional>
#include <random>
#include <utility>

#include "list_service.hpp"
#include "paths.hpp"
#include "storage.hpp"
#include "task_utils.hpp"
#include "text.hpp"
#include "window_state.hpp"

namespace taskmaster {

namespace {

// Идентификаторы контролов и команд главного окна.
constexpr int IDC_LIST_COMBO = 101;
constexpr int IDC_BTN_NEW_LIST = 102;
constexpr int IDC_BTN_SAVE = 103;
constexpr int IDC_BTN_DONE = 104;
constexpr int IDC_BTN_RANDOM = 105;
constexpr int IDC_TASKS_EDIT = 106;
constexpr int IDC_TITLE_LABEL = 107;
constexpr int IDM_SAVE = 201;

// Идентификаторы контролов окна «Новый список»; IDOK/IDCANCEL — стандартные (1/2).
constexpr int IDC_NEW_NAME_EDIT = 301;

constexpr int DEFAULT_WIDTH = 320;
constexpr int DEFAULT_HEIGHT = 400;

// Цвета как в питоновской версии.
constexpr COLORREF DONE_COLOR = RGB(0x1F, 0x9D, 0x3A);
constexpr COLORREF RANDOM_HIGHLIGHT = RGB(0xFF, 0xF2, 0xA8);

constexpr wchar_t WINDOW_CLASS[] = L"TaskMasterMainWindow";
constexpr wchar_t WINDOW_TITLE[] = L"TaskMaster";
constexpr wchar_t DIALOG_CLASS[] = L"TaskMasterListDialog";

HFONT make_font(const wchar_t* face, int height, bool bold) {
    return CreateFontW(-height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE,
                       FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, face);
}

// Текст RichEdit через GETTEXTEX: UTF-8, разделитель строк \r\n.
std::string get_edit_text(HWND edit) {
    const int length = static_cast<int>(SendMessageW(edit, WM_GETTEXTLENGTH, 0, 0));
    const int capacity = (length + 2) * 3 + 16;
    std::string content(static_cast<size_t>(capacity), '\0');
    GETTEXTEX params = {};
    params.cb = static_cast<DWORD>(capacity);
    params.flags = GT_USECRLF;
    params.codepage = CP_UTF8;
    const size_t written =
        SendMessageW(edit, EM_GETTEXTEX, reinterpret_cast<WPARAM>(&params),
                     reinterpret_cast<LPARAM>(content.data()));
    if (written > 0) {
        // written — число записанных байтов + завершающий нуль.
        content.resize(written);
    } else {
        content.clear();
    }
    while (!content.empty() && content.back() == '\0') {
        content.pop_back();
    }
    return content;
}

}  // namespace

// -----------------------------------------------------------------------

namespace {

// Полная перерисовка поля задач: нумерация и зелёный цвет выполненных задач.
void render_numbered_tasks(HWND edit, const std::vector<std::string>& tasks) {
    std::string rendered;
    std::vector<int> line_length_utf16;  // длина "N. задача" без \r\n
    for (size_t i = 0; i < tasks.size(); ++i) {
        const std::string line = std::to_string(i + 1) + ". " + tasks[i];
        line_length_utf16.push_back(static_cast<int>(to_utf16(line).size()));
        rendered += line;
        rendered += "\r\n";
    }

    SendMessageW(edit, WM_SETREDRAW, FALSE, 0);
    SendMessageW(edit, WM_SETTEXT, 0,
                 reinterpret_cast<LPARAM>(to_utf16(rendered).c_str()));

    const int text_length =
        static_cast<int>(SendMessageW(edit, WM_GETTEXTLENGTH, 0, 0));

    // Сброс оформления по всему тексту: фон прозрачный, цвет системный.
    CHARFORMAT2W reset_format = {};
    reset_format.cbSize = sizeof(reset_format);
    reset_format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
    reset_format.dwEffects = CFE_AUTOBACKCOLOR;
    reset_format.crTextColor = GetSysColor(COLOR_WINDOWTEXT);
    CHARRANGE whole_range{0, text_length};
    SendMessageW(edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&whole_range));
    SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION,
                 reinterpret_cast<LPARAM>(&reset_format));

    // Зелёный для выполненных задач.
    CHARFORMAT2W done_format = {};
    done_format.cbSize = sizeof(done_format);
    done_format.dwMask = CFM_COLOR;
    done_format.crTextColor = DONE_COLOR;

    int offset = 0;
    for (size_t i = 0; i < tasks.size(); ++i) {
        const int length = line_length_utf16[i];
        const int line_end = offset + length;   // до \r
        offset += length + 2;                   // \r\n
        if (is_task_done(tasks[i])) {
            CHARRANGE range{line_end - length, line_end};
            SendMessageW(edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
            SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION,
                         reinterpret_cast<LPARAM>(&done_format));
        }
    }

    CHARRANGE caret_end{text_length, text_length};
    SendMessageW(edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&caret_end));

    SendMessageW(edit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(edit, nullptr, TRUE);
}

// Контекст модального окна «Новый список».
struct ListDialogState {
    bool finished = false;
    bool confirmed = false;
    std::wstring value;
};

LRESULT CALLBACK list_dialog_proc(HWND dialog, UINT message, WPARAM wparam,
                                  LPARAM lparam) {
    if (WM_NCCREATE == message) {
        auto* state = static_cast<ListDialogState*>(
            reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return DefWindowProcW(dialog, message, wparam, lparam);
    }
    auto* state = reinterpret_cast<ListDialogState*>(
        GetWindowLongPtrW(dialog, GWLP_USERDATA));
    if (nullptr == state) {
        return DefWindowProcW(dialog, message, wparam, lparam);
    }

    switch (message) {
        case WM_COMMAND:
            if (LOWORD(wparam) == IDOK) {
                const int length =
                    GetWindowTextLengthW(GetDlgItem(dialog, IDC_NEW_NAME_EDIT));
                std::wstring text(static_cast<size_t>(length) + 1, L'\0');
                GetWindowTextW(GetDlgItem(dialog, IDC_NEW_NAME_EDIT), text.data(),
                               static_cast<int>(text.size()));
                state->value.assign(text.c_str());
                state->confirmed = true;
                state->finished = true;
                DestroyWindow(dialog);
                return 0;
            }
            if (LOWORD(wparam) == IDCANCEL) {
                state->finished = true;
                DestroyWindow(dialog);
                return 0;
            }
            break;
        case WM_CLOSE:
            state->finished = true;
            DestroyWindow(dialog);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(dialog, message, wparam, lparam);
}

}  // namespace

// -----------------------------------------------------------------------

// Читаем задачи из текстового поля (один в один с питоновским get_tasks_with_text_lines).
std::vector<LineTask> get_tasks_with_text_lines(TodoApp& app) {
    std::vector<std::string> lines;
    split_lines(get_edit_text(app.tasks_edit), lines);

    std::vector<LineTask> result;
    for (size_t i = 0; i < lines.size(); ++i) {
        const int line_number = static_cast<int>(i) + 1;  // 1-based как в tkinter
        const std::string task = normalize_task_line(lines[i]);
        if (!task.empty()) {
            result.push_back({line_number, task});
        }
    }
    return result;
}

std::vector<std::string> get_tasks_from_text(TodoApp& app) {
    const auto with_lines = get_tasks_with_text_lines(app);
    std::vector<std::string> tasks;
    tasks.reserve(with_lines.size());
    for (const LineTask& item : with_lines) {
        tasks.push_back(item.text);
    }
    return tasks;
}

bool has_unsaved_changes(TodoApp& app) {
    return get_tasks_from_text(app) != app.tasks;
}

void save_current_tasks(TodoApp& app) {
    app.tasks = get_tasks_from_text(app);
    app.list_service->save_active_tasks(app.tasks);
    render_numbered_tasks(app.tasks_edit, app.tasks);
    app.random_range = {0, 0};
}

// Сохранение по прямому действию пользователя: как в питоне — с сообщением.
void save_with_message(TodoApp& app) {
    save_current_tasks(app);
    const std::wstring message =
        L"Изменения сохранены в '" + to_utf16(app.list_service->active_list_name()) +
        L"'.";
    MessageBoxW(app.window, message.c_str(), L"Сохранено",
                MB_OK | MB_ICONINFORMATION);
}

// true — можно продолжать; false — пользователь отменил (Cancel).
bool confirm_unsaved_changes(TodoApp& app, const wchar_t* message) {
    const int answer = MessageBoxW(app.window, message, L"Несохраненные изменения",
                                   MB_YESNOCANCEL | MB_ICONQUESTION);
    if (IDYES == answer) {
        save_current_tasks(app);
        return true;
    }
    return IDCANCEL != answer;
}

int find_combobox_index(TodoApp& app, const std::string& name) {
    const auto& names = app.list_service->list_names();
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) {
            return static_cast<int>(i);
        }
    }
    return CB_ERR;
}

void refresh_list_combobox(TodoApp& app) {
    const auto& names = app.list_service->refresh_list_names();
    SendMessageW(app.list_combo, CB_RESETCONTENT, 0, 0);
    for (const std::string& name : names) {
        SendMessageW(app.list_combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(to_utf16(name).c_str()));
    }
}

void restore_combobox_selection(TodoApp& app) {
    const int index = find_combobox_index(app, app.list_service->active_list_name());
    SendMessageW(app.list_combo, CB_SETCURSEL,
                 index == CB_ERR ? 0 : static_cast<WPARAM>(index), 0);
}

void switch_active_list(TodoApp& app, const std::string& new_list_name) {
    if (new_list_name == app.list_service->active_list_name()) {
        return;
    }
    if (has_unsaved_changes(app) &&
        !confirm_unsaved_changes(app,
                                 L"Сохранить текущий список перед переключением?")) {
        restore_combobox_selection(app);
        return;
    }
    app.tasks = app.list_service->switch_to(new_list_name);
    render_numbered_tasks(app.tasks_edit, app.tasks);
    app.random_range = {0, 0};
    restore_combobox_selection(app);
}

void on_list_selected(TodoApp& app) {
    const int index = static_cast<int>(SendMessageW(app.list_combo, CB_GETCURSEL, 0, 0));
    const auto& names = app.list_service->list_names();
    if (index == CB_ERR || index < 0 || static_cast<size_t>(index) >= names.size()) {
        restore_combobox_selection(app);
        return;
    }
    switch_active_list(app, names[static_cast<size_t>(index)]);
}

void mark_task_done(TodoApp& app) {
    CHARRANGE selection{};
    SendMessageW(app.tasks_edit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const int caret_char = selection.cpMax;
    const LRESULT line_index =
        SendMessageW(app.tasks_edit, EM_EXLINEFROMCHAR, 0, caret_char);
    const int cursor_line = static_cast<int>(line_index) + 1;  // 1-based
    const int cursor_col = caret_char -
        static_cast<int>(SendMessageW(app.tasks_edit, EM_LINEINDEX, line_index, 0));

    const auto tasks_with_lines = get_tasks_with_text_lines(app);
    const int task_index = find_task_index(tasks_with_lines, cursor_line);
    if (task_index < 0) {
        MessageBoxW(app.window, L"Поставьте курсор на строку с задачей.",
                    L"Нет задачи", MB_OK | MB_ICONWARNING);
        return;
    }

    // Читаем из поля, а не из app.tasks: иначе ✅ затрет несохраненные правки.
    app.tasks.clear();
    app.tasks.reserve(tasks_with_lines.size());
    for (const LineTask& item : tasks_with_lines) {
        app.tasks.push_back(item.text);
    }
    app.tasks[static_cast<size_t>(task_index)] =
        toggle_done_mark(app.tasks[static_cast<size_t>(task_index)]);

    app.list_service->save_active_tasks(app.tasks);
    render_numbered_tasks(app.tasks_edit, app.tasks);
    app.random_range = {0, 0};

    // Возврат курсора в то же место (аналог restore_cursor питоновской версии).
    const int line_count = static_cast<int>(
        SendMessageW(app.tasks_edit, EM_GETLINECOUNT, 0, 0));
    int safe_line_index = cursor_line - 1;
    if (safe_line_index >= line_count) {
        safe_line_index = line_count - 1;
    }
    if (safe_line_index < 0) {
        safe_line_index = 0;
    }
    int safe_char = static_cast<int>(
        SendMessageW(app.tasks_edit, EM_LINEINDEX, safe_line_index, 0));
    if (safe_char < 0) {
        safe_char = 0;
    }
    const int line_length = static_cast<int>(
        SendMessageW(app.tasks_edit, EM_LINELENGTH, safe_char, 0));
    const int safe_col = std::min(cursor_col, line_length);
    CHARRANGE caret{safe_char + safe_col, safe_char + safe_col};
    SendMessageW(app.tasks_edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&caret));
    SendMessageW(app.tasks_edit, EM_SCROLLCARET, 0, 0);
    SetFocus(app.tasks_edit);
}

void pick_random_task(TodoApp& app) {
    const auto tasks_with_lines = get_tasks_with_text_lines(app);
    std::vector<LineTask> not_done;
    for (const LineTask& item : tasks_with_lines) {
        if (!is_task_done(item.text)) {
            not_done.push_back(item);
        }
    }
    if (not_done.empty()) {
        MessageBoxW(app.window, L"Нет невыполненных задач.", L"Случайный выбор",
                    MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<size_t> distribution(0, not_done.size() - 1);
    const LineTask chosen = not_done[distribution(generator)];

    // Снимаем старую подсветку без перерисовки текста (несохраненные правки не трогаем).
    SendMessageW(app.tasks_edit, WM_SETREDRAW, FALSE, 0);
    if (app.random_range.cpMin < app.random_range.cpMax) {
        CHARFORMAT2W clear_format = {};
        clear_format.cbSize = sizeof(clear_format);
        clear_format.dwMask = CFM_BACKCOLOR;
        clear_format.dwEffects = CFE_AUTOBACKCOLOR;
        SendMessageW(app.tasks_edit, EM_EXSETSEL, 0,
                     reinterpret_cast<LPARAM>(&app.random_range));
        SendMessageW(app.tasks_edit, EM_SETCHARFORMAT, SCF_SELECTION,
                     reinterpret_cast<LPARAM>(&clear_format));
    }

    const int char_index = static_cast<int>(
        SendMessageW(app.tasks_edit, EM_LINEINDEX, chosen.line_number - 1, 0));
    if (char_index < 0) {
        SendMessageW(app.tasks_edit, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(app.tasks_edit, nullptr, TRUE);
        return;
    }
    const std::wstring line_utf16 =
        to_utf16(std::to_string(chosen.line_number) + ". " + chosen.text);

    CHARFORMAT2W highlight = {};
    highlight.cbSize = sizeof(highlight);
    highlight.dwMask = CFM_BACKCOLOR;
    highlight.crBackColor = RANDOM_HIGHLIGHT;

    app.random_range.cpMin = char_index;
    app.random_range.cpMax = char_index + static_cast<int>(line_utf16.size());
    SendMessageW(app.tasks_edit, EM_EXSETSEL, 0,
                 reinterpret_cast<LPARAM>(&app.random_range));
    SendMessageW(app.tasks_edit, EM_SETCHARFORMAT, SCF_SELECTION,
                 reinterpret_cast<LPARAM>(&highlight));

    CHARRANGE caret{char_index, char_index};
    SendMessageW(app.tasks_edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&caret));
    SendMessageW(app.tasks_edit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(app.tasks_edit, nullptr, TRUE);
    SendMessageW(app.tasks_edit, EM_SCROLLCARET, 0, 0);
    SetFocus(app.tasks_edit);
}

void create_new_list(TodoApp& app) {
    // Окно «Новый список».
    static bool dialog_class_registered = false;
    if (!dialog_class_registered) {
        WNDCLASSEXW info = {};
        info.cbSize = sizeof(info);
        info.lpfnWndProc = list_dialog_proc;
        info.hInstance = app.instance;
        info.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        info.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        info.lpszClassName = DIALOG_CLASS;
        if (0 == RegisterClassExW(&info)) {
            return;
        }
        dialog_class_registered = true;
    }

    ListDialogState state;
    RECT owner_rect;
    GetWindowRect(app.window, &owner_rect);
    const int dialog_width = 260;
    const int dialog_height = 140;
    const int dialog_x = owner_rect.left +
        (owner_rect.right - owner_rect.left - dialog_width) / 2;
    const int dialog_y = owner_rect.top +
        (owner_rect.bottom - owner_rect.top - dialog_height) / 2;

    EnableWindow(app.window, FALSE);
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, DIALOG_CLASS, L"Новый список",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                  dialog_x, dialog_y, dialog_width, dialog_height,
                                  app.window, nullptr, app.instance, &state);
    if (nullptr == dialog) {
        EnableWindow(app.window, TRUE);
        return;
    }
    CreateWindowExW(0, L"STATIC", L"Введите имя нового списка:",
                    WS_CHILD | WS_VISIBLE, 12, 12, 224, 20, dialog, nullptr,
                    app.instance, nullptr);
    HWND name_edit =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                        12, 40, 224, 24, dialog,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NEW_NAME_EDIT)),
                        app.instance, nullptr);
    SendMessageW(name_edit, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    CreateWindowExW(0, L"BUTTON", L"OK",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    12, 76, 80, 26, dialog,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
                    app.instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Отмена",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    100, 76, 80, 26, dialog,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
                    app.instance, nullptr);

    ShowWindow(dialog, SW_SHOW);
    SetFocus(name_edit);

    MSG message;
    while (!state.finished && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    EnableWindow(app.window, TRUE);
    SetActiveWindow(app.window);

    if (!state.confirmed) {
        return;
    }

    const std::string new_name = to_utf8(state.value);
    try {
        app.list_service->create(new_name);
    } catch (const std::exception& error) {
        MessageBoxW(app.window, to_utf16(error.what()).c_str(), L"Ошибка",
                    MB_OK | MB_ICONERROR);
        return;
    }
    refresh_list_combobox(app);
    switch_active_list(app, new_name);
    restore_combobox_selection(app);
}

void layout_controls(TodoApp& app) {
    // WM_SIZE приходит и во время CreateWindowExW, до создания контролов.
    if (nullptr == app.window || nullptr == app.tasks_edit) {
        return;
    }
    RECT client;
    GetClientRect(app.window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;

    const int combo_width = std::min(150, std::max(width - 180, 60));
    SetWindowPos(app.list_combo, nullptr, 12, 8, combo_width, 24, SWP_NOZORDER);

    int next_x = 12 + combo_width + 6;
    for (int id : {IDC_BTN_NEW_LIST, IDC_BTN_SAVE, IDC_BTN_DONE, IDC_BTN_RANDOM}) {
        SetWindowPos(GetDlgItem(app.window, id), nullptr, next_x, 7, 34, 26,
                     SWP_NOZORDER);
        next_x += 40;
    }

    SetWindowPos(app.title_label, nullptr, 0, 40, width, 28, SWP_NOZORDER);

    const int edit_top = 72;
    SetWindowPos(app.tasks_edit, nullptr, 12, edit_top, std::max(width - 24, 40),
                 std::max(height - edit_top - 12, 40), SWP_NOZORDER);
}

void on_close(TodoApp& app) {
    if (has_unsaved_changes(app)) {
        const int answer = MessageBoxW(
            app.window, L"Сохранить изменения перед выходом?",
            L"Несохраненные изменения", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (IDCANCEL == answer) {
            return;  // остаёмся в приложении
        }
        if (IDYES == answer) {
            save_current_tasks(app);
        }
    }

    RECT rect;
    GetWindowRect(app.window, &rect);
    save_window_size(get_base_dir() / L"window_state.json", rect.right - rect.left,
                     rect.bottom - rect.top);
    DestroyWindow(app.window);
}

void handle_command(TodoApp& app, int id, int notify_code) {
    switch (id) {
        case IDC_BTN_NEW_LIST:
            create_new_list(app);
            break;
        case IDC_BTN_SAVE:
        case IDM_SAVE:
            save_with_message(app);
            break;
        case IDC_BTN_DONE:
            mark_task_done(app);
            break;
        case IDC_BTN_RANDOM:
            pick_random_task(app);
            break;
        case IDC_LIST_COMBO:
            if (CBN_SELCHANGE == notify_code) {
                on_list_selected(app);
            }
            break;
        default:
            break;
    }
}

void add_tooltip(TodoApp& app, HWND control, const wchar_t* text) {
    TOOLINFOW tool = {};
    tool.cbSize = sizeof(tool);
    tool.hwnd = app.window;
    tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tool.uId = reinterpret_cast<UINT_PTR>(control);
    tool.lpszText = const_cast<LPWSTR>(text);
    SendMessageW(app.tooltip_control, TTM_ADDTOOLW, 0,
                 reinterpret_cast<LPARAM>(&tool));
}

void create_controls(TodoApp& app) {
    app.list_combo =
        CreateWindowExW(0, WC_COMBOBOXW, nullptr,
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 12, 8,
                        140, 200, app.window,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LIST_COMBO)),
                        app.instance, nullptr);

    app.title_label =
        CreateWindowExW(0, L"STATIC", L"Мои задачи",
                        WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 40, DEFAULT_WIDTH, 28,
                        app.window,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TITLE_LABEL)),
                        app.instance, nullptr);

    app.tasks_edit =
        CreateWindowExW(0, MSFTEDIT_CLASS, nullptr,
                        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP |
                            ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                            ES_NOHIDESEL,
                        12, 72, DEFAULT_WIDTH - 24, DEFAULT_HEIGHT - 86, app.window,
                        reinterpret_cast<HMENU>(
                            static_cast<INT_PTR>(IDC_TASKS_EDIT)),
                        app.instance, nullptr);

    struct ButtonSpec {
        int id;
        const wchar_t* text;
    };
    const ButtonSpec buttons[] = {
        {IDC_BTN_NEW_LIST, L"+"},
        {IDC_BTN_SAVE, L"💾"},
        {IDC_BTN_DONE, L"✅"},
        {IDC_BTN_RANDOM, L"🎲"},
    };
    // «+» — один шрифт, три эмодзи-кнопки — другой; живут до конца сеанса.
    app.plus_button_font = make_font(L"Arial", 11, true);
    app.emoji_button_font = make_font(L"Segoe UI Emoji", 10, false);
    for (const ButtonSpec& spec : buttons) {
        const HFONT font =
            spec.id == IDC_BTN_NEW_LIST ? app.plus_button_font : app.emoji_button_font;
        HWND button =
            CreateWindowExW(0, L"BUTTON", spec.text,
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            0, 0, 34, 26, app.window,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(spec.id)),
                            app.instance, nullptr);
        SendMessageW(button, WM_SETFONT,
                     reinterpret_cast<WPARAM>(font), TRUE);
    }

    app.title_font = make_font(L"Arial", 16, true);
    app.edit_font = make_font(L"Arial", 12, false);
    SendMessageW(app.title_label, WM_SETFONT,
                 reinterpret_cast<WPARAM>(app.title_font), TRUE);
    SendMessageW(app.tasks_edit, WM_SETFONT,
                 reinterpret_cast<WPARAM>(app.edit_font), TRUE);

    // Событие изменения текста.
    SendMessageW(app.tasks_edit, EM_SETEVENTMASK, 0, ENM_CHANGE);

    // Подсказки (аналог tooltip.py).
    app.tooltip_control =
        CreateWindowExW(0, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP,
                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                        app.window, nullptr, app.instance, nullptr);
    add_tooltip(app, app.list_combo, L"Выбрать активный список");
    add_tooltip(app, GetDlgItem(app.window, IDC_BTN_NEW_LIST),
                L"Создать новый список");
    add_tooltip(app, GetDlgItem(app.window, IDC_BTN_SAVE),
                L"Сохранить задачи (Ctrl+S)");
    add_tooltip(app, GetDlgItem(app.window, IDC_BTN_DONE),
                L"Отметить/снять выполнение для строки с курсором");
    add_tooltip(app, GetDlgItem(app.window, IDC_BTN_RANDOM),
                L"Выбрать случайную невыполненную задачу");
}

TodoApp::TodoApp(HINSTANCE instance) : instance(instance) {
    INITCOMMONCONTROLSEX init = {};
    init.dwSize = sizeof(init);
    init.dwICC = ICC_WIN95_CLASSES;  // комбо, кнопки, подсказки
    InitCommonControlsEx(&init);

    WNDCLASSEXW info = {};
    info.cbSize = sizeof(info);
    info.style = CS_HREDRAW | CS_VREDRAW;
    info.lpfnWndProc = window_proc;
    info.hInstance = instance;
    info.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    info.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    info.lpszClassName = WINDOW_CLASS;
    RegisterClassExW(&info);

    // Акселератор Ctrl+S создаем кодом, чтобы не тянуть файл ресурсов.
    ACCEL save_accel = {};
    save_accel.fVirt = FVIRTKEY | FCONTROL;
    save_accel.key = static_cast<WORD>('S');
    save_accel.cmd = IDM_SAVE;
    accelerator = CreateAcceleratorTableW(&save_accel, 1);

    int saved_width = DEFAULT_WIDTH;
    int saved_height = DEFAULT_HEIGHT;
    std::tie(saved_width, saved_height) = load_window_size(
        get_base_dir() / L"window_state.json", DEFAULT_WIDTH, DEFAULT_HEIGHT);

    window = CreateWindowExW(0, WINDOW_CLASS, WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, saved_width, saved_height,
                             nullptr, nullptr, instance, this);
}

LRESULT CALLBACK TodoApp::window_proc(HWND window, UINT message, WPARAM wparam,
                                      LPARAM lparam) {
    if (WM_NCCREATE == message) {
        auto* app =
            static_cast<TodoApp*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return DefWindowProcW(window, message, wparam, lparam);
    }
    auto* app = reinterpret_cast<TodoApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (nullptr == app) {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    return app->handle(message, wparam, lparam);
}

LRESULT TodoApp::handle(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_SIZE:
            layout_controls(*this);
            return 0;
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMinTrackSize.x = DEFAULT_WIDTH;
            limits->ptMinTrackSize.y = DEFAULT_HEIGHT;
            return 0;
        }
        case WM_COMMAND:
            handle_command(*this, LOWORD(wparam), HIWORD(wparam));
            return 0;
        case WM_CLOSE:
            on_close(*this);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wparam, lparam);
    }
}

int TodoApp::run(int show_command) {
    storage = std::make_unique<TaskStorage>(get_base_dir());
    list_service = std::make_unique<ListService>(*storage);

    create_controls(*this);
    refresh_list_combobox(*this);
    restore_combobox_selection(*this);
    tasks = list_service->get_active_tasks();
    render_numbered_tasks(tasks_edit, tasks);
    layout_controls(*this);

    // Окно по центру, как в питоновской center_window.
    RECT rect;
    GetWindowRect(window, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    SetWindowPos(window, nullptr,
                 (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
                 (GetSystemMetrics(SM_CYSCREEN) - height) / 2, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(window, show_command);
    UpdateWindow(window);
    SetFocus(tasks_edit);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (accelerator != nullptr &&
            TranslateAcceleratorW(window, accelerator, &message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

TodoApp::~TodoApp() {
    if (title_font) DeleteObject(title_font);
    if (edit_font) DeleteObject(edit_font);
    if (emoji_button_font) DeleteObject(emoji_button_font);
    if (plus_button_font) DeleteObject(plus_button_font);
    if (accelerator) DestroyAcceleratorTable(accelerator);
}

}  // namespace taskmaster

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command) {
    // RichEdit 4.1: класс MSFTEDIT_CLASS из msftedit.dll.
    if (nullptr == LoadLibraryW(L"msftedit.dll")) {
        MessageBoxW(nullptr, L"Не удалось загрузить msftedit.dll", L"Ошибка",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    taskmaster::TodoApp app(instance);
    return app.run(show_command);
}
