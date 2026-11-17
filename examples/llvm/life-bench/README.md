# Bounded toroidal Life benchmark

The runner copies `map.c`, `eworld.c` and their headers unchanged from
`examples/games/conway-life/tor/conway-life-poly-tor/src`. `life.c` supplies
seeded in-memory maps and bounded simulation rounds; `timer.c` replaces the
interactive loop, terminal output and sleeps. No source generalization is
closed: all four original `Cell` families must stay on their runtime path.

Variants: native x86-64, separate LLVM O2 units, combined LLVM O2 through
`--closed-program`. The timer is compiled by each pipeline. The `life_run`
round boundary is marked `noinline` in raw LLVM IR and verified after O2;
inner simulation functions remain eligible for ordinary optimization.

The runner checks an independent Python cellular-automaton model, including
eight round phases, small tori, fingerprints, population and step counters,
then optionally calibrates and times equal work using `llvm_bench.py`.

```
cmake --build build --target test-llvm-life-bench
cmake --build build --target llvm-life-bench
cat build/llvm-life-bench/report.md
```

See [the Russian guide](../../../docs/llvm-life-benchmark-ru.md) for measurement
boundaries and the native large-copy/memory-return fixes found by this workload.
