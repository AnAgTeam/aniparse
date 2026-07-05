<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/lockup-dark.svg">
  <img src="assets/lockup-light.svg" alt="aniparse" width="280">
</picture>

**C++ библиотека для парсинга аниме, манги, изображений и видео**

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/build-CMake-orange?logo=cmake&logoColor=white)
![License: MIT](https://img.shields.io/badge/License-MIT-green)
![Branch: nightly](https://img.shields.io/badge/branch-nightly-purple)
[![CI](https://github.com/AnAgTeam/aniparse/actions/workflows/coverage.yml/badge.svg?branch=nightly)](https://github.com/AnAgTeam/aniparse/actions/workflows/coverage.yml)
[![codecov](https://codecov.io/gh/AnAgTeam/aniparse/branch/nightly/graph/badge.svg)](https://codecov.io/gh/AnAgTeam/aniparse)

[English](README.md) · **Русский**

</div>

> [!WARNING]
> Библиотека находится в ранней стадии разработки, API может существенно изменяться.

---

## О проекте

**aniparse** — статическая C++20 библиотека для парсинга контента из различных источников. Основной фокус — аниме и манга; также поддерживается получение изображений и видео.

Библиотека построена на асинхронной сети ([libasyncnet](https://github.com/AnAgTeam/libasyncnet)) и HTML-парсере [lexbor](https://github.com/lexbor/lexbor), что позволяет эффективно обрабатывать веб-страницы без лишних зависимостей.

---

## Возможности

- Парсинг аниме-тайтлов (релизы, метаданные)
- Парсинг манги
- Получение изображений и видео из различных источников
- Backend-нейтральный HTTP-клиент: curl сейчас, но контракт запроса/ответа не завязан на него (можно подменить бэкенд, например на NSURLSession)
- Типизированные запросы `request_html` / `request_json` с обработкой ошибок без исключений (`tl::expected`)
- Встроенный HTML/DOM-парсер, CSS-селекторы и JS-парсер на базе `lexbor`
- Куки и авторизация парсеров, `multipart/form-data`, произвольные HTTP-методы

---

## Быстрый старт

Получить страницу и распарсить как HTML — клиент бэкенд-нейтральный, парсер не видит curl:

```cpp
#include <aniparse/Client.hpp>
#include <aniparse/ClientContext.hpp>
#include <coro/sync_wait.hpp>
#include <print>

using namespace aniparse;

int main() {
    auto client = std::make_shared<AsyncClient>();
    RequestorContext ctx(client, nullptr, nullptr);

    // request_html: получение + проверка статуса + парсинг в одном expected.
    auto page = coro::sync_wait(ctx.request_html(GetRequest{ .url = "https://example.com" }));
    if (page) {
        std::println("{}", page->title());
    }
}
```

Чтобы написать свой источник, скопируй [скелет парсера](examples/example_manga_parser.cpp) и заполни геттеры. Все [примеры](#примеры) ниже.

---

## Как устроено

**`Parser`** регистрирует домены, которые умеет обрабатывать, и предоставляет геттеры (например `MangaRootGetter`), которые получают и парсят контент. Геттеры работают через **`RequestorContext`** — он несёт конфиг парсера, куки и логгер и сидит поверх бэкенд-нейтрального **`ClientContext`** (сейчас curl). Парсеры оперируют только нейтральными типами запроса/ответа — поэтому HTTP-бэкенд можно заменить, не трогая ни одного парсера.

---

## Требования

- **Компилятор:** с поддержкой C++20 (GCC 12+, Clang 15+, MSVC 2022+). Сами примеры и тесты собираются как C++23 (используют `std::print`/`std::println`), поэтому для них нужен более новый компилятор и стандартная библиотека (ориентировочно GCC 14+, Clang 18+, MSVC 19.40+).
- **CMake:** 3.18+
- **vcpkg** (рекомендуется для управления зависимостями)

**Зависимости** (устанавливаются через vcpkg). Все обязательны — библиотека линкует их безусловно:

| Пакет | Назначение |
|---|---|
| `boost-json` | JSON-парсинг |
| `boost-regex` | Регулярные выражения |
| `boost-system` | Системные утилиты Boost |
| `curl` | HTTP-клиент (через `libasyncnet`) |
| `fmt` | Форматирование строк |
| `tl-expected` | Обработка ошибок без исключений (`tl::expected`) |

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

- [Скелет парсера](examples/example_manga_parser.cpp) — структура парсера: какие методы реализовать. Копируется под свой источник.
- [Парсинг](examples/example_parse.cpp) — извлечение данных из HTML: CSS-селекторы, DOM, JSON из `<script>`. Работает офлайн, на фикстуре.
- [Сеть](examples/example_net.cpp) — реальные HTTP-запросы через `request` / `request_html` / `request_json`.

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
