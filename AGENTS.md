# Agent Instructions

This repository is an experimental PPC/4P extension of chibicc.

## Main Rules

- Preserve ordinary C compatibility unless a PPC feature explicitly changes parsing or code generation.
- Keep PPC changes small and covered by tests.
- Prefer regression tests under `test/ppc.c` or `test/ppc-multifile`.
- Do not remove existing PPC examples when changing compiler internals.
- Update `PPC_NOTES.md` after each semantic or ABI change.

## Validation

Run:

```sh
make test      # быстрый основной тест
make test-all  # полный тест со stage2
```