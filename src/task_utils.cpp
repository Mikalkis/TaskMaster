#include "task_utils.hpp"

namespace tm {

namespace {

// UTF-8 последовательности знаков метки.
const char DONE_MARK_EMOJI[] = "\xE2\x9C\x85";    // ✅
const char DONE_MARK_CHECK[] = "\xE2\x9C\x94";    // ✔
const char DONE_MARK_TEXT[]  = "DONE:";

bool starts_with(const std::string& text, size_t at, const char* prefix) {
    const size_t prefix_length = std::char_traits<char>::length(prefix);
    return text.size() - at >= prefix_length &&
           text.compare(at, prefix_length, prefix) == 0;
}

size_t skip_spaces(const std::string& text, size_t at) {
    while (at < text.size() && (text[at] == ' ' || text[at] == '\t')) {
        ++at;
    }
    return at;
}

bool done_prefix_length(const std::string& task_text, size_t& prefix_length) {
    const size_t at = skip_spaces(task_text, 0);
    if (starts_with(task_text, at, DONE_MARK_EMOJI) ||
        starts_with(task_text, at, DONE_MARK_CHECK)) {
        prefix_length = at + 3;             // обе метки — 3 байта UTF-8
        return true;
    }
    if (starts_with(task_text, at, DONE_MARK_TEXT)) {
        prefix_length = at + 5;             // "DONE:"
        return true;
    }
    return false;
}

std::string trim_both(const std::string& text, size_t first, size_t last) {
    first = skip_spaces(text, first);
    while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\t' ||
                            text[last - 1] == '\r')) {
        --last;
    }
    return text.substr(first, last - first);
}

}  // namespace

bool is_task_done(const std::string& task_text) {
    size_t unused = 0;
    return done_prefix_length(task_text, unused);
}

std::string normalize_task_line(const std::string& line) {
    const size_t first = skip_spaces(line, 0);
    size_t at = first;
    // Визуальная нумерация: последовательность цифр, точка, пробел.
    while (at < line.size() && line[at] >= '0' && line[at] <= '9') {
        ++at;
    }
    if (at > first && at + 1 < line.size() && line[at] == '.' &&
        (line[at + 1] == ' ' || line[at + 1] == '\t')) {
        return trim_both(line, at + 1, line.size());
    }
    return trim_both(line, 0, line.size());
}

std::string add_done_mark(const std::string& task_text) {
    // Как в питоновской версии: старая метка снимается, вторая ✅ появиться не может.
    return std::string(DONE_MARK_EMOJI) + " " + remove_done_mark(task_text);
}

std::string remove_done_mark(const std::string& task_text) {
    size_t prefix_length = 0;
    if (!done_prefix_length(task_text, prefix_length)) {
        return task_text;
    }
    return trim_both(task_text, prefix_length, task_text.size());
}

std::string toggle_done_mark(const std::string& task_text) {
    if (is_task_done(task_text)) {
        return remove_done_mark(task_text);
    }
    return add_done_mark(task_text);
}

int find_task_index(const std::vector<LineTask>& tasks_with_lines, int line_number) {
    for (size_t index = 0; index < tasks_with_lines.size(); ++index) {
        if (tasks_with_lines[index].line_number == line_number) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void split_lines(const std::string& content, std::vector<std::string>& out) {
    out.clear();
    std::string current;
    for (size_t i = 0; i < content.size(); ++i) {
        const char c = content[i];
        if (c == '\n' || c == '\r') {
            // \r\n — один разделитель; одиночный \r тоже используем.
            if (c == '\r' && i + 1 < content.size() && content[i + 1] == '\n') {
                ++i;
            }
            out.push_back(current);
            current.clear();
            continue;
        }
        current += c;
    }
    out.push_back(current);
}

}  // namespace tm
