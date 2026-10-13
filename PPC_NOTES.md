# PPC extension snapshot 19

This snapshot starts a Procedural-Parametric C experiment on top of
`rui314/chibicc` commit `90d1f7f199cc55b13c7fdb5839d1409806633fdb`.

Implemented so far:

- `struct Name { ... } case { ... };`
- closed generalizations with `struct Name { ... } case { ... } const;`
- zero-disabled generalizations with `struct Name { ... } case { ... } = 0;`
  and `struct Name { ... } case { ... } const = 0;`
- empty case lists, for example `struct Figure {} case {};`
- decentralized extension with `struct Name + { ... };`
- decentralized extension through a typedef name, for example `Figure2 + { ... };`
- specialization types such as `struct Figure.triangle` and `Figure2.rect`
- specialization chains for variables, for example
  `Shape.color.rect` and recurrent chains such as `Shape.color.color.rect`
- specializations without an instance body, written as `void Name;` inside the
  `case` or extension list
- field access through the specialization member, for example `t.triangle.a`
- common-field access through the generated specialization type
- a hidden first `int` field holding the runtime PPC specialization tag
- automatic specialization-tag initialization for local, global, and static
  PPC variables, including nested tags inside specialization chains
- user-facing aggregate initializers skip the hidden tag field, so
  `struct WeekDay.Monday monday = {35};` initializes the first visible common
  field rather than the hidden tag
- PPC specializations support ordinary aggregate initialization styles:
  compact positional initialization, grouped specialization-body
  initialization, and C99 designated initialization, for example
  `{3, 4, 5}`, `{3, {4, 5}}`, and `{.color=3, .rectangle={.x=4, .y=5}}`
- one-parameter procedural-parametric function declarations, for example
  `int FigureCode<struct Figure *f>() = 0;`
- multi-parameter procedural-parametric function declarations, for example
  `int FigurePairCode<struct Figure *a, struct Figure *b>() = 0;`
- specialization handlers, for example
  `int FigureCode<struct Figure.rectangle *r>() { ... }`
- PPC handler declarations and definitions can be split across translation
  units; generated handler function symbols now use external linkage by
  default, like ordinary C functions, unless the source handler is explicitly
  `static`
- generated PPC handler symbol names use stable specialization type names
  instead of local numeric specialization indices, so separately compiled
  handlers such as `Figure.rect` and `Figure.trian` no longer collide when both
  are locally seen as specialization index 1
- PPC declarations and calls through a pointer to a named incomplete
  generalization are supported, for example `typedef struct Figure Figure;`
  followed by `void FigureOut<Figure *f>(FILE *out);` and
  `FigureOut<ptr>(out);`
- multimethod handlers, for example
  `int FigurePairCode<struct Figure.triangle *a,
                       struct Figure.rectangle *b>() { ... }`
- the synthetic zero-index subtype `Base.void` in PPC handler parameter
  types, for example `int FigureCode<struct Figure.void *f>() { ... }`
  and `int FigurePairCode<struct Figure.void *a,
                          struct Figure.rectangle *b>() { ... }`
- parametric calls with a specialization pointer, for example
  `FigureCode<&r>()`
- parametric calls with several PPC arguments, for example
  `FigurePairCode<pf1, pf2>()`
- parametric calls with a generalization pointer, for example
  `FigureCode<pf>()`
- ordinary arguments after the PPC parameter, for example
  `FigureColorPlus<pf>(10)`
- runtime registration across translation units for the current
  one-PPC-parameter function model
- generated finalizers for dynamically allocated PPC dispatch tables
- diagnostic runtime handlers for uncovered abstract PPC dispatch cells
- startup completeness checks for PPC functions whose dispatch axes are all
  closed generalizations and that have no concrete all-general default handler
- `create_spec(type-name)` for heap-creating PPC generalizations,
  specializations, and specialization chains with all reachable hidden runtime
  tags initialized
- `create_spec(type-name[N])` for heap-creating an array of PPC
  generalizations, specializations, or specialization chains; all reachable
  hidden runtime tags are initialized in every array element and the expression
  returns a pointer to the first element
- `init_spec(type-name, pointer)` for initializing all reachable hidden runtime
  tags in an already allocated PPC object or memory area
- `init_spec(type-name[N], pointer)` for initializing all reachable hidden
  runtime tags in every element of an already allocated PPC array or memory area
- `get_spec_size(generalization-type)` for reading the runtime number of
  user-visible PPC specialization indices
