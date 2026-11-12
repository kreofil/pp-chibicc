# Закрытая LLVM-сборка через ppchibicc и CMake

Опция `--closed-program` включает LLVM и собирает полную пользовательскую
программу: исходный IR всех модулей объединяется, подходящие закрытые таблицы
4П становятся константными, затем запускаются `opt`, `llc` и ELF-линкер.
Ручной запуск `ppc-static-dispatch.py` больше не обязателен.

Режим предназначен для конечного исполняемого файла на Linux x86-64. Передавая
эту опцию, разработчик утверждает, что перечислил все пользовательские модули.
Проверки состава и преобразование описаны в [руководстве статических
таблиц](ppc-static-dispatch-ru.md). Автоматически преобразуются только функции,
у которых закрыты **все** процедурные типы и однозначно заполнена вся матрица.
Открытые и неразрешённые семьи сохраняют runtime-путь; их причины перечисляются
в отчёте. Если подходящих семей нет, допустима обычная C-программа или
программа с открытыми обобщениями: `optimization_applied` будет `false`.

## Одна команда

Нужны Python 3.8+, системный C toolchain и `llvm-as`, `opt`, `llvm-link`, `llc`
одной major-версии, не ниже 19. Проверены LLVM 19.1.7, 21.1.8 и 23.1.6.
Сначала соберите сам компилятор:

```sh
cmake -S . -B build -DPPCHIBICC_LLVM_VERSION=23
cmake --build build --target ppchibicc_stage1

bin/ppchibicc --closed-program -O2 \
  examples/llvm/closed-registry/main.c \
  examples/llvm/closed-registry/special.c \
  examples/llvm/closed-registry/default.c \
  -o build/closed-registry
build/closed-registry
cat build/closed-registry.closed.json
```

Ожидается `closed registry OK`. Матрица `registry_combine` размером `2×3`
заменяется статическим массивом; удаляются 11 функций выделения, регистрации,
проверки полноты и освобождения этой таблицы. Поддержка признаков и глобальных
объектов сохраняется.

По умолчанию используется `-O0`; для оптимизации всей программы задайте
`-O2` в **окончательной** команде. Поддержаны также O1/O3/Os/Oz.
`--closed-family=registry_combine` требует преобразовать указанную семью;
при неполной, открытой или неоднозначной матрице сборка завершится ошибкой.
Опцию можно повторять. Без неё выбираются все подходящие семьи.

Пути инструментов задаются окружением:

```sh
export PPCHIBICC_LLVM_AS=/path/to/llvm-as-23
export PPCHIBICC_LLVM_OPT=/path/to/opt-23
export PPCHIBICC_LLVM_LINK=/path/to/llvm-link-23
export PPCHIBICC_LLVM_LLC=/path/to/llc-23
export PPCHIBICC_PYTHON=/path/to/python3
```

Значение каждой переменной — одна программа без дополнительных аргументов.
Без переменных используются имена из `PATH`. CMake-параметры с этими именами
настраивают тестовые цели; при отдельном запуске ppchibicc нужны окружение
либо `PATH`. Версии четырёх LLVM-инструментов проверяются перед преобразованием.

## Раздельная компиляция

```sh
mkdir -p build/closed-objects
for unit in main special default; do
  bin/ppchibicc --closed-program -c \
    examples/llvm/closed-registry/$unit.c -o build/closed-objects/$unit.o
done
bin/ppchibicc --closed-program -O2 build/closed-objects/*.o \
  -o build/closed-registry
```

В этом явно выбранном режиме `.o` — **текстовый исходный LLVM IR с маркером
ppchibicc**, а не ELF и не bitcode. Он сохраняет сведения, нужные для проверки
и объединения всей программы. Стадия `-c` не требует Python или установленных
LLVM-инструментов; оптимизация отложена до окончательной сборки. Прямые входы
`.c`, `.ppc`, исходные `.ll` ppchibicc и такие `.o` можно смешивать.
Файлы `.pph` подключаются через `#include`.

Обычные `-c` и `--backend=llvm -c` продолжают выдавать ELF. При смене режима
сделайте чистую пересборку либо используйте отдельный каталог: промежуточный
IR-объект нельзя передавать системному линкеру или обычной сборке ppchibicc.
Такая попытка диагностируется. ABI 4П и значения признаков этим этапом не меняются.

`-MMD -MP -MF ...` поддерживаются при отдельном `-c`. Генерация зависимостей
при окончательной сборке нескольких модулей отклоняется. Препроцессорные
`-E`/`-M` работают без внешних инструментов.

## Обычные цели CMake

Готовый пример использует стандартные `add_executable`, объектную библиотеку,
зависимости заголовков, инкрементальную сборку и CTest:

