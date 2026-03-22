<div align="center">

# aniparse

**C++ библиотека для парсинга аниме, манги, изображений и видео**

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/build-CMake-orange?logo=cmake&logoColor=white)
![License: MIT](https://img.shields.io/badge/License-MIT-green)
![Branch: nightly](https://img.shields.io/badge/branch-nightly-purple)

</div>

> [!WARNING]  
> Библиотека находится в ранней стадии разработки, API может существенно изменятся.

---

## О проекте

**aniparse** — статическая C++20 библиотека для парсинга контента из различных источников. Основной фокус — аниме и манга; также поддерживается получение изображений и видео.

Библиотека построена на асинхронной сети ([libasyncnet](https://github.com/AnAgTeam/libasyncnet)) и HTML-парсере [lexbor](https://github.com/lexbor/lexbor), что позволяет эффективно обрабатывать веб-страницы без лишних зависимостей.

---

## Возможности

- Парсинг аниме-тайтлов (релизы, метаданные)
- Парсинг манги
- Получение изображений и видео из различных источников
- Асинхронный HTTP-клиент на базе `libasyncnet` + `libcurl`
- Встроенный HTML/DOM-парсер и JS-парсер на базе `lexbor`
- Гибкая настройка зависимостей через CMake-опции

---

## Требования

- **Компилятор:** GCC 12+, Clang 15+, или MSVC 2022+ с поддержкой C++20
- **CMake:** 3.18+
- **vcpkg** (рекомендуется для управления зависимостями)

**Зависимости** (устанавливаются через vcpkg):

| Пакет | Назначение |
|---|---|
| `boost-json` | JSON-парсинг |
| `boost-regex` | Регулярные выражения (опционально) |
| `boost-system` | Системные утилиты Boost |
| `curl` | HTTP-клиент |
| `fmt` | Форматирование строк (опционально) |
| `tl-expected` | Обработка ошибок без исключений (опционально) |

Субмодули (подтягиваются автоматически):
- [lexbor](https://github.com/lexbor/lexbor) — HTML-парсер
- [libasyncnet](https://github.com/AnAgTeam/libasyncnet) — асинхронная сеть

---

## Сборка

### Клонирование

```bash
git clone --recurse-submodules https://github.com/AnAgTeam/aniparse
cd aniparse
```

### С vcpkg (рекомендуется)

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Без vcpkg

Убедитесь, что зависимости установлены в системе, затем:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### CMake-опции

| Опция | По умолчанию | Описание |
|---|---|---|
| `ANIPARSE_BUILD_TESTS` | `OFF` | Сборка тестов |
| `ANIPARSE_BUILD_EXAMPLES` | `OFF` | Сборка примеров |
| `ANIPARSE_BUILD_TOOLS` | `OFF` | Сборка утилит |
| `ANIPARSE_USE_TL_EXPECTED` | `ON` | Использовать `tl::expected` вместо `std::expected` |
| `ANIPARSE_USE_LIBFMT` | `OFF` | Использовать `fmt::format` вместо `std::format` |
| `ANIPARSE_USE_BOOST_REGEX` | `OFF` | Использовать `boost::regex` вместо `std::regex` |

---

## Установка

```bash
cmake --install build --prefix /usr/local
```

После установки библиотека доступна через CMake:

```cmake
find_package(aniparse REQUIRED)
target_link_libraries(your_target PRIVATE aniparse::aniparse)
```

Или через pkg-config:

```bash
pkg-config --libs --cflags aniparse
```

---

## Примеры

Примеры находятся в директории [`examples/`](examples/). Для их сборки:

```bash
cmake -B build -DANIPARSE_BUILD_EXAMPLES=ON
cmake --build build
```

[Пример манга парсера](examples/example_manga_parser.cpp)

---

## Структура проекта

```
aniparse/
├── include/aniparse/       # Публичные заголовки
│   ├── anime/              # Парсинг аниме (Release и др.)
│   ├── manga/              # Парсинг манги
│   ├── images/             # Парсинг изображений
│   ├── html/               # HTML/DOM/JS парсер
│   └── utility/            # Вспомогательные утилиты
├── src/                    # Реализация
├── examples/               # Примеры использования
├── tests/                  # Тесты
├── tools/                  # Дополнительные инструменты
├── lexbor/                 # Субмодуль: HTML-парсер
└── libasyncnet/            # Субмодуль: асинхронная сеть
```

---

## Вклад в проект

Pull request'ы и issue приветствуются. Убедитесь, что код компилируется без предупреждений (`-Wall -Wextra -Wpedantic`) и проходит тесты:

```bash
cmake -B build -DANIPARSE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

---

## Лицензия

Распространяется под лицензией [MIT](LICENSE). Copyright © 2025–2026 Toilettrauma.

Обратите внимание: проект включает сторонние компоненты с собственными лицензиями — см. файл [NOTICE](NOTICE).
