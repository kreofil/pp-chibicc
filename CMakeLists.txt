cmake_minimum_required(VERSION 3.16)

project(ppchibicc VERSION 0.1 LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not supported. Use: mkdir build && cd build && cmake ..")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(PPCHIBICC_SOURCES
  src/codegen.c
  src/codegen-llvm.c
  src/hashmap.c
  src/main.c
  src/parse.c
  src/preprocess.c
  src/strings.c
  src/tokenize.c
  src/type.c
  src/unicode.c
)

set(PPCHIBICC_STAGE1_DIR "${CMAKE_BINARY_DIR}/stage1")
set(PPCHIBICC_STAGE2_DIR "${CMAKE_BINARY_DIR}/stage2")
set(PPCHIBICC_PUBLIC_BIN_DIR "${CMAKE_SOURCE_DIR}/bin")
set(PPCHIBICC_PUBLIC_COMPILER "${PPCHIBICC_PUBLIC_BIN_DIR}/ppchibicc")

add_executable(ppchibicc_stage1 ${PPCHIBICC_SOURCES})
set_target_properties(ppchibicc_stage1 PROPERTIES
  OUTPUT_NAME ppchibicc
  RUNTIME_OUTPUT_DIRECTORY "${PPCHIBICC_STAGE1_DIR}"
)
target_compile_options(ppchibicc_stage1 PRIVATE
  -g
  -fno-common
  -Wall
  -Wno-switch
)

# Both build/stage1/ppchibicc and build/stage2/ppchibicc find the project
# headers through ../include. The public bin/ppchibicc finds source/include
# through the same relative rule.
file(REMOVE "${CMAKE_BINARY_DIR}/include")
file(CREATE_LINK "${CMAKE_SOURCE_DIR}/include" "${CMAKE_BINARY_DIR}/include"
  SYMBOLIC)

