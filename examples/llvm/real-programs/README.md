# Real 4P programs

The `upstream/` subset is copied byte for byte from
[kreofil/4P-examples](https://github.com/kreofil/4P-examples) at commit
`9e8f1f1c07ef08e845f877c8dac7a804ef9c0af4`. `upstream.json` records original
paths and SHA256 values. Only the source files required for the three selected
programs are included; original comments and source text are preserved.

- `simplest/spec-param`: ordinary parameters/results carrying specialized
  values and pointers. The original abstract `Print` omits the tag-0 handler,
  so the current completeness protocol rejects it before `main`.
- `patterns/shvets-patterns/visitor-mm.ppc`: a two-argument visitor with two
  open case generalizations, heap objects and four concrete combinations.
- `containers/vector-list-lib`: integer vector construction, growth with
  `reallocarray`, generic printing, sum, sorting through a comparator, and
  destruction across three translation units.

`spec-param-covered.c` is our separate test adapter. It includes the unchanged
original with `main` renamed, defines a concrete `Figure.void` handler, invokes
it, then runs the original main. It does not close any open generalization.
The expected text files are manually specified from the examples' operations;
the vector reference is calculated independently by the Python runner.

Run `cmake --build build --target llvm-real-programs` to compare native,
per-unit LLVM O0/O2 and closed-program O0/O2. The original startup rejection
is an expected negative case. The stage2 equivalent is
`test-llvm-real-programs-stage2`. No network download is needed for these tests.
See [the Russian guide](../../../docs/llvm-real-programs-ru.md).
