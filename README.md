# TaskMaster

Диспетчер задач под Windows: Win32 GUI (WinAPI + RichEdit), без внешних зависимостей.
Язык интерфейса — русский. Каталог данных приложения (`lists/`, `app_state.json`,
`window_state.json`) лежит рядом с exe.

## Возможности

- Списки задач: переключение в комбо-боксе, создание новых кнопкой `+`.
- Отметка выполнения кнопкой `✅` — задача красится в зелёный; повторное нажатие снимает отметку.
- Кнопка `💾` или `Ctrl+S` — сохранить; при переключении списка и выходе программа
  сама предлагает сохранить, если поле правилось.
- Кнопка `🎲` — случайная невыполненная задача с подсветкой строки.
- Размер окна запоминается между запусками.

## Сборка

Нужен любой x64-компилятор для Windows.

### Visual Studio Build Tools (cl.exe, из «Developer Command Prompt»)

```
rc /fo taskmaster.res resources.rc
cl /EHsc /utf-8 /std:c++17 /DUNICODE /D_UNICODE /DNOMINMAX /FeTaskMaster.exe src\*.cpp taskmaster.res /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib
```

### CMake (MSVC или MinGW w64)

```
cmake -S . -B build
cmake --build build --config Release
```

Готовый `TaskMaster.exe` появится в `build\Release` (MSVC) или `build` (MinGW).
Положите его рядом с `lists/` (или скопируйте файлы данных к exe) — и запускайте.

### Консольные тесты логики

```
cl /EHsc /utf-8 /std:c++17 /DNOMINMAX /Fetests_core.exe src\test_core.cpp src\storage.cpp src\task_utils.cpp src\window_state.cpp src\file_utils.cpp src\paths.cpp src\text.cpp
tests_core.exe
```

Через CMake — цель `test_core`: `cmake --build build --target test_core` и запустить
`test_core.exe` (или файл в `build\Release`). Код возврата 0 — все проверки пройдены.

## Формат данных

- `lists/<имя>.txt` — по одной задаче в строке; пустые строки игнорируются.
  Выполненные задачи начинаются с `✅ ` / `✔ ` / `DONE: `.
- `app_state.json` — `{"active_list": "..."}`
- `window_state.json` — `{"width": N, "height": N}`
- `tasks.txt` — старый одиночный файл задач; при первом запуске автоматически
  мигрируется в `lists/default.txt`.

Кодировка файлов — UTF-8. Форматы полностью совместимы с прежней (python-)версией.

## Структура

```
src/
  main.cpp      — GUI-логика Win32 и WinMain
  gui.hpp       — класс окна TodoApp
  list_service  — активный список поверх хранилища
  storage       — каталог lists/ и app_state.json
  task_utils    — чистая логика задач (метки, нумерация)
  window_state  — размер окна ↔ window_state.json
  file_utils    — атомарная запись + мини-JSON
  paths         — базовый каталог (рядом с exe)
  text          — UTF-8 ↔ UTF-16
  test_core.cpp — консольные тесты логики
resources.rc    — иконка exe (icon.ico лежит рядом)
```

## Лицензия

MIT
