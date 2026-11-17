# Проверка ppchibicc перед переносом LLVM в main

Это инструкция для воспроизводимой проверки и сообщения об ошибках.
Пока PR №5 не перенесён в `main`, LLVM проверяется в `feature/llvm-ir-mvp`.
После переноса можно использовать `main`. Компилятор экспериментальный;
границы поддержки перечислены в [сводке стабилизации](llvm-stabilization-ru.md).

## Окружение и исходники

Нужны Linux x86-64, системный GCC или Clang, GNU binutils, заголовки и
библиотеки разработки libc, Make и CMake 3.16+. Основная проверка не требует
LLVM. LLVM-наборы используют Python 3.8+; для них рекомендуются `llvm-as`,
`opt`, `llc` и `llvm-link` одной версии LLVM 23.1.x. Ранее также проверялись
LLVM 19.1.7 и 21.1.8. Clang и compiler-rt не нужны для обычной LLVM-сборки
программ; они нужны для отдельных экспериментов PGO.

```sh
git clone --branch feature/llvm-ir-mvp https://github.com/kreofil/pp-chibicc.git
cd pp-chibicc
git rev-parse HEAD
git status --short
```

Сохраните SHA: он однозначно указывает проверенные исходники. Используйте
новые каталоги сборки. При нескольких сборках `bin/ppchibicc` содержит stage1
из последней успешно завершившейся сборки; для сравнения используйте точные
пути `build/stage1/ppchibicc` и `build/stage2/ppchibicc`.

## Основная проверка без LLVM

```sh
cmake -S . -B build
cmake --build build --target test-all -j2
```

Проверяются stage1, native-stage2, C/4П, командная строка, многофайловые
программы и примеры. Успех — нулевой код завершения всей команды.
Ожидаемые отрицательные тесты могут выводить диагностику; отдельная строка
`error` в журнале сама по себе не означает провал набора.

## Проверка LLVM

Сначала убедитесь, что версии инструментов согласованы:

```sh
llvm-as --version
opt --version
llc --version
llvm-link --version
cmake -S . -B build-llvm-check -DPPCHIBICC_LLVM_VERSION=23
cmake --build build-llvm-check --target test-llvm test-llvm-stage2 -j2
cmake --build build-llvm-check --target \
  test-llvm-closed-compiler test-llvm-closed-compiler-stage2 -j2
```

Наборы проверяют O0/O2, ABI с системным компилятором, обычные и полиморфные
вызовы и закрытые программы, включая CMake. Если доступны только версионные
имена, CMake найдёт их по `PPCHIBICC_LLVM_VERSION`. Для самостоятельного
запуска компилятора настройте окружение отдельно:

```sh
export PPCHIBICC_LLVM_AS=llvm-as-23
export PPCHIBICC_LLVM_OPT=opt-23
export PPCHIBICC_LLVM_LLC=llc-23
export PPCHIBICC_LLVM_LINK=llvm-link-23
```

Если обычные имена без суффикса уже указывают на нужную версию, эти четыре
строки не нужны. Значение переменной — имя или путь одной программы.

Небольшой пример с одинаковым результатом у обоих генераторов:

```sh
build-llvm-check/stage1/ppchibicc --backend=x86-64 \
  examples/llvm/ppc-vararg-items.c -o build-llvm-check/items-native
build-llvm-check/stage1/ppchibicc --backend=llvm -O2 \
  examples/llvm/ppc-vararg-items.c -o build-llvm-check/items-llvm
build-llvm-check/items-native
build-llvm-check/items-llvm
```

Оба запуска должны вывести `normal=13.0 scaled=39.0`.
Самостоятельная [закрытая сборка](llvm-closed-compiler-ru.md) и
[генерация IR без LLVM-инструментов](llvm-backend-ru.md) описаны отдельно.

## Дополнительная проверка самокомпиляции

```sh
cmake -S . -B build-llvm-selfhost-check \
  -DPPCHIBICC_STAGE2_BACKEND=llvm -DPPCHIBICC_LLVM_VERSION=23
cmake --build build-llvm-selfhost-check --target test-llvm-selfhost -j2
cat build-llvm-selfhost-check/llvm-selfhost/report.md
```

Здесь сам компилятор stage2 строится через LLVM, затем повторяет сборку в
stage3. Проверяются совпадение IR/объектов и работа stage3. Подробности — в
[руководстве самокомпиляции](llvm-selfhost-ru.md). Конфигурацию O0 при желании
проверьте отдельно с `-DPPCHIBICC_STAGE2_LLVM_OPTIMIZATION=0` в новом каталоге.

Полный состав CI и закреплённая установка инструментов описаны в
[llvm-ci-ru.md](llvm-ci-ru.md). Установщик CI предназначен для Ubuntu Noble
amd64 и требует Python 3.11+, curl и dpkg-deb; это отдельный способ подготовки
окружения, а не обязательный шаг для системы с уже установленным LLVM.

## Как сообщить об ошибке

Создайте [issue](https://github.com/kreofil/pp-chibicc/issues/new/choose)
по шаблону «Ошибка компилятора». Приложите:

- SHA коммита, локальные изменения, ОС/архитектуру и версии инструментов;
- минимальную программу, все её необходимые заголовки и точные команды;
- ожидаемый результат, фактический вывод и код завершения;
- полный журнал команды, которая завершилась неуспешно;
- путь/поколение компилятора, backend, уровень оптимизации и режим
  диспетчеризации, если он задавался.

Для сравнения попробуйте native и LLVM O0/O2. Результат GCC/Clang полезен
для обычного C; особенности 4П проверяются по документации PPC. Если проблема
возникает только в проекте, укажите состав модулей, их порядок и команды
компоновки. Для bootstrap приложите `llvm-selfhost/report.md` и `report.json`,
для закрытой программы — её `.closed.json`. Перед публикацией удалите из
примера и журналов данные, которые не собирались публиковать.
