# Сборка программ через LLVM компилятором ppchibicc

Параметр `--backend=llvm` соединяет общий frontend, генератор текстового IR,
внешние `opt` и `llc` и существующий ELF-линкер компилятора. Можно собрать программу
одной командой:

```sh
bin/ppchibicc --backend=llvm -O2 examples/llvm/hello.c -o build/hello
build/hello
```

Платформа — Linux x86-64 / System V, поддерживаемое подмножество C и 4П
описано в [руководстве backend](llvm-backend-ru.md). По умолчанию продолжает
использоваться прямой генератор x86-64; явно выбрать его можно через
`--backend=x86-64`. При нескольких выборах backend действует последний.

## Режимы вывода

| Команда | Результат |
| --- | --- |
| `--backend=llvm program.c -o program` | Исполняемый файл |
| `--backend=llvm -c program.c` | Объект `program.o` |
| `--backend=llvm -S program.c` | Машинный ассемблер `program.s` |
| `-S -emit-llvm program.c` | Исходный текстовый IR `program.ll` |
| `--backend=llvm input.ll -o program` | Оптимизация IR, объект и линковка |
| `--backend=llvm -c input.bc -o program.o` | Объект из bitcode |

`-o` задаёт имя результата. Для `-S` и `-c` поддерживается `-o -`: ассемблер
или двоичный объект выводится в stdout. Вход из stdin задаётся через `-x c -`
или `--backend=llvm -x ir -`.

Старый `-S -emit-llvm` сохраняет роль генератора **исходного IR**: он работает
без установленных LLVM-инструментов. Даже `-O2` в этом режиме не запускает
`opt`. Для оптимизации сохранённого IR используйте внешний `opt` либо
передайте `.ll`/`.bc` в новый режим компилятора. Одновременный явный выбор
`--backend=x86-64` и `-emit-llvm` диагностируется как конфликт.

IR-входы должны соответствовать Linux x86-64 и принятому ABI/раскладке данных.
Выбор backend не меняет целевую модель frontend. Текстовый IR содержит
`source_filename` с исходным именем C-файла: временные имена компилятора не
заменяют его в машинном листинге. Кавычки, обратные слеши и UTF-8 в имени
экранируются LLVM-последовательностями байтов.

## Уровни оптимизации

По умолчанию действует `-O0`. Последний параметр `-O...` определяет уровень.

| Параметр | `opt` | `llc` |
| --- | --- | --- |
| `-O0` | `default<O0>` | `-O=0` |
| `-O` или `-O1` | `default<O1>` | `-O=1` |
| `-O2` | `default<O2>` | `-O=2` |
| `-O3` | `default<O3>` | `-O=3` |
| `-Os` | `forceattrs,default<O2>`, атрибут `optsize` | `-O=2` |
| `-Oz` | `forceattrs,default<O2>`, атрибуты `optsize`, `minsize` | `-O=2` |

LLVM 23 удалил pipeline `default<Os>` и `default<Oz>`, предлагая выражать
приоритет размера через атрибуты функций. Компилятор использует этот способ на
LLVM 19/21/23, включая переданные `.ll` и `.bc`. Другие уровни, например
`-Ofast`, в новом режиме отклоняются. Native backend по-прежнему игнорирует
параметры оптимизации.

IR проверяется через `-verify-each`. В `opt` и `llc` задан CPU `x86-64`,
машинный вывод использует `-relocation-model=pic`. `-c` создаёт ELF-объект
непосредственно через `llc -filetype=obj`; сборка не проходит через текстовый
ассемблер и системный GNU as. Ассемблерный листинг `-S` можно сохранить для
изучения, в том числе с новыми директивами LLVM 23 вроде `.prefalign`.

Каждая единица трансляции оптимизируется отдельно. Объединение bitcode,
internalize и сбор/применение PGO остаются явными шагами исследовательских
целей `llvm-dispatch-demo`, `llvm-pgo-demo` и соответствующих бенчмарков.
Эта команда не включает LTO или PGO автоматически.

## Выбор инструментов

Для автоматической компиляции нужны `opt` и `llc` одной major-версии, не ниже
19. Текущая линия — 23.1.x; проверки выполнены с 19.1.7, 21.1.8 и 23.1.6.
Компилятор запускает выбранные инструменты; отдельного автоматического
согласования версий при каждом вызове нет.

По умолчанию используются `opt` и `llc` из `PATH`. Для версионных имён или
нестандартных путей задайте **переменные окружения**:

```sh
export PPCHIBICC_LLVM_OPT=/path/to/opt-23
export PPCHIBICC_LLVM_LLC=/path/to/llc-23
bin/ppchibicc --backend=llvm -O2 examples/llvm/ppc-floating.c -o build/figures
build/figures
```

Значение — путь к одной программе, без дополнительных аргументов. Пробелы
в пути поддерживаются. Параметры CMake с такими же именами выбирают инструменты
для тестовых целей; при отдельном запуске компилятора используются окружение
и `PATH`.

`llvm-as` не требуется для этой цепочки: `opt` читает текстовый IR сам.
Clang и compiler-rt для обычной компиляции также не требуются. Линковка
использует системный `ld`, стартовые объекты и библиотеки C toolchain, как
у исходного компилятора. Native-сборка, `-E`, `-M`, исходный IR и линковка уже
готовых объектов не вызывают LLVM.

