# Самокомпиляция ppchibicc через LLVM

ppchibicc собирает собственные десять C-модулей через свой LLVM-бэкенд.
Полученный компилятор снова собирает те же исходники. На Linux x86-64
проверяются совпадение поколений, оба генератора, C/4П и закрытая сборка.

| Поколение | Как получено |
| --- | --- |
| stage1 | Системный C-компилятор собирает ppchibicc |
| stage2 | stage1 формирует IR; `opt` и `llc` создают объекты |
| stage3 | Построенный через LLVM stage2 повторяет эту сборку |

Объекты связывает системный C-компилятор с обычным C runtime.
Библиотеки LLVM не подключаются: вызываются внешние инструменты.

При одинаковых исходниках, опциях и инструментах stage2 и stage3 должны
дать побайтно одинаковые IR и `.o`. Это проверка устойчивости bootstrap:
компилятор, построенный через LLVM, воспроизводит результат предыдущего
поколения. Хеши бинарников также сохраняются; в проверенных сборках они
совпали. Равенство бинарников отдельно не требуется: детали компоновки
могут зависеть от системного инструментария.

## Запуск

Из корня репозитория, с LLVM 23 в `PATH`:

```sh
cmake -S . -B build-llvm-selfhost \
  -DPPCHIBICC_STAGE2_BACKEND=llvm \
  -DPPCHIBICC_LLVM_VERSION=23
cmake --build build-llvm-selfhost --target llvm-selfhost -j
```

Полученные компиляторы находятся в `build-llvm-selfhost/stage2/ppchibicc`
и `build-llvm-selfhost/stage3/ppchibicc`, IR — в `stage2/ir` и `stage3/ir`.
Отчёты: `build-llvm-selfhost/llvm-selfhost/report.md` и `report.json`.

```sh
build-llvm-selfhost/stage3/ppchibicc --backend=llvm -O2 \
  examples/llvm/ppc-vararg-items.c -o build-llvm-selfhost/items
build-llvm-selfhost/items
```

Ожидается `normal=13.0 scaled=39.0`.

`PPCHIBICC_STAGE2_BACKEND` выбирает способ сборки самого компилятора.
Бэкенд пользовательской программы выбирается обычным `--backend=llvm`
или `--backend=x86-64`; значение по умолчанию остаётся x86-64.
`bin/ppchibicc` и обычная установка CMake используют stage1.
Build сохраняет относительный поиск `include` и `share/ppchibicc` для
stage2/stage3, в том числе для закрытой сборки.

## Оптимизация и инструменты

LLVM-поколения по умолчанию строятся с `-O2` и `-fno-common`. Для O0:

```sh
cmake -S . -B build-llvm-selfhost-O0 \
  -DPPCHIBICC_STAGE2_BACKEND=llvm \
  -DPPCHIBICC_STAGE2_LLVM_OPTIMIZATION=0 \
  -DPPCHIBICC_LLVM_VERSION=23
cmake --build build-llvm-selfhost-O0 --target llvm-selfhost -j
```

Допустимые уровни: `0`, `1`, `2`, `3`, `s`, `z`. Равенство поколений
проверяется отдельно внутри выбранной конфигурации.

Для LLVM-stage2 нужны `opt` и `llc`, для отчёта — также `llvm-as` и Python
3.8+. Полный набор закрытой сборки использует `llvm-link`.
`PPCHIBICC_LLVM_AS`, `PPCHIBICC_LLVM_OPT`, `PPCHIBICC_LLVM_LLC`,
`PPCHIBICC_LLVM_LINK`, `Python3_EXECUTABLE` задают точные пути.
Отчёт требует совпадения major-версий as/opt/llc и LLVM 19+.

По умолчанию `PPCHIBICC_STAGE2_BACKEND=x86-64`: прежняя native-сборка
stage2 и `test-all` доступны без LLVM-инструментов.

## Полная проверка

```sh
cmake --build build-llvm-selfhost --target test-llvm-selfhost -j
```

Цель объединяет:

- `llvm-selfhost`: верификацию IR всех десяти модулей, сравнение IR/объектов
  stage2/stage3 и native/LLVM-генерации пяти C/4П-фикстур у seed/stage2/stage3,
  16 запусков примеров, проверки hashmap и диагностик;
- `test-stage3`: основной C/4П-набор, многофайловые тесты и CLI;
- `test-examples-stage3`: эволюционные примеры 4П;
- `test-llvm-stage3`: полный LLVM-набор, O0/O2, публичный компилятор,
  взаимные вызовы с системным C-компилятором и смешанные модули;
- `test-llvm-closed-compiler-stage3`: закрытые программы и интеграцию CMake.

Прежние цели stage2 также доступны: здесь они используют компилятор,
построенный через LLVM. `test-all` сохраняет прежний состав целей;
stage3 проверяется отдельной целью.

Отчёт относится к сравнению поколений и фокусным проверкам. Полные наборы —
отдельные зависимости `test-llvm-selfhost`. JSON содержит версии и пути
инструментов, хеши компиляторов, исходников, заголовков, IR, объектов и команды.
При ошибке сравнения или фокусных проверок статус становится `failed`,
цель завершается с ненулевым кодом.

## Результат и границы

На LLVM 23.1.6 при O0/O2 совпали IR, объекты всех десяти модулей и бинарники
stage2/stage3. На собранном через LLVM stage3 при O2 прошли 69 C/4П-программ, CLI,
LLVM O0/O2, примеры и закрытая сборка. Фокусная проверка bootstrap, генерации
и примеров также прошла на LLVM 19.1.7 и 21.1.8 при O2.
Это функциональные проверки; производительность компилятора не измерялась.

Границы C перечислены в [руководстве LLVM-бэкенда](llvm-backend-ru.md).
Язык 4П, ABI регистрации, метаданные v2 и правила закрытой диспетчеризации
сохраняются. Отдельная утилита преобразования открытых обобщений в закрытые
остаётся отложенной идеей.
