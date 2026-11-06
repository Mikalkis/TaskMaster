// Консольные проверки логики без GUI — замена питоновских тестов tests/.
// Запуск без аргументов пишет результаты и возвращает 0 (ок) или 1 (есть падения).

#include "storage.hpp"
#include "task_utils.hpp"
#include "window_state.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "ok    " << name << "\n";
    } else {
        ++g_failures;
        std::cout << "FAIL  " << name << "\n";
    }
}

std::string vk_right(const std::string& expected, const std::string& actual,
                     const std::string& name) {
    check(expected == actual, name);
    if (expected != actual) {
        std::cout << "      expected: [" << expected << "] actual: [" << actual << "]\n";
    }
    return actual;
}

const char UTF8_DONE_EMOJI[] = "\xE2\x9C\x85";  // ✅

std::filesystem::path make_temp_base_dir(const std::string& tag) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
        ("taskmaster_test_" + tag + std::to_string(::rand()));
    std::filesystem::create_directories(dir);
    return dir;
}

void test_task_utils() {
    std::cout << "-- task_utils\n";
    check(taskmaster::is_task_done(std::string(UTF8_DONE_EMOJI) + " купить молоко"), "done emoji");
    check(taskmaster::is_task_done("\xE2\x9C\x94 сходить"), "done check mark");
    check(taskmaster::is_task_done("DONE: принести"), "done text mark");
    check(taskmaster::is_task_done("  DONE: с пробелами"), "done text mark with spaces");
    check(!taskmaster::is_task_done("просто задача"), "not done cyrillic");
    check(!taskmaster::is_task_done("✋ рука"), "not done other emoji");

    vk_right(std::string(UTF8_DONE_EMOJI) + " купить молоко",
             taskmaster::add_done_mark("купить молоко"), "add done mark");
    vk_right(std::string(UTF8_DONE_EMOJI) + " работа",
             taskmaster::add_done_mark(std::string(UTF8_DONE_EMOJI) + "  работа"),
             "add done mark idempotent");
    vk_right("работа", taskmaster::remove_done_mark(std::string(UTF8_DONE_EMOJI) + " работа"),
             "remove done mark");
    vk_right("работа", taskmaster::remove_done_mark("работа"), "remove done mark no-op");

    vk_right(std::string(UTF8_DONE_EMOJI) + " a", taskmaster::toggle_done_mark("a"), "toggle on");
    vk_right("a", taskmaster::toggle_done_mark(std::string(UTF8_DONE_EMOJI) + " a"), "toggle off");
    // Идемпотентность питоновского багфикса: двойной вызов возвращает исходный текст.
    vk_right("a", taskmaster::toggle_done_mark(taskmaster::toggle_done_mark("a")),
             "toggle double round");

    vk_right("мыть посуду", taskmaster::normalize_task_line("12. мыть посуду"), "normalize numbered");
    vk_right("мыть посуду", taskmaster::normalize_task_line("  12.  мыть посуду "), "normalize spaced");
    vk_right("12 мыть посуду", taskmaster::normalize_task_line("12 мыть посуду"), "normalize no dot");
    vk_right("пусто", taskmaster::normalize_task_line(" пусто "), "normalize unnumbered");

    std::vector<taskmaster::LineTask> with_lines;
    with_lines.push_back({2, "первая"});
    with_lines.push_back({5, "вторая"});
    check(taskmaster::find_task_index(with_lines, 5) == 1, "find task index");
    check(taskmaster::find_task_index(with_lines, 3) == -1, "find task index missing");

    std::vector<std::string> lines;
    taskmaster::split_lines("a\r\nb\nc\r\n", lines);
    check(lines.size() == 4 && lines[0] == "a" && lines[1] == "b" && lines[2] == "c" &&
              lines[3].empty(),
          "split lines crlf lf");
}

void test_storage() {
    std::cout << "-- storage\n";
    const std::filesystem::path base = make_temp_base_dir("storage");

    taskmaster::TaskStorage storage(base);
    storage.ensure_ready();

    const std::vector<std::string> names = storage.list_names();
    check(names.size() == 1 && names[0] == "default", "default list created");
    check(storage.get_active_name() == "default", "active default");

    // Дубль имени — исключение.
    bool duplicate_rejected = false;
    try {
        storage.create_list("default");
    } catch (const std::exception&) {
        duplicate_rejected = true;
    }
    check(duplicate_rejected, "create duplicate rejected");

    bool invalid_rejected = false;
    try {
        storage.create_list("bad/name");
    } catch (const std::exception&) {
        invalid_rejected = true;
    }
    check(invalid_rejected, "create invalid name rejected");

    const std::string created = storage.create_list(" summer ");
    check(created == "summer", "create name trimmed");
    storage.save_tasks({"молоко", "хлеб", "сыр"}, "summer");
    const std::vector<std::string> loaded = storage.load_tasks("summer");
    check(loaded.size() == 3 && loaded[0] == "молоко" && loaded[2] == "сыр",
          "save/load roundtrip");

    storage.set_active_name("summer");
    check(storage.get_active_name() == "summer", "switch active");
    check(storage.load_tasks().size() == 3, "load without name uses active");

    // Активный список, отсутствующий на диске, сбрасывается на первый.
    std::filesystem::remove(base / "lists" / L"summer.txt");
    check(storage.get_active_name() == "default", "missing active falls back");

    storage.save_tasks({"первая", "вторая"});
    check(storage.load_tasks().size() == 2, "default list saved");

    std::filesystem::remove_all(base);
}

void test_legacy_migration() {
    std::cout << "-- storage migration\n";
    const std::filesystem::path base = make_temp_base_dir("migrate");
    {
        std::ofstream legacy(base / "tasks.txt", std::ios::binary);
        legacy << "старая задача\nвторая старая\n";
    }
    taskmaster::TaskStorage storage(base);
    const std::vector<std::string> migrated = storage.load_tasks();
    check(migrated.size() == 2 && migrated[0] == "старая задача",
          "tasks.txt migrated into default list");
    std::filesystem::remove_all(base);
}

void test_window_state() {
    std::cout << "-- window_state\n";
    const std::filesystem::path file = make_temp_base_dir("state") / "window_state.json";

    taskmaster::save_window_size(file, 824, 668);
    const auto [width, height] = taskmaster::load_window_size(file, 320, 400);
    check(width == 824 && height == 668, "window size roundtrip");

    // Меньше минимума — откат на дефолт.
    taskmaster::save_window_size(file, 100, 10);
    const auto clamped = taskmaster::load_window_size(file, 320, 400);
    check(clamped.first == 320 && clamped.second == 400, "window size clamped");

    // Битые данные — дефолт.
    std::ofstream(file, std::ios::binary) << "not json";
    const auto broken = taskmaster::load_window_size(file, 320, 400);
    check(broken.first == 320 && broken.second == 400, "window size broken json");
    std::filesystem::remove_all(file.parent_path());
}

}  // namespace

int main() {
    test_task_utils();
    test_storage();
    test_legacy_migration();
    test_window_state();

    if (g_failures == 0) {
        std::cout << "все проверки пройдены\n";
        return 0;
    }
    std::cout << "падений: " << g_failures << "\n";
    return 1;
}