# bin/ppchibicc is a durable copy of a successful stage1 build. It is not
# declared as a CMake output, so the generated `make clean` intentionally
# leaves it in place.
add_custom_command(TARGET ppchibicc_stage1 POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${PPCHIBICC_PUBLIC_BIN_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "$<TARGET_FILE:ppchibicc_stage1>" "${PPCHIBICC_PUBLIC_COMPILER}"
  COMMENT "Updating bin/ppchibicc"
  VERBATIM
)

install(TARGETS ppchibicc_stage1 RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/" DESTINATION include)

function(add_ppc_test_binaries suite compiler compiler_dependency output_dir out_var)
  file(GLOB test_sources CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/test/*.c")
  set(test_executables)

  foreach(test_source IN LISTS test_sources)
    get_filename_component(test_name "${test_source}" NAME_WE)
    file(RELATIVE_PATH test_source_relative "${CMAKE_SOURCE_DIR}" "${test_source}")
    set(test_object "${output_dir}/${test_name}.o")
    set(test_executable "${output_dir}/${test_name}.exe")

    add_custom_command(
      OUTPUT "${test_object}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
      COMMAND "${compiler}"
              "-I${CMAKE_SOURCE_DIR}/include"
              "-I${CMAKE_SOURCE_DIR}/test"
              -c -o "${test_object}" "${test_source_relative}"
      DEPENDS ${compiler_dependency} "${test_source}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "Compiling ${suite} test ${test_name}"
      VERBATIM
    )

    add_custom_command(
      OUTPUT "${test_executable}"
      COMMAND "${CMAKE_C_COMPILER}" -pthread -o "${test_executable}"
              "${test_object}" -xc "${CMAKE_SOURCE_DIR}/test/common"
      DEPENDS "${test_object}" "${CMAKE_SOURCE_DIR}/test/common"
      COMMENT "Linking ${suite} test ${test_name}"
      VERBATIM
    )

    list(APPEND test_executables "${test_executable}")
  endforeach()

  set(multifile_dir "${output_dir}/ppc-multifile")
  set(multifile_sources base.c spec.c spec2.c forward-use.c main.c)
  set(multifile_objects)

  foreach(source_name IN LISTS multifile_sources)
    get_filename_component(object_name "${source_name}" NAME_WE)
    set(source_relative "test/ppc-multifile/${source_name}")
    set(source_path "${CMAKE_SOURCE_DIR}/${source_relative}")
    set(object_path "${multifile_dir}/${object_name}.o")

    add_custom_command(
      OUTPUT "${object_path}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${multifile_dir}"
      COMMAND "${compiler}"
              "-I${CMAKE_SOURCE_DIR}/include"
              "-I${CMAKE_SOURCE_DIR}/test"
              "-I${CMAKE_SOURCE_DIR}/test/ppc-multifile"
              -c -o "${object_path}" "${source_relative}"
      DEPENDS ${compiler_dependency} "${source_path}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "Compiling ${suite} multifile test ${source_name}"
      VERBATIM
    )

    list(APPEND multifile_objects "${object_path}")
  endforeach()

  set(multifile_executable "${multifile_dir}/ppc-multifile.exe")
  add_custom_command(
    OUTPUT "${multifile_executable}"
    COMMAND "${CMAKE_C_COMPILER}" -pthread -o "${multifile_executable}"
            ${multifile_objects} -xc "${CMAKE_SOURCE_DIR}/test/common"
    DEPENDS ${multifile_objects} "${CMAKE_SOURCE_DIR}/test/common"
    COMMENT "Linking ${suite} multifile test"
    VERBATIM
  )

  list(APPEND test_executables "${multifile_executable}")
  add_custom_target(${suite}_test_binaries DEPENDS ${test_executables})
  set(${out_var} "${test_executables}" PARENT_SCOPE)
endfunction()

add_ppc_test_binaries(
  stage1
  "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1
  "${CMAKE_BINARY_DIR}/test"
  STAGE1_TEST_EXECUTABLES
)

add_custom_target(check-stage1
  COMMAND "${CMAKE_SOURCE_DIR}/cmake/run-test-suite.sh"
          "${CMAKE_SOURCE_DIR}"
          "$<TARGET_FILE:ppchibicc_stage1>"
          ${STAGE1_TEST_EXECUTABLES}
  DEPENDS stage1_test_binaries
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Running stage1 test suite"
  USES_TERMINAL
  VERBATIM
)

enable_testing()
add_test(
  NAME stage1
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
          --target check-stage1
)

# Build the compiler sources with stage1, then link the resulting objects with
# the host compiler. This preserves the original chibicc self-hosting model.
set(STAGE2_OBJECTS)
foreach(source_file IN LISTS PPCHIBICC_SOURCES)
  get_filename_component(source_name "${source_file}" NAME_WE)
  set(stage2_object "${PPCHIBICC_STAGE2_DIR}/obj/${source_name}.o")

  add_custom_command(
    OUTPUT "${stage2_object}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PPCHIBICC_STAGE2_DIR}/obj"
    COMMAND "$<TARGET_FILE:ppchibicc_stage1>"
            -c -o "${stage2_object}" "${source_file}"
    DEPENDS ppchibicc_stage1
            "${CMAKE_SOURCE_DIR}/${source_file}"
            "${CMAKE_SOURCE_DIR}/src/chibicc.h"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Self-compiling ${source_file}"
    VERBATIM
  )

  list(APPEND STAGE2_OBJECTS "${stage2_object}")
endforeach()

set(PPCHIBICC_STAGE2_COMPILER "${PPCHIBICC_STAGE2_DIR}/ppchibicc")
add_custom_command(
  OUTPUT "${PPCHIBICC_STAGE2_COMPILER}"
  COMMAND "${CMAKE_C_COMPILER}" -std=c11 -g -fno-common -Wall -Wno-switch
          -o "${PPCHIBICC_STAGE2_COMPILER}" ${STAGE2_OBJECTS}
  DEPENDS ${STAGE2_OBJECTS}
  COMMENT "Linking stage2 ppchibicc"
  VERBATIM
)
add_custom_target(ppchibicc_stage2 DEPENDS "${PPCHIBICC_STAGE2_COMPILER}")

add_ppc_test_binaries(
  stage2
  "${PPCHIBICC_STAGE2_COMPILER}"
  ppchibicc_stage2
  "${PPCHIBICC_STAGE2_DIR}/test"
  STAGE2_TEST_EXECUTABLES
)

add_custom_target(test-stage2
  COMMAND "${CMAKE_SOURCE_DIR}/cmake/run-test-suite.sh"
          "${CMAKE_SOURCE_DIR}"
          "${PPCHIBICC_STAGE2_COMPILER}"
          ${STAGE2_TEST_EXECUTABLES}
  DEPENDS stage2_test_binaries
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Running stage2 test suite"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(test-examples
  COMMAND "${CMAKE_SOURCE_DIR}/cmake/run-example-tests.sh"
          "${CMAKE_SOURCE_DIR}"
          "$<TARGET_FILE:ppchibicc_stage1>"
  DEPENDS ppchibicc_stage1
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Running PPC example suite"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(test-all)
add_dependencies(test-all check-stage1 test-stage2 test-examples)

# Emitting textual IR needs no LLVM library. The external tools are optional
# and used only by explicitly requested LLVM tests and experiments.
set(PPCHIBICC_LLVM_VERSION "" CACHE STRING
    "LLVM tool suffix for optional tests (empty prefers unversioned tools)")
function(find_llvm_tool variable tool)
  if(PPCHIBICC_LLVM_VERSION)
    find_program(${variable} NAMES ${tool}-${PPCHIBICC_LLVM_VERSION} ${tool})
  else()
    find_program(${variable} NAMES ${tool} ${tool}-23 ${tool}-22 ${tool}-21 ${tool}-20 ${tool}-19)
  endif()
endfunction()
find_llvm_tool(PPCHIBICC_LLVM_AS llvm-as)
find_llvm_tool(PPCHIBICC_LLVM_OPT opt)
find_llvm_tool(PPCHIBICC_LLVM_LLC llc)
find_llvm_tool(PPCHIBICC_LLVM_LINK llvm-link)
find_llvm_tool(PPCHIBICC_LLVM_PROFDATA llvm-profdata)
find_llvm_tool(PPCHIBICC_LLVM_CLANG clang)
find_package(Python3 3.8 QUIET COMPONENTS Interpreter)
message(STATUS "LLVM tools (optional): llvm-as=${PPCHIBICC_LLVM_AS}; opt=${PPCHIBICC_LLVM_OPT}; llc=${PPCHIBICC_LLVM_LLC}; llvm-link=${PPCHIBICC_LLVM_LINK}")
message(STATUS "LLVM PGO tools (optional): llvm-profdata=${PPCHIBICC_LLVM_PROFDATA}; clang=${PPCHIBICC_LLVM_CLANG}")

function(add_llvm_target target script compiler dependency output_name description)
  set(tools_found TRUE)
  if(NOT PPCHIBICC_LLVM_AS OR NOT PPCHIBICC_LLVM_OPT OR NOT PPCHIBICC_LLVM_LLC)
    set(tools_found FALSE)
  endif()
  if(ARGC GREATER 6)
    math(EXPR last_tool_index "${ARGC} - 1")
    foreach(tool_index RANGE 6 ${last_tool_index})
      if(NOT ARGV${tool_index})
        set(tools_found FALSE)
      endif()
    endforeach()
  endif()
  if(tools_found)
    add_custom_target(${target}
      COMMAND "${CMAKE_SOURCE_DIR}/${script}"
              "${CMAKE_SOURCE_DIR}" "${compiler}" "${CMAKE_C_COMPILER}"
              "${PPCHIBICC_LLVM_AS}" "${PPCHIBICC_LLVM_OPT}"
              "${PPCHIBICC_LLVM_LLC}" "${CMAKE_BINARY_DIR}/${output_name}" ${ARGN}
      DEPENDS ${dependency}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "${description}"
      USES_TERMINAL
      VERBATIM
    )
  else()
    add_custom_target(${target}
      COMMAND "${CMAKE_COMMAND}" -E echo
              "LLVM target ${target}: required tools are missing. Set PPCHIBICC_LLVM_AS, PPCHIBICC_LLVM_OPT and PPCHIBICC_LLVM_LLC. Dispatch demos/reports also need PPCHIBICC_LLVM_LINK; reports/static demos need Python 3.8+ (Python3_EXECUTABLE). PGO targets (llvm-pgo-demo, llvm-pgo-bench, llvm-figures-bench) need LLVM/Clang 23+, PPCHIBICC_LLVM_LINK, PPCHIBICC_LLVM_PROFDATA, PPCHIBICC_LLVM_CLANG and the compiler-rt profile runtime. Benchmark targets also need Python 3.8+ (Python3_EXECUTABLE)."
      COMMAND "${CMAKE_COMMAND}" -E false
      VERBATIM
    )
  endif()
endfunction()

add_llvm_target(test-llvm cmake/run-llvm-tests.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-test "Checking LLVM IR, execution and ABI (test-llvm)")
add_llvm_target(test-llvm-stage2 cmake/run-llvm-tests.sh "${PPCHIBICC_STAGE2_COMPILER}"
  ppchibicc_stage2 llvm-test-stage2 "Checking LLVM IR, execution and ABI (test-llvm-stage2)")
add_llvm_target(test-llvm-driver cmake/run-llvm-driver-tests.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-driver-test "Checking the LLVM compiler driver")
add_llvm_target(llvm-opt-demo cmake/run-llvm-opt-demo.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-opt-demo "Comparing LLVM optimization pipelines")
add_llvm_target(llvm-dispatch-demo cmake/run-llvm-dispatch-demo.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-dispatch-demo "Comparing PPC dispatch and module visibility"
  "${PPCHIBICC_LLVM_LINK}")
add_llvm_target(llvm-dispatch-report cmake/run-llvm-dispatch-report.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-dispatch-report "Inspecting closed PPC dispatch across LLVM modules"
  "${PPCHIBICC_LLVM_LINK}" "${Python3_EXECUTABLE}")
add_llvm_target(test-llvm-dispatch-report-stage2 cmake/run-llvm-dispatch-report.sh "${PPCHIBICC_STAGE2_COMPILER}"
  ppchibicc_stage2 llvm-dispatch-report-stage2 "Checking self-hosted PPC dispatch metadata"
  "${PPCHIBICC_LLVM_LINK}" "${Python3_EXECUTABLE}")
add_llvm_target(llvm-static-dispatch-demo cmake/run-llvm-static-dispatch.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-static-dispatch-demo "Freezing closed PPC dispatch in a complete LLVM program"
  "${PPCHIBICC_LLVM_LINK}" "${Python3_EXECUTABLE}")
add_llvm_target(test-llvm-static-dispatch-stage2 cmake/run-llvm-static-dispatch.sh "${PPCHIBICC_STAGE2_COMPILER}"
  ppchibicc_stage2 llvm-static-dispatch-stage2 "Checking self-hosted static PPC dispatch"
  "${PPCHIBICC_LLVM_LINK}" "${Python3_EXECUTABLE}")
add_llvm_target(llvm-static-dispatch-bench cmake/run-llvm-static-dispatch-bench.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-static-dispatch-bench "Measuring closed LLVM dispatch pipelines"
  "${PPCHIBICC_LLVM_LINK}" "${Python3_EXECUTABLE}")
add_llvm_target(test-llvm-static-dispatch-bench cmake/run-llvm-static-dispatch-bench.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-static-dispatch-bench-check "Checking closed dispatch benchmark results"
  "${PPCHIBICC_LLVM_LINK}" "${Python3_EXECUTABLE}" "--check-only")
add_llvm_target(test-llvm-static-dispatch-bench-stage2 cmake/run-llvm-static-dispatch-bench.sh "${PPCHIBICC_STAGE2_COMPILER}"
  ppchibicc_stage2 llvm-static-dispatch-bench-check-stage2 "Checking self-hosted closed dispatch benchmark"
  "${PPCHIBICC_LLVM_LINK}" "${Python3_EXECUTABLE}" "--check-only")
add_llvm_target(llvm-pgo-demo cmake/run-llvm-pgo-demo.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-pgo-demo "Profiling PPC dispatch and testing indirect-call promotion"
  "${PPCHIBICC_LLVM_LINK}" "${PPCHIBICC_LLVM_PROFDATA}" "${PPCHIBICC_LLVM_CLANG}")
add_llvm_target(llvm-pgo-bench cmake/run-llvm-pgo-bench.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-pgo-bench "Measuring native, LLVM O2 and PGO dispatch"
  "${PPCHIBICC_LLVM_LINK}" "${PPCHIBICC_LLVM_PROFDATA}" "${PPCHIBICC_LLVM_CLANG}"
  "${Python3_EXECUTABLE}")
add_llvm_target(llvm-figures-bench cmake/run-llvm-figures-bench.sh "$<TARGET_FILE:ppchibicc_stage1>"
  ppchibicc_stage1 llvm-figures-bench "Measuring collection sorting with held-out PGO workloads"
  "${PPCHIBICC_LLVM_LINK}" "${PPCHIBICC_LLVM_PROFDATA}" "${PPCHIBICC_LLVM_CLANG}"
  "${Python3_EXECUTABLE}")

add_custom_target(distclean
  COMMAND "${CMAKE_COMMAND}"
          "-DSOURCE_DIR:PATH=${CMAKE_SOURCE_DIR}"
          "-DBINARY_DIR:PATH=${CMAKE_BINARY_DIR}"
          -P "${CMAKE_SOURCE_DIR}/cmake/distclean.cmake"
  COMMENT "Removing build and bin directories"
  USES_TERMINAL
  VERBATIM
)