- `get_spec_ptr(generalization-type, index)` for obtaining a stable tag-only
  pseudo pointer that can be passed as a PPC dispatch argument
- `spec_index_cmp(pointer, pointer)` for comparing two PPC specialization
  indices from the same generalization family

Current implementation strategy:

- A PPC generalization stores its specialization list in the `Type` object.
- A specialization such as `struct Figure.triangle` is represented as a normal
  struct type generated from the hidden tag field, the common members of
  `Figure`, and the selected specialization member.
- A chain such as `Shape.color.rect` is generated by specializing the outer
  type first and then recursively specializing the selected member that can
  provide the next name. If more than one nested member can provide that name,
  the compiler reports an ambiguous nested PPC specialization.
- Recurrent decorator-like chains remain finite structural types. For example,
  `Shape.color.color.rect` contains the outer `Shape.color` level, whose
  decorator member contains another specialized `Shape.color.rect` level.
- Initializers reuse the ordinary chibicc C aggregate-initializer machinery.
  Hidden PPC tag fields are skipped by positional and designated initializers,
  so a specialization may be initialized either as one flat field sequence, as
  grouped common/specialization parts, or with named fields. For chain types,
  named initialization can use nested aggregate names, for example
  `{.color={.color=7, .fig={.rect={10, 16}}}}`.
- A `void` specialization such as `void Monday;` has no selected specialization
  member. Its generated struct contains only the hidden tag and the common
  members of the generalization, which allows enum-like evolutionary extension.
- `Base.void` is a separate synthetic specialization type for PPC handler
  parameters. It is not taken from the `case` list and always denotes tag 0,
  i.e. the generalization object itself as a concrete dispatch-table coordinate.
- A `const` generalization has a closed specialization set. Later `+`
  extensions of such a generalization are rejected.
- A generalization declared with `= 0` disables the source-level zero
  specialization. Direct objects of that generalization type, arrays of such
  objects, `Base.void`, `create_spec(Base)`, `init_spec(Base, ptr)`, and
  ordinary function parameters or return values that would pass such objects by
  value are rejected. Pointers to the generalization remain valid and can be
  used for default PPC handlers.
- The hidden tag field is the first struct member. The generalization uses
  tag 0; concrete specializations receive tags 1..N at program startup. For
  `= 0` generalizations, tag 0 remains a technical table coordinate but is not
  a valid source-level specialization.
- Each generalization has a common runtime counter:
  `__ppc_<type>_count`.
- Generalizations used with `get_spec_ptr` also have a common runtime
  tag-carrier array pointer:
  `__ppc_<type>_spec_ptr_tags`.
- Each specialization has a common runtime tag cell:
  `__ppc_<type>_<specialization>_tag`.
- Specialization constructors in `.init_array.101` assign tags by incrementing
  the common counter. They are idempotent, so a PPC header included by several
  translation units does not register the same specialization more than once.
- Tag-carrier constructors in `.init_array.151` allocate and fill the
  `get_spec_ptr` array after specialization tags have been assigned and before
  dispatch tables are allocated.
- Each PPC function has a common dispatch table:
  `__ppc_<function>_table`.
- For functions with several PPC parameters, the dispatch table is stored as a
  flat one-dimensional array. The runtime table size is:
  `(count1 + 1) * (count2 + 1) * ... * (countN + 1)`.
- A multimethod call computes the flat index in row-major order:
  `(((tag1 * extent2) + tag2) * extent3 + tag3) ...`, where
  `extentK = countK + 1`.
- PPC handler identity is now keyed by both the numeric tag coordinates and a
  wildcard mask. Thus `struct Figure *` and `struct Figure.void *` are distinct:
  the former is the all-table default coordinate, while the latter is the
  concrete zero tag coordinate.
- Table constructors in `.init_array.201` allocate and fill the table with the
  all-generalizing/default handler, for example
  `FigurePair<struct Figure *a, struct Figure *b>() { ... }`. If the
  generalizing handler is abstract or missing, the table is filled with a
  generated diagnostic handler.
- If every PPC parameter of a function belongs to a closed generalization and
  the function has no concrete all-general default handler, the table
  constructor leaves cells empty and a later `.init_array.351` completeness
  checker aborts before `main` if any specialization tuple is still uncovered.
- If any PPC parameter belongs to a `= 0` generalization, a
  `.init_array.341` constructor overwrites all dispatch-table cells whose
  coordinate is zero on that axis with a generated diagnostic handler. Dense
  table sizes are not compressed; zero-coordinate slots remain present as
  runtime error slots.
