#pragma once

// Чистая логика задач, портирована 1:1 с task_utils.py (тот же поведенческий контракт — тесты test_core.cpp).

#include <string>
#include <vector>

namespace tm {

// Строка текстового поля (1-based, как в tkinter) и задача на ней.
struct LineTask {
    int line_number = 0;
    std::string text;
};

// Задача выполнена, если начинается с "✅", "✔" или "DONE:" (после пробелов).
bool is_task_done(const std::string& task_text);
// Убирает визуальную нумерацию "12. " и внешние пробелы.
std::string normalize_task_line(const std::string& line);
std::string add_done_mark(const std::string& task_text);
std::string remove_done_mark(const std::string& task_text);
// Идемпотентно: у отмеченной задачи снимает метку, у обычной — добавляет.
std::string toggle_done_mark(const std::string& task_text);

// Индекс задачи, живущей на данной строке, или -1.
int find_task_index(const std::vector<LineTask>& tasks_with_lines, int line_number);

// Режет текст на строки; поддерживает \n и \r\n; пустые строки сохраняются.
void split_lines(const std::string& content, std::vector<std::string>& out);

}  // namespace tm
