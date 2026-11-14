cmake_minimum_required(VERSION 3.16)
project(ppchibicc_closed_build LANGUAGES C)
get_filename_component(PPCHIBICC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${PPCHIBICC_ROOT}/cmake/PpchibiccClosedProgram.cmake")
set(CLOSED_REGISTRY_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../closed-registry" CACHE PATH
    "Sources for the closed-registry example")

add_library(registry_handlers OBJECT
  "${CLOSED_REGISTRY_SOURCE_DIR}/special.c"
  "${CLOSED_REGISTRY_SOURCE_DIR}/default.c")
ppchibicc_enable_closed_program(registry_handlers)

add_executable(closed-registry "${CLOSED_REGISTRY_SOURCE_DIR}/main.c")
target_link_libraries(closed-registry PRIVATE registry_handlers)
ppchibicc_enable_closed_program(closed-registry OPTIMIZATION 2 FAMILIES registry_combine)

enable_testing()
add_test(NAME closed-registry COMMAND closed-registry)
set_tests_properties(closed-registry PROPERTIES PASS_REGULAR_EXPRESSION "closed registry OK")