Параметр `-###` показывает **и выполняет** команды стадий, как в прежнем
компиляторе. Он позволяет увидеть фактический pipeline, пути инструментов
и временные файлы. При ошибке frontend, `opt` или `llc` временные файлы
удаляются; запрошенный `.o`/`.s` открывается после успешного завершения
обеих внешних стадий. Существующий файл сохраняется при такой ошибке.

## Модули и библиотеки

В одной команде можно компилировать несколько C-файлов, добавлять готовые
native/LLVM объекты, `.a`, `.so`, аргументы `-L`, `-l` и `-Wl,...`:

```sh
bin/ppchibicc --backend=llvm -O2 \
  examples/llvm/pgo/base.c examples/llvm/pgo/handlers.c \
  examples/llvm/pgo/main.c examples/llvm/pgo/extension.c -o build/dispatch
build/dispatch 4 7
```

Ожидается `sum=40.00 visits=1 retagged=1`: обработчик меняет специализацию
объекта между итерациями. Теги, common-символы и порядок конструкторов
сохраняются между отдельно скомпилированными объектами.

Для архивов с регистрационными обработчиками 4П используйте прежний способ
подключения всех нужных объектных модулей:

```sh
bin/ppchibicc --backend=llvm main.c \
  -Wl,--whole-archive shapes.a -Wl,--no-whole-archive -o program
```

Обычный поиск символов в архиве может не извлечь модуль, который нужен только
ради конструктора регистрации. Аргументы `-shared`, `-static` и входной
ассемблер `.s` также поддерживаются обычными стадиями компилятора. `-c` и `-S`
останавливают процесс перед линковкой, включая команды с готовыми объектами.

## Пример с CMake

После сборки самого ppchibicc настройте отдельное приложение:

```sh
cmake -S examples/llvm/build-demo -B build-llvm-app \
  -DCMAKE_C_COMPILER="$PWD/bin/ppchibicc" \
  -DCMAKE_C_FLAGS="--backend=llvm -O2"
cmake --build build-llvm-app
ctest --test-dir build-llvm-app --output-on-failure
```

Если инструменты заданы окружением, экспортируйте его перед конфигурацией
и сборкой. Проект собирает hello, вещественный пример 4П и многомодульную
программу; четыре проверки включают изменение данных и смену специализации.
CMake определяет compiler ID как `unknown`; приведённый проект успешно
проходит определение ABI, сборку и CTest без принудительного задания GNU/Clang ID.

### Существующий пример `direct-classic/01-start`

В [CMakeLists.txt этого примера](../examples/evolution/figures/direct-classic/01-start/CMakeLists.txt)
компилятор выбирается до `project()`, а режим LLVM задаётся для target:

```cmake
set(CMAKE_C_COMPILER ppchibicc CACHE FILEPATH "Компилятор C")
project(evo-demo LANGUAGES C)

# После объявления add_executable(evo-demo ...):
target_compile_options(evo-demo PRIVATE --backend=llvm -O2)
target_link_options(evo-demo PRIVATE --backend=llvm)

set_source_files_properties(figures-input.c PROPERTIES
    COMPILE_OPTIONS --backend=x86-64
)
```

Это **смешанная сборка**: 14 C-файлов проходят через LLVM, а `figures-input.c`
компилируется исходным x86-64 backend. В этом файле используется `switch`,
который LLVM backend пока не поддерживает. Опции отдельного исходного файла
передаются после опций target, поэтому для него побеждает последний
`--backend=x86-64`. Исходные C-файлы менять не требуется.

Из корня репозитория, с уже собранным `bin/ppchibicc`, используйте отдельный
каталог сборки:

```sh
cmake -S examples/evolution/figures/direct-classic/01-start -B build-llvm-classic \
  -DCMAKE_C_COMPILER="$PWD/bin/ppchibicc"
cmake --build build-llvm-classic
build-llvm-classic/bin/evo-demo \
  examples/evolution/figures/direct-classic/01-start/data/input1.txt \
  build-llvm-classic/output1.txt
diff -u examples/evolution/figures/direct-classic/01-start/data/output1.txt \
  build-llvm-classic/output1.txt
```

`opt` и `llc` должны находиться в `PATH` либо выбираться экспортированными
переменными окружения, описанными выше. Программа находится в `bin` внутри
каталога сборки. Проверено на LLVM 23.1.6: вывод совпадает с эталоном
`data/output1.txt` на всех восьми фигурах, включая очистку контейнера.

## Проверки компилятора

```sh
cmake --build build-llvm23 --target test-llvm-driver
cmake --build build-llvm23 --target test-llvm test-llvm-stage2
```

Первый target проверяет уровни оптимизации, IR/stdin/stdout, модули 4П,
смешанные объекты, архивы, shared/static, зависимости и пути с пробелами,
кавычками, обратными слешами и кириллицей. Ошибки инструментов проверяются
с сохранением существующего результата и удалением временных файлов.
`test-llvm` и `test-llvm-stage2` дополнительно собирают все ABI/IEEE-фикстуры
самим компилятором при O0/O2 и сравнивают их с независимым эталоном.