- The diagnostic handler has the same generic PPC signature as the table entry.
  It prints the PPC function name and the actual runtime tag coordinates, for
  example `PPC dispatch error: uncovered handler for Missing<0, 1>()`, and then
  calls `abort()`.
- `create_spec(struct Figure.rectangle)` lowers to an internal expression that
  allocates `sizeof(struct Figure.rectangle)` bytes with `calloc`, writes every
  reachable hidden tag field from the generated PPC type, and returns
  `struct Figure.rectangle *`. Passing a generalization type or `Base.void`
  creates a tag-0 object unless the generalization was declared with `= 0`, in
  which case the compiler rejects the expression.
- `create_spec(Shape.color.rect)` and recurrent forms such as
  `create_spec(Shape.color.color.rect)` use the same type-chain parser as
  variable declarations and initialize every tag in the allocated chain.
- `create_spec(Shape.color.rect[2])` allocates `sizeof(Shape.color.rect[2])`
  bytes, initializes the tag chain in each element, and returns
  `Shape.color.rect *` pointing at the first element.
- `init_spec(struct Figure.rectangle, ptr)` casts `ptr` to the requested PPC
  type, writes every reachable hidden tag field, and returns `void`. It accepts
  pointers from the same PPC generalization family, plus `void *` for arbitrary
  memory that the caller guarantees is large and aligned enough for the
  requested PPC type.
- `init_spec(Shape.color.rect, ptr)` and recurrent forms such as
  `init_spec(Shape.color.color.rect, ptr)` use the same recursive tag
  initializer as local, global/static, and dynamically created PPC objects.
- `init_spec(Shape.color.rect[2], ptr)` casts `ptr` to the corresponding
  element pointer type, initializes the tag chain in every fixed-size array
  element, and still accepts `void *` for arbitrary caller-provided memory.
- `get_spec_size(struct Figure)` lowers to `__ppc_Figure_count + 1` for an
  ordinary generalization and to `__ppc_Figure_count` for a `= 0`
  generalization.
- `get_spec_ptr(struct Figure, i)` stores `i` in a local temporary, checks that
  `0 <= i < get_spec_size(struct Figure)` for an ordinary generalization, or
  `1 <= i <= get_spec_size(struct Figure)` for a `= 0` generalization, and
  returns `(struct Figure *)&__ppc_Figure_spec_ptr_tags[i]`. The returned
  pointer is a tag-only pseudo object and must not be used for field access.
- `spec_index_cmp(a, b)` stores both operands in temporaries, reads their
  hidden tag fields, and returns the common tag value if they match, otherwise
  `-1`. Both operands must point into the same PPC generalization family. For a
  `= 0` generalization, a zero tag in either operand is rejected at runtime
  instead of being returned as a valid specialization index.
- Table finalizers in `.fini_array.201` call `free()` for allocated dispatch
  tables and reset the common table pointer to zero. They are safe if several
  translation units emitted the same finalizer pattern.
- Tag-carrier finalizers in `.fini_array.151` free arrays allocated for
  `get_spec_ptr` and reset the common pointer to zero.
- Handler constructors in `.init_array.301` write specialization handler
  pointers into the table by reading the runtime tag cell. A `Base.void`
  handler contributes a literal zero coordinate. For a `= 0` generalization,
  `Base.void` handlers are rejected and the later zero-slot constructor restores
  the diagnostic handler for all zero-coordinate slots.
- PPC handler implementation symbols follow ordinary C linkage rules. A
  non-`static` handler definition such as `RunThread<ThreadData.Rect *r>()`
  emits an externally visible generated symbol, so constructors emitted from
  header declarations in other translation units can resolve it at link time.
- PPC specialization types receive stable internal names derived from their
  source-level path, for example `Figure_rect`, `Figure_trian`, and
  `Shape_color_rect`. These names are used for generated handler symbols;
  runtime dispatch table coordinates still use the constructor-assigned numeric
  tags.
- A named incomplete struct used in PPC syntax is treated as an external PPC
  generalization. The compiler emits the common runtime symbols needed for the
  family and reads the hidden first tag through the pointer ABI, without
  requiring the full `case` declaration in that translation unit. Full
  declarations are still required where specialization names, object layout,
  `create_spec`, or specialization fields are used.
