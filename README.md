[readme in russian](README-ru.md)

# ppchibicc: A procedural-parametric extension of the C language built upon the chibicc compiler
ppchibicc is an attempt to implement a compiler for the PPC programming language on top of the [chibicc](https://github.com/rui314/chibicc) compiler using "vibe coding." The PPC language extends C by adding constructs that support the procedural-parametric programming (PPP) paradigm. A description of this paradigm and its capabilities can be found on the [https://www.softcraft.ru/](https://www.softcraft.ru/) website, in the section dedicated to the [procedural-parametric programming paradigm](https://www.softcraft.ru/ppp/).

The choice of chibicc stems from the goal of creating a lightweight compiler suitable for educational and experimental purposes. Additionally, it serves as a testbed for new language constructs that will eventually be integrated into the [ppclang](https://gitverse.ru/kpdev/llvm-project) compiler—an extension of the [clang](https://clang.llvm.org/) compiler.

Along with the compiler, the repository contains examples demonstrating the capabilities of the procedural-parametric programming paradigm.

The documentation describes the syntax and semantics of the C language extensions implemented so far. The documentation and examples showcasing PPC's capabilities will be expanded as new constructs are implemented.

The current version of the compiler is being developed for the Linux operating system.

[LLVM self-hosting](docs/llvm-selfhost-ru.md) is available in a separate CMake
build with `-DPPCHIBICC_STAGE2_BACKEND=llvm`. The `test-llvm-selfhost` target
compares stage2/stage3 IR and objects and tests the LLVM-built compiler.

## Building the compiler

The project uses an out-of-source CMake build. Compiler sources stay in `src`,
intermediate results are written to `build`, and the successful user-facing
compiler is copied to `bin/ppchibicc`.

```sh
git clone https://github.com/kreofil/pp-chibicc.git
cd pp-chibicc
mkdir build
cd build
cmake ..
make -j
```

After a successful build, invoke the compiler as:

```sh
../bin/ppchibicc main.c -o main
```

The compiler automatically finds the project headers in the adjacent
`include` directory. To invoke it permanently by name, add the project `bin`
directory to `PATH`:

```sh
export PATH="/path/to/pp-chibicc/bin:$PATH"
```

You can then compile a program directly:

```sh
ppchibicc main.c -o main
```

Another CMake project can select it with:

```cmake
set(CMAKE_C_COMPILER ppchibicc)
```

If you wish to use a different file extension—for example, `ppc`—you must invoke the compiler with the `-xc` option. For instance:

```
ppchibicc -xc main.c -o main
```

In CMake, you also need to specify the following:

```
set_source_files_properties(
${SOURCE_FILES}
PROPERTIES
LANGUAGE C
)
```

## Verification and cleanup

```sh
cd build
make test       # main stage1 tests
make test-all   # stage1, stage2, and examples
make clean      # remove build products but keep ../bin/ppchibicc
make distclean  # also remove build and bin/ppchibicc
```

The top-level Makefile remains as a short wrapper for the same commands, so
`make`, `make test`, and `make test-all` also work from the repository root.
