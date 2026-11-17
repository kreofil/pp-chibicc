cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# Компилятор выбирается до project(). Путь можно задать через
# -DCMAKE_C_COMPILER=/absolute/path/to/ppchibicc.
set(CMAKE_C_COMPILER ppchibicc CACHE FILEPATH "Компилятор C")
project(evo-demo LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Тип сборки")

# Исполняемый файл находится внутри выбранного каталога сборки.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Установка переменной со списком исходников
set(SOURCE_FILES
    # headers
    figure.h
    rectangle.h
    figure-rectangle.h
    triangle.h
    figure-triangle.h
    container.h

    #sources
    figure-in.c
    figures-input.c
    figure-out.c
    figure-rectangle-in.c
    figure-rectangle-out.c
    figure-triangle-in.c
    figure-triangle-out.c
    rectangle-in.c
    rectangle-out.c
    triangle-in.c
    triangle-out.c
    container-constr.c
    container-in.c
    container-out.c
    main.c
)

add_executable(evo-demo ${SOURCE_FILES})

# Основной режим компиляции: LLVM с уровнем O2.
target_compile_options(evo-demo PRIVATE --backend=llvm -O2)
target_link_options(evo-demo PRIVATE --backend=llvm)

enable_testing()
add_test(NAME figures-run
    COMMAND evo-demo "${CMAKE_CURRENT_SOURCE_DIR}/data/input1.txt"
                     "${CMAKE_CURRENT_BINARY_DIR}/output1.txt")
add_test(NAME figures-output
    COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_SOURCE_DIR}/data/output1.txt"
            "${CMAKE_CURRENT_BINARY_DIR}/output1.txt")
set_tests_properties(figures-output PROPERTIES DEPENDS figures-run)