```sh
cmake -S examples/llvm/closed-build -B build-closed-demo \
  -DCMAKE_C_COMPILER="$PWD/bin/ppchibicc"
cmake --build build-closed-demo
ctest --test-dir build-closed-demo --output-on-failure
cat build-closed-demo/closed-registry.closed.json
```

Компилятор выбирается при первом конфигурировании, до `project()`. Для своего
проекта подключите модуль из репозитория или установленного пакета:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_closed_program LANGUAGES C)
include("/path/to/pp-chibicc/cmake/PpchibiccClosedProgram.cmake")

add_library(handlers OBJECT special.c default.c)
ppchibicc_enable_closed_program(handlers)

add_executable(program main.c)
target_link_libraries(program PRIVATE handlers)
ppchibicc_enable_closed_program(program
  OPTIMIZATION 2
  FAMILIES registry_combine
  REPORT "program.audit.json")
```

`OPTIMIZATION`, `FAMILIES` и `REPORT` необязательны и задаются только конечному
исполняемому target. Оптимизация применяется при окончательной сборке;
по умолчанию действует обычный уровень компилятора. Относительный `REPORT`
считается от текущего каталога сборки. Без него отчёт записывается рядом с
исполняемым файлом с суффиксом `.closed.json`. `clean` удаляет отчёт; изменение
утилит анализа вызывает повторную линковку. Все подключаемые объектные
библиотеки также должны получить `ppchibicc_enable_closed_program`.

Для `.ppc` CMake по-прежнему нужно `set_source_files_properties(... PROPERTIES
LANGUAGE C)`; `.pph` остаются заголовками. Пример этого описан в
[основной инструкции компилятора](llvm-compiler-ru.md).

## Установка и результаты

```sh
cmake --install build --prefix "$PWD/local"
local/bin/ppchibicc --closed-program -O2 \
  examples/llvm/closed-registry/main.c \
  examples/llvm/closed-registry/special.c \
  examples/llvm/closed-registry/default.c -o build/installed-program
```

Python-утилиты и CMake-модуль устанавливаются в `share/ppchibicc`. Компилятор
находит утилиты относительно своего исполняемого файла; весь префикс можно
перемещать. Для нестандартного размещения задайте
`PPCHIBICC_STATIC_DISPATCH=/path/to/ppc-static-dispatch.py`; рядом с ней должен
находиться `ppc-dispatch-report.py`.

`--closed-report=PATH` меняет путь JSON-отчёта. В нём сохраняются хеши входного
IR, выбранные матрицы, удалённые функции, невыбранные семьи и причины. Поле
`output_sha256` относится к объединённому IR **до `opt`**, а не к машинному
исполняемому файлу. Исходники, компилируемые одной командой, проходят через
временные `.ll`; их исходные имена сохраняются в LLVM-метаданных.

`--closed-program -S -emit-llvm ... -o program.ll` сохраняет этот объединённый
проверенный IR до оптимизации; здесь нужны Python, `llvm-as`, `opt`, `llvm-link`,
но не `llc`. Обычный `-S -emit-llvm` без `--closed-program` продолжает выдавать
IR отдельного исходника без внешних инструментов. Для машинного ассемблера
используйте `--closed-program -O2 -S ... -o program.s`.

При ошибках frontend, инструментов, анализа или линковки прежний исполняемый
файл и его отчёт сохраняются. Выход и отчёт должны отличаться друг от друга
и от входных модулей.

Первый вариант закрытой сборки принимает только перечисленные исходные
модули приложения. Native ELF-объекты, архивы, `.so`, входной ассемблер,
bitcode, произвольные `-l`/`-L`/`-Wl`/`-Xlinker` и `-shared` не входят в этот
режим. Допустимы системный C runtime, `-lm`, `-static` и `-s`. Остальные внешние
функции ограничены проверяемым списком C/POSIX/математических операций утилиты.
Для смешивания с другими компиляторами и расширяемых библиотек используйте
обычную [LLVM-сборку](llvm-compiler-ru.md).

## Проверки

```sh
cmake --build build --target test-llvm-closed-compiler
cmake --build build --target test-llvm-closed-compiler-stage2
```

Проверяются исходники и раздельные объекты, все представления диспетчеризации,
уровни оптимизации, IR/ассемблер, открытые семьи и чистый C, `.ppc`/`.pph`,
зависимости, ошибки инструментов и входов. Отдельно проверяются CMake/CTest,
пересборка по заголовку, `clean`, установка, перенос префикса и переопределение
пути утилиты. Обе цели входят в [LLVM CI](llvm-ci-ru.md).