- Global/static PPC variable constructors in `.init_array.401` initialize all
  runtime hidden tag fields reachable inside the variable after specialization
  tags have been assigned.
- PPC calls are lowered to a table lookup by reading the hidden first tag field;
  the previous `switch`-based prototype dispatch has been removed.

Still limited:

- multimethod dispatch is implemented as a dense flat table; sparse-table
  compression is not implemented yet
- chain-aware PPC function dispatch is not implemented yet; PPC calls still
  dispatch on the first hidden tag of each pointer argument
- `get_spec_ptr` and `spec_index_cmp` are still first-tag helper operations.
  They do not yet produce or compare an entire specialization chain as a
  semantic unit.
- mixed wildcard handlers such as `(specific, general)` are not expanded across
  a matching subspace yet; use `Base.void` for the concrete zero coordinate, or
  use the all-general handler as the default for the whole table
- runtime symbols are still generated directly by the compiler prototype; the
  declaration/definition model should be made more explicit before treating
  this as a stable ABI
- no linker extension is required in this snapshot
- `create_spec` currently owns allocation only; explicit deallocation helpers
  and ownership conventions are not defined yet
- the basic runtime helper set now covers `init_spec`, `create_spec`,
  `get_spec_size`, `get_spec_ptr`, and `spec_index_cmp`

Validation:

- `make test/ppc.exe`
- `./test/ppc.exe`
- assembly inspection for `.fini_array.201`
- manual two-translation-unit PPC dispatch check
- manual two-translation-unit multimethod dispatch check
- manual two-translation-unit `Base.void` multimethod dispatch check
- multi-translation-unit PPC handler linkage regression test in
  `test/ppc-multifile`, including two separately compiled specializations
  whose local numeric indices would otherwise collide
- user-supplied `thread-rect-perimeter-ppp-prj` check: separate default
  handler, specialization handler, and call site now link and run
- user-supplied `01-start` evolutionary demo check: separate rectangle and
  triangle `FigureIn`/`FigureOut` handlers now link, run, and produce the
  expected output
- user-supplied `01-start` forward-declaration variant check: `container.h`
  may use only `typedef struct Figure Figure;`; `container-out.c` can still
  declare and call `FigureOut<Figure *>` through stored pointers
- manual uncovered abstract multimethod diagnostic check
- `create_spec` coverage in `test/ppc.c` for a generalization, `Base.void`,
  ordinary specializations, typedef-based specializations, bodyless
  specializations, and recurrent specialization chains
- `init_spec` coverage in `test/ppc.c` for a generalization, `Base.void`,
  an ordinary specialization through a generalization pointer, and a bodyless
  specialization
- `init_spec` coverage for arbitrary memory passed as `void *`, including
  `Shape.color.rect` and recurrent `Shape.color.color.rect` chains
- `get_spec_size` and `get_spec_ptr` coverage in `test/ppc.c` for ordinary,
  typedef-based, enum-like, bodyless, and zero-disabled PPC generalizations,
  including tag-only dispatch for one-parameter functions and multimethods
- `spec_index_cmp` coverage in `test/ppc.c` for equal concrete
  specializations, different concrete specializations, tag-only pseudo
  pointers, the zero coordinate, bodyless specializations, and zero-disabled
  concrete indices
- specialization-chain variable coverage in `test/ppc.c` for
  `Shape.color.rect`, recurrent `Shape.color.color.rect`, nested field access,
  local tag initialization, and global/static constructor tag initialization
- specialization-chain array coverage in `test/ppc.c` for local,
  initialized, global, static, dynamically created, and arbitrary-memory arrays
  of `Shape.color.rect` and recurrent `Shape.color.color.rect`
- PPC aggregate-initializer coverage in `test/ppc.c` for compact positional,
  grouped specialization-body, and designated-field initialization styles
- zero-disabled specialization coverage in `test/ppc.c` and `test/driver.sh`
  for `const = 0`, rejected `Base` objects, rejected `Base.void`, rejected
  tag-0 `create_spec`/`init_spec`, default handlers for non-zero
  specializations, strict handlers, and runtime rejection of a technical zero
  dispatch coordinate
- special-function boundary coverage in `test/driver.sh` for ordinary
  `get_spec_ptr` lower/upper bounds, zero-disabled `get_spec_ptr` lower/upper
  bounds, and zero-disabled `spec_index_cmp` on a forged tag-0 pointer
- manual out-of-range `get_spec_ptr` diagnostic check
- `make test`
- `make test-all`
