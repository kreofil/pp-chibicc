cmake_minimum_required(VERSION 3.16)
project(ppchibicc_llvm_build_demo LANGUAGES C)

add_executable(llvm-hello ../hello.c)
add_executable(llvm-figures ../ppc-floating.c)
add_executable(llvm-dispatch
  ../pgo/base.c
  ../pgo/handlers.c
  ../pgo/main.c
  ../pgo/extension.c
)

enable_testing()
add_test(NAME hello COMMAND llvm-hello)
set_tests_properties(hello PROPERTIES PASS_REGULAR_EXPRESSION "Hello from ppchibicc LLVM IR")
add_test(NAME figures COMMAND llvm-figures)
set_tests_properties(figures PROPERTIES PASS_REGULAR_EXPRESSION "scaled=49.00")
add_test(NAME dispatch COMMAND llvm-dispatch 3 7)
set_tests_properties(dispatch PROPERTIES PASS_REGULAR_EXPRESSION "sum=43.00 visits=4 retagged=0")
add_test(NAME retag COMMAND llvm-dispatch 4 7)
set_tests_properties(retag PROPERTIES PASS_REGULAR_EXPRESSION "sum=40.00 visits=1 retagged=1")
