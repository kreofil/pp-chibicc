[readme in russian](README-ru.md)

# ppchibicc: A procedural-parametric extension of the C language built upon the chibicc compiler
ppchibicc is an attempt to implement a compiler for the PPC programming language on top of the [chibicc](https://github.com/rui314/chibicc) compiler using "vibe coding." The PPC language extends C by adding constructs that support the procedural-parametric programming (PPP) paradigm. A description of this paradigm and its capabilities can be found on the [https://www.softcraft.ru/](https://www.softcraft.ru/) website, in the section dedicated to the [procedural-parametric programming paradigm](https://www.softcraft.ru/ppp/).

The choice of chibicc stems from the goal of creating a lightweight compiler suitable for educational and experimental purposes. Additionally, it serves as a testbed for new language constructs that will eventually be integrated into the [ppclang](https://gitverse.ru/kpdev/llvm-project) compiler—an extension of the [clang](https://clang.llvm.org/) compiler.

Along with the compiler, the repository contains examples demonstrating the capabilities of the procedural-parametric programming paradigm.

The documentation describes the syntax and semantics of the C language extensions implemented so far. The documentation and examples showcasing PPC's capabilities will be expanded as new constructs are implemented.

The current version of the compiler is being developed for the Linux operating system.

## Options for configuring the compiler after building it

This is not the only approach, but to avoid modifying the entire system, you can do the following:

1. Add the `~/bin` directory to your PATH environment variable.
2. Create a wrapper script in the `~/bin` directory named after the compiler—for example, `ppchibicc`:

```
#!/bin/sh
exec /path/to/chibicc-ppc/chibicc -I/path/to/chibicc-ppc/include "$@"
```

3. Make the script executable:

```
chmod +x ~/bin/ppchibicc
```

You can then invoke C programs using this command. For example:

```
ppchibicc main.c -o main
```

For CMake, you can then specify:

```
SET(CMAKE_C_COMPILER ppchibicc)
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

## Verification after cloning

```
git clone https://github.com/kreofil/pp-chibicc.git
cd pp-chibicc
make test-all
```