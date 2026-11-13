# PPC extension snapshot 27

This snapshot starts a Procedural-Parametric C experiment on top of
`rui314/chibicc` commit `90d1f7f199cc55b13c7fdb5839d1409806633fdb`.

The experimental `-S -emit-llvm` backend emits LLVM IR for a limited C subset
on Linux x86-64, alongside the original assembly backend. It now supports
struct/union storage, member access and copying, plus the existing PPC runtime
lowering through `llvm.global_ctors` and `llvm.global_dtors` with unchanged
registration priorities and common symbols. Existing PPC semantics and ABI
remain unchanged. Aggregates with integer, pointer, float and double fields
pass and return by value using the Linux x86-64 System V ABI: INTEGER/SSE
eightbytes, whole-argument spill on register exhaustion, byval memory arguments
and sret result buffers. INTEGER wins when overlapping or adjacent fields
share an eightbyte; two floats in an SSE eightbyte use LLVM <2 x float>.
The six GP and eight SSE argument registers are counted independently, and
spilling an aggregate leaves both budgets available to later arguments.
Packed records with unaligned fields use memory; padding-only eightbytes do
not consume registers. Argument snapshots are allocated once in function entry.
The parser records explicit hidden result parameters, also for generated PPC
abort handlers returning large records. Local union initialization from a union
value or function result is supported; static union expression initializers
are diagnosed. Ordinary C and PPC ABI interoperability is covered by tests.
Scalar float/double arithmetic, comparisons, conversions, direct/indirect
calls and memory fields are supported, including PPC scalar arguments/results.
IEEE NaN conditions and signed zero are preserved without fast-math flags.
Floating-point constants use exact LLVM hexadecimal encodings. Scalar SSE
arguments do not consume the GP register budget used for record ABI.
External variadic calls accept integer/pointer and float/double arguments,
including float-to-double default promotion and named aggregate parameters.
Aggregates among unnamed variadic arguments remain diagnosed by LLVM.
Floating-point semantic and ABI tests use the system C compiler as an
independent reference, including edge cases with existing native-backend
limitations (NaN conditions and unsigned 64-bit conversions).
Tests cover case/enum dispatch, startup completeness,
runtime errors, separate compilation and mixed native/LLVM objects, with
LLVM 19, 21 and 23. The ppc-floating example exercises double payloads, ordinary
double arguments/results and printf output. The ppc-floating-by-value example
adds ordinary double-aggregate arguments/results and mixed integer/SSE results
of PPC handlers. Multifile tests exchange these records across native/LLVM
objects; record ABI tests cover nested arrays, unions, register rollback,
packed/aligned records and calls in both directions with the system compiler.
Interoperability was also checked with Clang 21 and 23 at LLVM O0 and O2.
See `docs/llvm-backend-ru.md` for examples and limitations.
LLVM now lowers ordinary switch/case/default to the LLVM switch instruction,
preserving fallthrough, nested loops, break/continue and goto entry into cases.
GNU case ranges use modular subtraction and unsigned bounds checks without
expanding the range. The shared parser applies integer promotions, retains
64-bit case values, converts labels to the condition type and rejects duplicate
or overlapping labels/defaults. Constant folding now preserves the width and
signedness of intermediate integers; unsigned arithmetic avoids host signed
overflow. Native 64-bit switch comparisons also load constants that do not fit
an immediate, and range spans are computed without signed overflow. These C
semantic corrections leave the PPC ABI unchanged. Shared native/LLVM tests
cover narrow/wide types, boundary values, ranges, Duff's device and diagnostics.
All 15 units of direct-classic/01-start now compile through LLVM, with CTest
fixture checks. See `docs/llvm-switch-ru.md`, including the reproducible audit
of the other standalone CMake examples and the remaining compatibility gaps.
The eight CMake projects blocked by empty ordinary argument lists now use
explicit (void) in the relevant PPC declarations, definitions and handlers,
including ppList_sort, FigurePerimeter, DrawCell/isLife, FigureDestroy and
RunThread. Calls still use empty parentheses after the PPC arguments. The
example migration changes no PPC dispatch rules or ABI. The LLVM suites
now reuse all eight evolutionary figure fixtures at O0/O2, including stage2.
A repeated audit builds 23 of 38 standalone CMake projects through LLVM 23;
15 figure outputs match their fixtures. The remaining 15 failures concern
CMake language/header settings. See docs/llvm-void-signatures-ru.md.
Empty () signatures now work directly in ordinary C and PPC LLVM definitions,
declarations and indirect calls. Type.is_unprototyped separates the C11/C17
unspecified-parameter type from true ellipsis varargs. Empty C definitions
have zero parameters; unprototyped scalar calls retain default promotions
and the System V call ABI. Later prototypes refine earlier declarations
without replacing a known prototype with an unspecified list. Compatibility
checks account for default promotions. Empty PPC ordinary lists are fixed
zero-argument lists, interchangeable with (void), while procedural arguments
remain explicit. LLVM emits fixed definitions and variadic-spelled C
declarations/calls where parameter types are unspecified, and for actual
ellipsis signatures. Aggregate arguments without a prototype remain diagnosed.
Native/stage2 and LLVM 19/21/23 tests cover function pointers, type/refinement
rules, scalar host ABI, small/large aggregate results, multimethods, strict
enum dispatch, and mixed native/LLVM modules. PPC ABI is unchanged.
The compiler now recognizes .ppc implementation files automatically alongside
.c in both backends. It classifies .h/.pph as headers for includes and direct
-E/-M processing; separate compilation requires an explicit -x c override.
There is no PCH generation. Include lookup itself remains suffix-independent.
Unknown implementation suffixes still require -x c; -x none restores detection.
Native/stage2 and LLVM driver tests cover mixed .c/.ppc units, .pph/.h includes,
dependencies, raw IR, header diagnostics and explicit overrides. CMake still
needs LANGUAGE C for .ppc and HEADER_FILE_ONLY for headers; examples' CMake
files are left for their separate maintenance step.
GitHub Actions now has separate native and LLVM 23 jobs on Ubuntu 24.04.
The LLVM job runs stage1/stage2 O0/O2/ABI/driver checks plus optimization and
dispatch demos. Python 3.12/CMake 3.31.6 and checksum-pinned LLVM 23.1.6
packages provide a reproducible setup without system APT updates. Official
LLVM packages are extracted without root and the compressed downloads are
cached; tools verify the expected version. Failure artifacts preserve LLVM
diagnostics. See docs/llvm-ci-ru.md. Stage2 itself is still built natively.
Variadic C and PPC definitions now support scalar va_start/va_arg/va_copy/va_end
and forwarding va_list to libc or other GCC/Clang modules. LLVM initializes a
private 24-byte va_list through llvm.va_start and ends it on each normal return
or fallthrough. Typed ND_VA_ARG accesses retain diagnostics for aggregate retrieval while
lowering scalar accesses through stdarg helpers.
The native backend now uses the System V register-save layout: six 8-byte GP
slots and eight 16-byte SSE slots. It reserves that area separately, accounts
for named stack arguments and named register classes, and clamps the offsets.
stdarg.h shares this ABI in both backends; long double uses stack memory.
Existing native objects creating/reading/passing va_list need a clean rebuild
because the earlier private SSE layout used 8-byte slots. PPC tags, tables and
dispatch ABI are unchanged. Native/stage2 and LLVM 19/21/23 tests cover register
exhaustion, interleaving, pointers, promotions, independent cursors/restarts,
libc forwarding, named records and sret, strict/PPC multimethods, and mixed
native/LLVM units. See docs/llvm-varargs-ru.md.
LLVM now supports long double as x86_fp80 (64-bit significand, 80 value bits,
16-byte storage/alignment), including exact 0xK constants, scalar arithmetic,
conversions, IEEE conditions, direct/indirect/unprototyped calls and stdarg.
Scalar long-double arguments use the stack without consuming GP/SSE budgets;
results use st(0). Record ABI classification merges X87/X87UP with INTEGER/SSE:
X87 records use byval arguments and x86_fp80 results, other records use the
appropriate integer pieces or memory/sret. GCC/Clang helpers check both call
directions, named stack arguments, variadic cursors/copies/forwarding and
small/large/packed aggregates. PPC tests cover long-double payloads, scalar
and aggregate parameters/results, variadic dispatch and mixed native/LLVM
scalar modules. Existing native small-record X87 ABI remains a separate limit;
exchange these records through pointers or compile their functions with LLVM.
The shared constant evaluator now preserves long-double precision, rounds
intermediate floating types/casts, evaluates floating comparisons/truths,
and converts unsigned floating results without passing through int64_t.
Global long-double initializers serialize ten value bytes and zero padding.
Native long-double-to-short/unsigned-short/unsigned-int/unsigned-long casts
are corrected, including the upper uint64_t half used by self-hosted stage2.
Public PPC tags, registration and dispatch rules are unchanged. See
`docs/llvm-long-double-ru.md` for the ABI and regression commands.
Closed case/enum/nullptr/union families now have declaration-order constant
indices: zero/default is 0, nonzero alternatives are 1..N, and get_spec_size
includes zero. get_subtype_index and get_spec_size are integer constant
expressions for visible closed definitions (case labels, C enums, array bounds
and static initializers). Member.ppc_tag_base retains the owning family through
specialization/nested-type copies, so tag reads, enum feature assignments,
local/global initialization, create_spec/init_spec and handler registration
agree. Explicit global initializers serialize fixed tags directly; tentative
and implicit-static objects retain startup initialization to preserve common
symbol merging. Shared tag/count metadata remains available to opaque modules;
closed registration writes fixed values instead of startup-order increments.
All objects/libraries exchanging closed PPC types must be clean rebuilt.
Open-family nonzero tags/extents remain runtime values. Dispatch tables and
handler registration remain dynamic at this stage, including mixed open/closed
multimethods. The guarded local stage below builds on these constants; future multimethod
optimization is restricted to functions whose procedural families are all
closed and whose handler set is known; no automatic macros or source-closing utility are introduced. Native,
stage2, LLVM and independent host startup tests cover all four forms, empty and
named zero cases, static data, nesting, mutable enum tags, side effects, strict
completeness, opaque declarations and mixed native/LLVM modules. See
`docs/ppc-closed-indices-ru.md`.
Closed dispatch now has a guarded local optimization, including multimethods
when every procedural parameter family is closed. After parsing the unit,
complete local coverage produces a private immutable pointer-array catalog.
A real all-general default fills holes and concrete tuples override it,
independent of source order. Generated abort stubs do not count as defaults.
The limit is 16 cells in the product of all extents, including zero on each
axis; the product is checked before multiplication. The flattened selector
uses the same row-major coordinates as the shared runtime table.
Bodies can be in other units, and declarations after a call are considered.
Each optimized call snapshots the object tags and active runtime handler once,
then checks that handler against the catalog. A match uses the catalog/table
call or an explicit switch of direct calls; a mismatch retains the active
indirect call. Runtime tables/constructors and ABI remain available for other
units, including private handlers registered from native/LLVM modules. Closed
types do not imply a closed handler registry. Partial sets without a real
default, any open/opaque parameter and over-limit grids retain runtime dispatch.
--ppc-dispatch=auto|runtime|table|switch selects the representation; auto uses
switch for LLVM and runtime natively. Existing LLVM opt can inline known switch
branches while retaining the guard/fallback. Generic typed AST lowering covers
void/scalars, records/sret and varargs. Arguments are snapshotted before
branching so nested dispatch labels are emitted once; array/VLA/function arguments
decay to pointers and records are copied without aggregate casts. Catalogs
use LLVM internal constant pointer arrays and native .data.rel.ro; function
roots referenced by them are retained. Native/stage2 and LLVM tests cover all
modes, late definitions, all four families, tag mutation, argument side effects,
record/variadic ABI and separately registered overriding handlers in both link
orders and mixed backends. New multimethod fixtures cover every cell in 2x3,
2x3x2 and 2x2 grids, early/late defaults, records, sret plus variadic stack
arguments, all six orders of three objects, and a third opaque unit.
Removing the external real default must abort for closed incompleteness before
main, regardless of object order; opaque error stubs cannot mask that check.
A VLA fixture verifies variadic pointer decay in native/LLVM and closed mode.

The startup registration protocol has changed for both native and LLVM PPC
objects: priority 201 only allocates a zeroed table, priority 301 concrete
registrations overwrite their cells and actual defaults fill only null cells,
priority 351 checks closed families without a local actual default, and
priority 361 fills remaining open/opaque holes with abort stubs. Thus real
defaults work before or after concrete handlers and across translation units.
All objects/libraries containing PPC functions/registrations must be cleanly
rebuilt together. Old constructors prefill tables and must not be mixed with
this protocol; ordinary C libraries require no rebuild. Closed tag numbers,
object layouts and function calling conventions are unchanged. See
`docs/ppc-closed-dispatch-ru.md`.
LLVM modules now carry descriptive !ppchibicc.dispatch named metadata with
the ppchibicc.dispatch.v2 schema (the report reader also accepts v1).
A frontend snapshot exposes each family;
fixed axes, alternative names/order, type size/alignment, lowered ABI and
handler coordinates are emitted only for all-closed procedural arguments.
Open/opaque participants contribute exclusion records. Unit markers include
ordinary C units. Handler/table operands are real LLVM pointer references,
preserving internal symbol renaming across llvm-as/opt/llvm-link. Definition
and original linkage flags distinguish declarations and private registrations.
This adds no runtime ABI or registration changes.

cmake/ppc-dispatch-report.py collects raw or linked textual IR into JSON and
Markdown matrices. Concrete coordinates override generic defaults; repeated
external declarations resolve to definitions, while different private
registrations stay distinct and order-dependent. Missing definitions/IR
bodies, removed pointers, opaque participants, inconsistent axes/ABI and
duplicate definitions prevent a resolved conclusion. Matrix expansion is
bounded separately from local switch optimization (4096 report cells by
default). The report explicitly does not establish whole-program completeness
and does not apply optimization. Supplied modules may still omit other native
objects, archives or registrations. Runtime allocation, constructors and guards
remain necessary until a later whole-program validation stage.

llvm-dispatch-report and test-llvm-dispatch-report-stage2 require Python 3.8+
and matching LLVM tools including llvm-link. The three-unit closed-registry
example verifies all six object orders; regression cases cover all four type
forms, external bodies, private overrides/defaults, incomplete/inconsistent
metadata, bitcode and O2, quoted UTF-8 paths and input-error preservation.
Both targets are included in LLVM CI. See docs/ppc-dispatch-report-ru.md.

The separate ppc-static-dispatch.py executable mode now accepts an explicitly
complete set of raw ppchibicc v2 LLVM units and --closed-program. Resolved
all-closed families can be frozen into private immutable pointer arrays and
internal constant table cells. The frontend describes allocate/release,
concrete/default registration and completeness-check functions as real LLVM
references. The tool checks counts, definitions, ownership and actual startup
priorities, audits table loads and derived addresses for writes/escapes, and
rejects unknown application dependencies (only the supported C/POSIX runtime
and LLVM intrinsics may remain external). Open/opaque participants, missing
cells/definitions, conflicting registrations and inconsistent axes/ABI exclude
a family; explicitly requesting it fails. Native application objects, merged
or preoptimized inputs and old metadata cannot establish this contract.

After validation, the selected table lifecycle entries/bodies are removed,
actual handler definitions are internalized, and the output is assembled and
verified before atomic publication. Input SHA256s, selected matrices, removed
functions and output SHA256 are recorded; an explicit-input-set manifest hash
is also embedded in !ppchibicc.closed_program. This is an explicit complete-
input contract, not automatic discovery of every possible library. Adding a
handler module requires rebuilding the closed program. Unselected/open tables
and required tag/carrier/global initialization retain their runtime behavior.

Compiler-owned table-slot additions now carry a Node marker and use byte GEP
without inbounds in LLVM. This exposes constant entries without changing
ordinary C pointer/integer lowering or native code generation. The 2x3 example
removes 11 lifecycle functions and LLVM O2 reduces registry_known to ret i32
507, while runtime dispatch retains an indirect call. Current LLVM 23 dispatch
demo raw/O2 counts are 101/45 (known), 139/64 (closed), 201/87 (multi), and
99/43 (open); these are static metrics, not speed measurements. ABI/tag values
are unchanged. Static-dispatch stage1/stage2 CI tests cover all three modes,
six module orders, records/sret/varargs, all four closed forms, a private handler
that overrides a locally known default, adjacent open/opaque runtime families,
metadata/priority completeness, unknown dependencies and table address escapes.
See docs/ppc-static-dispatch-ru.md.
The optional llvm-static-dispatch-bench compares runtime, guarded switch,
static-table and static-switch with all-closed 2x3/2x3x2 families, short and
six-step unsigned-mixing handlers, and shuffled uniform/hot90 records.
Checksums and default/concrete counters are independently checked in scalar
C and Python: 256 cases include empty/single/small/full batches and boundary
seeds, plus 52 invalid argument cases. Every calibration/timing sample is also
checked. Existing llvm_bench.py supplies affinity, equal work per workload,
shuffled repeats, calibration and median/MAD/CPU statistics.

The timer's raw LLVM unit participates in the complete input set at closure.
Code generation then partitions that same registry-free unit into one shared
timer object and lowers the remaining core into four objects, without linker
LTO. Four noinline batch kernels preserve a common boundary while handlers
remain available for inlining. Catalogs, timer clock/call boundaries, closure
lifecycle removal and retained batch calls are checked. This respects the
complete-input contract without adding an unknown native application module.
Commands/versions, source SHA256s, input histograms/snapshots, IR metrics,
assembler and per-kernel/whole-core code sizes are retained. Compiler
internals, ABI and tag semantics are unchanged. Default timing runs 224 samples;
stage1/stage2 CI uses check-only without performance gates. See
docs/llvm-static-dispatch-benchmark-ru.md.
Real application checks now compare three unchanged source subsets from
kreofil/4P-examples at 9e8f1f1c07ef08e845f877c8dac7a804ef9c0af4:
specialized ordinary parameters/results (spec-param), the open two-argument
visitor-mm, and the three-unit integer ppVector example. Source paths and
SHA256s are pinned; no network is needed. A separate adapter covers and invokes
Figure.void before calling the unchanged spec-param main. The original is
also tested: its missing tag-0 Print handler must abort before main, or fail at
build time when explicitly required with --closed-family=Print. This preserves
the existing completeness rules rather than silently accepting an empty slot.

Each compiler builds and runs 30 programs: native, per-unit LLVM O0/O2 and
whole-program O0/O2 for five successful cases plus the historical negative
case. The current repository spec-param (open Figure) and an adapter calling
its own Figure.void handler are successful runtime cases; the pinned closed
4P-examples version and its independent adapter remain separate checks. Expected output is specified independently, with a scalar Python vector
reference. Raw matrices, frozen IR and public-compiler manifests are checked;
Print freezes into three addresses and loses six lifecycle functions, while
both open examples retain their runtime tables. Reports keep input/executable
SHA256s, tools, commands, stdout/stderr and complete ELF section sizes. An
optional --examples-dir checks a separate upstream checkout with actual hashes
and its Git HEAD. Stage1/stage2 targets llvm-real-programs and
test-llvm-real-programs-stage2 are included in LLVM CI.

The vector revealed that the strict closed-program runtime allowlist omitted
libc reallocarray; it is now included. A C execution regression and the closed
compiler suite check allocation/growth and multiplication-overflow failure,
including preserved contents and ENOMEM. Unknown application dependencies
still require their IR. No compiler ABI, type/tag semantics or source-project
CMake changes are introduced. LLVM 19/21/23 focused checks pass. These short
printing demos are not timed; executable .text can grow under whole-program
O2, as it does for the open vector. See docs/llvm-real-programs-ru.md.

The optional llvm-life-bench measures the original toroidal Life simulation
(map.c/eworld.c unchanged) in native, per-unit LLVM O2 and combined LLVM O2.
A finite harness replaces animation/file loading/sleep, seeds initial maps,
cycles eight single-cell perturbations, restores both maps, runs 32 generations
and hashes the final board/population. The four original Cell families remain
open/excluded, with no lifecycle removal; no source closure is performed.
The timer is compiled by the selected pipeline, not shared machine code.
The LLVM life_run round boundary is marked noinline and verified after O2.

A Python model independently counts eight neighbours (the PPC code counts
nine including self), checks all eight phases, input/final fingerprints,
population/counter invariants and aggregate warmup/sample results. Six map
sizes including 1x1/3x5, 0/1/7/32 generations, boundary seeds and 1/9 rounds
produce 144 execution checks plus 45 bad-argument checks per compiler. The
shared benchmark protocol supplies CPU affinity, equal calibrated work per
workload, shuffled repeats, median/MAD and CPU statistics: default 84 samples.
Both compilers run check-only in CI, with no performance gates. Snapshots,
tools/commands/SHA256, raw/O2 IR, matrices, manifests and ELF sizes are retained.

This workload exposed pathological native code growth from byte-unrolled
large record assignments. Native stores above 64 bytes now use rep movsb,
preserving rax and rdi/rsi/rcx; the Life executable .text fell from 1702904 to
24926 bytes. Memory-class returns also copy large results compactly and now
return the hidden result-buffer address in rax as required by SysV, fixing a
latent bug that returned a pointer into a dead source/argument frame.
Native objects containing these memory-return definitions should be rebuilt;
their old generated code still violates the required return-pointer rule. A new
large-copy C regression checks unaligned guarded records, odd tails, unions,
self/chained/value assignments, stack arguments and aggregate returns, in
native stage1/stage2, LLVM and system GCC. Type/tag layout and signatures are
unchanged. See docs/llvm-life-benchmark-ru.md.

PPC matrix size and mixed-radix handler indices now use unsigned 64-bit
arithmetic before multiplication. The old int products could wrap 4^16 to
zero, allocate an empty table and silently skip filling/completeness checks.
The default/stub fill loop counter is widened too; tiny local catalogs keep
an int switch index because their existing bound is 16 cells.

All-closed table products are validated at compilation against SIZE_MAX/8.
If any axis is open, constructor 201 checks each extent/product before
multiplication, then checks calloc before registration or table iteration.
A zero extent or unrepresentable byte size aborts with a named size-overflow
diagnostic; allocation failure reports the full unsigned slot count. No new
arbitrary runtime size cap is imposed. Tag values, public introspection types,
object layout, symbol names and priorities are unchanged. Rebuild PPC objects
and raw IR to replace the old arithmetic; update the sealer with the compiler.

The sealer admits allocation-error dprintf/abort calls and diagnostic strings
in the owned allocate role, retaining the other call/global-reference audits.
Tests insert valid assembled IR with an unrelated global store or puts call
and verify rejection by the runtime-side-effect audit. Metadata stays v2.

The new ppc-table-size source checks a 2x3 matrix and one-time argument
capture. Its fault modes cover open/closed 4^16 allocation under RLIMIT_AS,
a small deterministic calloc failure, 4^31 slot-to-byte overflow and 4^32
slot-product overflow. Native stage1/stage2 and LLVM O0/O2 run the cases;
no huge physical allocation is attempted. Focused LLVM 19/21/23 and full
stage1/stage2/static/application checks pass. See docs/ppc-table-dimensions-ru.md.

LLVM now supports local variable-length arrays and direct alloca calls through
dynamic byte allocas, keeping a separate pointer slot for each VLA. Array
decay, addresses of whole arrays and multi-dimensional row operations use the
allocated payload. PPC runtime tag-initialization loops work for open/closed
case/enum/nullptr/union elements and nested fields, with unchanged dispatch
eligibility, tags and ABI. Direct alloca memory still requires explicit init_spec.

Shared frontend corrections capture VLA typedef bounds at declaration and
reuse them, widen size multiplications before calculation, divide pointer
differences by the saved runtime row size, and retain the element alignment
for _Alignof. VLA typedef parameters decay to pointers; array assignment is
rejected except for the compiler-owned pointer initialization node.
Generated allocations carry the element/declaration alignment. The built-in
alloca parameter is size_t-width; native allocation keeps 64-bit arithmetic,
aligns the result address (also above 16 bytes), and moves pending expression
temporaries by the same displacement. PPC layouts and calling conventions
are unchanged. Rebuild affected VLA/alloca functions to use these corrections.

Dynamic storage currently lasts until function return in both backends; no
block-exit VLA stack restoration is emitted. Headers referring to earlier
parameter names, such as f(int n, int a[n]), remain a shared parser limit.
New checks cover saved typedef sizes/side effects, 4-GiB sizeof without
allocation, 1D/2D/3D addressing, alignment, recursion and nested/loop alloca.
Existing VLA sources and an expanded all-form PPC fixture run in LLVM O0/O2,
native and stage2. All dispatch modes and closed builds cover VLA pointer
arguments to a variadic multimethod. See docs/llvm-vla-ru.md.

The public compiler now integrates this closure pipeline through
--closed-program (implicitly LLVM). A final executable command accepts all
source units, raw ppchibicc .ll units and marked closed-program .o files.
--closed-program -c saves raw textual IR with a versioned comment header,
without Python or external LLVM tools; ordinary compilation still produces
ELF objects. Final closure checks matching LLVM as/opt/link/llc versions,
uses the existing strict input/table audits, optimizes the combined IR at the
requested level, lowers it and links with the existing system ELF toolchain.
--closed-family is repeatable and requires a uniquely resolved closed matrix;
--closed-report selects the JSON manifest (default OUTPUT.closed.json).
Unselected/open families retain runtime behavior and their statuses appear
in the report. A complete pure-C/all-open program is allowed with no frozen
tables; standalone freezing still requires a selected table by default.

Native application objects, archives/shared libraries, bitcode/assembly and
arbitrary linker options cannot establish this complete-input contract and
are rejected in closed mode; the supported C runtime and -lm are permitted.
Ordinary mixed compilation is unchanged. There is no PPC ABI/tag change;
clean rebuilds or separate directories are needed when changing the object
mode. Dependency prerequisites now use the existing Makefile quoting helper,
including paths with spaces. Final output/report are prepared in sibling
temporary files before publication; tool or linker failure preserves existing
results. -S -emit-llvm emits combined verified pre-optimization IR; normal
single-unit raw IR remains tool-free.

PpchibiccClosedProgram.cmake enables this mode on standard CMake executable
and OBJECT library targets, with final-link optimization/family/report options.
Python helpers and the module install under share/ppchibicc and are discovered
relative to the compiler; moved prefixes and explicit helper overrides work.
The closed-build example covers object libraries, incremental builds and CTest.
Stage1/stage2 CI adds 13 regression tests for these paths, all dispatch modes,
O0/O2/O3/Os/Oz, source/IR/object combinations, .ppc/.pph, dependency output,
empty closure, native/external-module rejection and output preservation.
Focused checks pass with LLVM 19, 21 and 23. See docs/llvm-closed-compiler-ru.md.

The public driver now accepts --backend=llvm to compile C, .ll or .bc through
external opt/llc, produce objects directly and use the existing ELF linker.
-S selects machine assembly; -S -emit-llvm retains tool-free raw IR output.
O0/O1/O2/O3 use standard pipelines; Os/Oz use forceattrs with optsize/minsize
and O2, including LLVM 23 where default<Os>/default<Oz> were removed. Tools
are selected through PATH or PPCHIBICC_LLVM_OPT/PPCHIBICC_LLVM_LLC environment
variables. Compilation remains per translation unit, with PIC machine output.
source_filename preserves the escaped C source name across temporary IR files.
The native backend remains the default; preprocessing and object-only linking
need no LLVM tools. Assembly inputs are included in the link, and -c/-S stop
before linking existing object inputs. Tests cover the driver at stage1/stage2,
ABI fixtures, mixed PPC objects, archives, shared/static linking, size modes,
IR/stdin/stdout, dependency files, path escaping and failed-tool cleanup. A
standalone CMake application example is in examples/llvm/build-demo. See
`docs/llvm-compiler-ru.md` for commands and the optimization/tool selection rules.
The optional llvm-opt-demo target compares verify-only IR, mem2reg,
sroa/mem2reg and default<O2> from one bitcode input with fixed llc settings.
It validates execution against known results and native output, saves IR,
assembly, pass pipelines and tool/source checksums, and reports static counts
for local aggregate code and PPC runtime dispatch. See
`docs/llvm-optimization-ru.md` for the LLVM 19/21/23 observations.
The current toolchain line is Clang/LLVM 23.1.x. Full native, self-hosted
stage2, LLVM O0/O2 and optimization-demo checks pass with Clang/LLVM 23.1.6;
LLVM 23 object interoperability also passes against GCC 14.2.0. CMake searches
versioned tools through -23 and prints the selected paths. Existing cache
entries remain explicit selections; separate build directories keep toolchains
apart. The demo emits objects directly with llc, so .prefalign in LLVM 23's
assembly listing does not require a newer system GNU assembler.
The llvm-dispatch-demo target compares known-specialization, closed-family
and open multifile calls, with a plain C function-pointer control. It tests
separate objects, per-unit O2, linked O2 and explicit internalization while
preserving inspectable public probe functions. An extension replaces one
object without rebuilding callers. On LLVM 23.1.6 the PPC call sites remain
indirect in all these modes; O2 eliminates the control's indirect call.
See `docs/llvm-dispatch-ru.md` for assumptions and reproducible results.
The optional llvm-pgo-demo target uses real IR instrumentation, compiler-rt
and llvm-profdata with LLVM/Clang 23. Training observes 4096 rectangle calls;
the standard PGO O2 pipeline promotes that target with an address guard and
inlines it while keeping the general indirect path. The optimized core is
also linked with unseen handlers that modify data and retag an object with
init_spec. Native, O2 and PGO outputs agree. main and extension objects stay
outside the profiled module; internalization is not required. See
`docs/llvm-pgo-ru.md` for the LLVM 23.1.6 results and runtime requirements.
The optional llvm-pgo-bench target reuses these production objects in a
checked microbenchmark: rectangle, square, mixed and an unseen mutating
extension. A shared ordinary-C timer excludes startup and warmup; Python 3.8+
calibrates equal work, pins the process CPU by default, randomizes repeated
runs, and reports medians, MAD and raw wall/CPU times without speed thresholds.
See `docs/llvm-pgo-benchmark-ru.md` for reproduction and interpretation.
The optional llvm-figures-bench target exercises the existing ppVector and
ppvector-figures perimeter code on shuffled collections, restoring and sorting
the vector on every round. Two independent profiles (90% rectangles and 50%)
are evaluated on new seeds and larger collections. A shared C timer and Python
measurement protocol check sortedness and exact position-weighted checksums;
the original ten-figure program is also compared between native and LLVM O2.
No-argument ppVector_sort and selected FigurePerimeter signatures now use
explicit (void), including the vector examples sharing that interface, so
they fit the supported LLVM function subset. See `docs/llvm-figures-benchmark-ru.md`.
While adding it, the shared type pass was corrected to apply C integer
promotions to `~`, `<<` and `>>`: narrow integer operands promote to `int`,
and the result type follows the promoted left operand. Regression coverage
is included in `test/usualconv.c` and the LLVM differential tests.
The frontend also retains `volatile` qualifiers so the LLVM MVP can diagnose
unsupported volatile accesses rather than silently optimize them. This does
not add volatile lowering to the native backend.

Implemented so far:

- C23 `nullptr` and the distinct scalar type `nullptr_t`, declared by
  `<stddef.h>` as `typeof(nullptr)`; this feature is available in the default
  language mode without claiming support for all of C23
- `nullptr_t` has the size, alignment, representation and calling convention
  of a null `void *` on the x86-64 target; it can be stored, passed, returned,
  selected with `_Generic`, and used in variadic calls
- conversion from `nullptr_t` to object/function pointers, `_Bool` (false),
  and `void`; conversion to `nullptr_t` from itself or a null pointer constant
- equality with pointers, other `nullptr_t` values and integer null pointer
  constants; conditional expressions with two `nullptr_t` operands or a
  `nullptr_t` operand and a pointer; arithmetic, ordering and conversion to
  numeric types other than `_Bool` are diagnosed
- ordinary C struct and union declarations reject declarator-less typedef or
  tagged record types and duplicate visible member names, including names
  promoted recursively from anonymous structs and unions
- `struct Name { ... } case { ... };`
- `struct Name { ... } enum { a, b, c };` as ADT-like sugar for bodyless
  specializations
- enum feature initialization and assignment into a zero-typed receiver:
  `WeekDay wd = WeekDay.Monday;`,
  `WeekDay days[3] = {WeekDay.Sunday, WeekDay.Thursday, WeekDay.Friday};`,
  `wd = WeekDay.Tuesday;`, and `days[2] = WeekDay.Monday;`
- `struct Name { ... } nullptr { Type *a, *b; OtherType *c; };` for
  pointer-only specialization lists, with optional `const` and pointer-only
  `Name + { Type *d; };` extensions
- pointer-list zero specializations (`Base` and `Base.void`) reserve a
  `void *` payload, including for an initially empty `nullptr {}` list;
  the zero payload is accessible as `.void`
- fixed-storage value generalizations with
  `struct Name { ... } union sizeof(...) { ... };`; alternatives share a
  common size and alignment but retain distinct specialization tags and types
- union storage may be specified as `sizeof(Type)`,
  `sizeof(integer constant expression)`, or
  `sizeof(integer constant expression, alignment expression)`; the
  one-expression form uses the target maximum alignment and rounds the
  requested size upward
- a closed `union { ... } const` generalization may omit `sizeof`; its payload
  size and alignment are inferred from the largest initial alternative
- extensible union generalizations require an explicit fixed capacity;
  later `Name + { ... };` alternatives are accepted only when their size and
  alignment fit that capacity
- closed generalizations with `struct Name { ... } case { ... } const;`
- named default zero specializations with
  `struct Name { ... } case { default Type name; ... };`
- empty case lists, for example `struct Figure {} case {};`
- decentralized extension with `struct Name + { ... };`
- decentralized ADT-style extension with `struct Name + { a, b, c };` when the
  generalization is already enum-like, or when an existing all-void `case`
  generalization is first extended by a feature list
- decentralized extension through a typedef name, for example `Figure2 + { ... };`
- specialization types such as `struct Figure.triangle` and `Figure2.rect`
- specialization chains for variables, for example
  `Shape.color.rect` and recurrent chains such as `Shape.color.color.rect`
- specializations without an instance body, written as `void Name;` inside the
  `case` or extension list
- specializations with anonymous struct or union bodies inside the `case` or
  extension list, for example
  `case { struct { int x, y; } rectangle, rhombus; }` and
  `Name + { union { int i; double d; } scalar; };`
- a `case` list may start with `default Type name;`; that named
  specialization occupies tag/index 0 and replaces the synthetic `Base.void`
  spelling for that generalization
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
- Enum and pointer-list generalizations allow assignment from any specialization in the
  same family to the zero specialization (`Base` or `Base.void`), because all
  generated alternatives have the same object layout

Current implementation strategy:

- A PPC generalization stores its specialization list in the `Type` object.
- A specialization such as `struct Figure.triangle` is represented as a normal
  struct type generated from the hidden tag field, the common members of
  `Figure`, and the selected specialization member.
- PPC specialization lists use the ordinary C member-declaration parser for
  specialization bodies. This allows inline anonymous `struct { ... }` and
  `union { ... }` bodies; the specialization name is still the member name, for
  example `rectangle` in `struct { int x, y; } rectangle;`.
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
- An `enum { ... }` PPC generalization stores the listed feature names as
  `void` specializations and marks the generalization as enum-like. A feature
  list extension (`Name + { feature };`) does the same for an already enum-like
  generalization. If it is applied to a `case` generalization, all existing
  specializations must be bodyless; otherwise the extension is rejected.
- A `nullptr { ... }` list requires an explicit pointer declarator for every
  specialization, for example `Rectangle *rect, *box; Triangle *trian;`.
  Non-pointer alternatives, array/function alternatives, bit-fields, and
  pointer members with a different alignment are rejected. The list can be
  empty and is extended only with pointer declarations.
- Pointer-list layout is the hidden tag, common fields, and a single pointer
  payload. The base/zero type stores `void *` in that payload; a concrete
  specialization stores its declared pointer type at the same offset. Thus
  every alternative, including zero, has the same size and alignment, even
  when the common fields are packed or over-aligned.
- Pointer-list specialization initialization uses the ordinary aggregate
  initializer rules. With no common fields, `Figure.rect fr = &r;` and
  `Figure f = nullptr;` initialize the single visible pointer payload. With
  common fields, use positional or designated initializers such as
  `{7, &r}` or `{.color=7, .rect=&r}`. Omitted fields in an initializer are
  zero-initialized, so omitted elements of an initialized `Figure` array
  select tag 0 with a null `void *` payload.
- A `union sizeof(...)` generalization lays out the hidden tag, common fields,
  and one hidden fixed-capacity byte payload in `Base` and `Base.void`. A
  concrete specialization replaces that hidden payload with its named object
  plus hidden trailing padding. Consequently every direct alternative has the
  same size, alignment, and array stride without using C union aliasing.
- Union alternatives may be ordinary complete object types or `void`.
  Functions, VLAs, flexible types, bit-fields, oversized alternatives, and
  alternatives whose alignment exceeds the fixed storage alignment are
  diagnosed. `default` is intentionally unavailable because `Base.void`
  remains the zero specialization and the polymorphic value carrier.
- Arrays may use either the generalization type, allowing values of different
  alternatives in one array, or a concrete specialization type, restricting
  every element to that specialization.
- `Base.void` is a separate synthetic specialization type for PPC handler
  parameters when a `case` generalization has no named default specialization.
  It is not taken from the `case` list and denotes tag 0, i.e. the
  generalization object itself as a concrete dispatch-table coordinate.
- If a `case` generalization starts with `default Type name;`, the named
  specialization is tag 0. The generalization object uses the default
  specialization layout, so direct objects can be initialized with the default
  payload, and `Base.name` is used for concrete tag-0 handlers.
- Ordinary `case` aggregate assignment remains strict.
  For a named default specialization, `Base` and `Base.default_name` are treated
  as the same concrete zero specialization for aggregate assignment.
  Enum, pointer-list, and fixed-storage union generalizations add the
  polymorphic value assignment
  `Base = Base.some` and `Base.void = Base.some`; concrete-to-concrete
  cross-specialization assignment and assignment from zero to a concrete
  specialization remain rejected.
- Polymorphic value assignment copies the tag, common fields, and complete
  fixed payload together; copying a zero-typed variable preserves its current
  dynamic specialization. Pointer-list copies remain shallow and retain the
  same referenced object. The size and alignment must match, so a
  specialization chain that enlarges a common member cannot be copied into a
  smaller base.
- An enum feature assigned directly to `Base` or `Base.void` changes only
  the runtime specialization tag and preserves all common data. The lvalue
  is evaluated once, including in `days[i++] = WeekDay.Monday`. The assignment
  expression has the receiver's aggregate type and value, so chaining and
  ordinary copying of its result work as for other aggregate assignments.
  Concrete specialization receivers are rejected, even if the feature name
  matches their declared specialization.
- Enum feature values require `Generalization.feature` in direct assignment
  and initialization contexts. The qualifier can be a typedef (including an
  alias), an unshadowed struct tag, or an explicit `struct Tag` spelling.
  The qualifier must denote the receiver's enum family; even an identically
  named feature of another family is rejected. Bare feature names are not
  feature values and do not enter the ordinary C namespace. Ordinary variables
  and member access retain their meaning. Parenthesized features and
  single-feature braced initializers are supported. In type contexts,
  `Generalization.feature` continues to denote the specialization type.
  Feature assignment does not apply to ordinary `case` or pointer lists.
- Initializing an enum object from a feature initializes its common fields to
  zero. Feature initializers work for automatic, global and local `static`
  objects, arrays, nested aggregate members and automatic compound literals. Explicit closed-family global/static initializers contain fixed tag bytes;
  open-family feature tags are initialized at startup from registered tag
  cells, including specializations declared in other translation units.
- A `const` generalization has a closed specialization set. Later `+`
  extensions of such a generalization are rejected.
- The hidden tag field is the first struct member. The generalization uses
  tag 0; concrete non-default specializations have declaration-order tags 1..N
  for closed definitions and startup-assigned tags for open families. A named default specialization also uses tag 0 and does not need a
  runtime tag cell.
- Each generalization has a common runtime counter:
  `__ppc_<type>_count`.
- Generalizations used with `get_spec_ptr` also have a common runtime
  tag-carrier array pointer:
  `__ppc_<type>_spec_ptr_tags`.
- Each specialization has a common runtime tag cell:
  `__ppc_<type>_<specialization>_tag`.
- Specialization constructors in `.init_array.101` increment the counter for
  open families, or store fixed indices/counts for closed families. They are idempotent, so a PPC header included by several
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
- Table constructors in `.init_array.201` allocate a zeroed table once. All
  slots remain null until registration. Real all-general defaults, such as
  `FigurePair<struct Figure *a, struct Figure *b>() { ... }`, register at
  `.init_array.301` by filling only null slots. Concrete registrations always
  overwrite their coordinate, so their priority does not depend on whether
  they execute before or after the default registration, even across units.
- If every PPC parameter of a function belongs to a closed generalization and
  the function has no local concrete all-general default handler, a later
  `.init_array.351` completeness
  checker aborts before `main` if any specialization tuple is still uncovered.
  Actual defaults from other units have already filled holes by this phase.
- Non-strict open/opaque declarations without a real default fill remaining
  null slots with generated diagnostic handlers at `.init_array.361`, after
  all closed completeness checks. Such a stub cannot hide a closed hole or
  block a real default registered in another unit. Global PPC initialization
  at priority 401 still happens after all these phases.
- The diagnostic handler has the same generic PPC signature as the table entry.
  It prints the PPC function name and the actual runtime tag coordinates, for
  example `PPC dispatch error: uncovered handler for Missing<0, 1>()`, and then
  calls `abort()`.
- `create_spec(struct Figure.rectangle)` lowers to an internal expression that
  allocates `sizeof(struct Figure.rectangle)` bytes with `calloc`, writes every
  reachable hidden tag field from the generated PPC type, and returns
  `struct Figure.rectangle *`. Passing a generalization type creates a tag-0
  object. If the generalization has a named default specialization, the object
  is allocated with the default specialization layout.
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
- `get_spec_size(struct Figure)` is a literal for a visible closed definition,
  otherwise `__ppc_Figure_count + 1`; both include the zero coordinate.
- `get_spec_ptr(struct Figure, i)` stores `i` in a local temporary, checks that
  `0 <= i < get_spec_size(struct Figure)`, and returns
  `(struct Figure *)&__ppc_Figure_spec_ptr_tags[i]`. The returned pointer is a
  tag-only pseudo object and must not be used for field access.
- `spec_index_cmp(a, b)` stores both operands in temporaries, reads their
  hidden tag fields, and returns the common tag value if they match, otherwise
  `-1`. Both operands must point into the same PPC generalization family.
- Table finalizers in `.fini_array.201` call `free()` for allocated dispatch
  tables and reset the common table pointer to zero. They are safe if several
  translation units emitted the same finalizer pattern.
- Tag-carrier finalizers in `.fini_array.151` free arrays allocated for
  `get_spec_ptr` and reset the common pointer to zero.
- Handler constructors in `.init_array.301` write specialization handler
  pointers into the table using literal closed indices or open runtime tags. A `Base.void`
  handler, or a named default-specialization handler, contributes a literal
  zero coordinate.
- PPC handler implementation symbols follow ordinary C linkage rules. A
  non-`static` handler definition such as `RunThread<ThreadData.Rect *r>()`
  emits an externally visible generated symbol, so constructors emitted from
  header declarations in other translation units can resolve it at link time.
- PPC specialization types receive stable internal names derived from their
  source-level path, for example `Figure_rect`, `Figure_trian`, and
  `Shape_color_rect`. These names are used for generated handler symbols;
  runtime dispatch table coordinates use literal closed indices or
  constructor-assigned open tags.
- A named incomplete struct used in PPC syntax is treated as an external PPC
  generalization. The compiler emits the common runtime symbols needed for the
  family and reads the hidden first tag through the pointer ABI, without
  requiring the full `case` declaration in that translation unit. Full
  declarations are still required where specialization names, object layout,
  `create_spec`, or specialization fields are used.
- Global/static PPC variable constructors in `.init_array.401` initialize all
  runtime hidden tag fields reachable inside the variable after specialization
  tags have been assigned. Enum feature initializers retain their selected
  feature for each initialized object or array element instead of resetting
  those objects to the declared zero specialization.
- PPC calls are lowered to a table lookup by reading the hidden first tag field;
  the previous `switch`-based prototype dispatch has been removed.

Still limited:

- packed fixed-storage union generalizations are not supported in this first
  implementation; over-aligned common fields remain supported by ordinary
  struct layout
- enum feature initialization of thread-local storage is not supported;
  such initializers are diagnosed
- multimethod dispatch is implemented as a dense flat table; sparse-table
  compression is not implemented yet
- chain-aware PPC function dispatch is not implemented yet; PPC calls still
  dispatch on the first hidden tag of each pointer argument
- `get_spec_ptr` and `spec_index_cmp` are still first-tag helper operations.
  They do not yet produce or compare an entire specialization chain as a
  semantic unit.
- mixed wildcard handlers such as `(specific, general)` are not expanded across
  a matching subspace yet; use `Base.void` or the named default specialization
  for the concrete zero coordinate, or use the all-general handler as the
  default for the whole table
- runtime symbols are still generated directly by the compiler prototype; the
  declaration/definition model should be made more explicit before treating
  this as a stable ABI
- no linker extension is required in this snapshot
- `create_spec` currently owns allocation only; explicit deallocation helpers
  and ownership conventions are not defined yet
- the basic runtime helper set now covers `init_spec`, `create_spec`,
  `get_spec_size`, `get_spec_ptr`, and `spec_index_cmp`

Validation:

- out-of-source CMake build with `mkdir build && cd build && cmake ..`
- `make test`
- `make test-all`
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
- `create_spec` coverage in `test/ppc.c` for a generalization, a named default
  specialization, `Base.void`, ordinary specializations, typedef-based
  specializations, bodyless specializations, and recurrent specialization
  chains
- `init_spec` coverage in `test/ppc.c` for a generalization, `Base.void`, an
  ordinary specialization through a generalization pointer, and a bodyless
  specialization
- `init_spec` coverage for arbitrary memory passed as `void *`, including
  `Shape.color.rect` and recurrent `Shape.color.color.rect` chains
- `get_spec_size` and `get_spec_ptr` coverage in `test/ppc.c` for ordinary,
  typedef-based, enum-like, bodyless, and named-default PPC generalizations,
  including tag-only dispatch for one-parameter functions and multimethods
- enum-ADT coverage in `test/ppc.c` and `test/driver.sh` for `enum { ... }`,
  feature-list extension, same-size alternatives, assignment into `Base` and
  `Base.void`, initializer assignment into `Base`, and rejection of reverse or
  cross-concrete assignment
- `spec_index_cmp` coverage in `test/ppc.c` for equal concrete
  specializations, different concrete specializations, tag-only pseudo
  pointers, the zero coordinate, bodyless specializations, and named default
  specializations
- specialization-chain variable coverage in `test/ppc.c` for
  `Shape.color.rect`, recurrent `Shape.color.color.rect`, nested field access,
  local tag initialization, and global/static constructor tag initialization
- specialization-chain array coverage in `test/ppc.c` for local,
  initialized, global, static, dynamically created, and arbitrary-memory arrays
  of `Shape.color.rect` and recurrent `Shape.color.color.rect`
- PPC aggregate-initializer coverage in `test/ppc.c` for compact positional,
  grouped specialization-body, and designated-field initialization styles
- anonymous struct/union specialization coverage in `test/ppc.c` for inline
  `case` entries, `+` extensions, shared anonymous struct bodies for several
  specialization names, field access, dispatch, `create_spec`, `get_spec_size`,
  `get_spec_ptr`, and `spec_index_cmp`
- named-default specialization coverage in `test/ppc.c` and `test/driver.sh`
  for tag-0 handlers by name, rejected `Base.void`, direct default-object
  initialization, `create_spec(Base)`, dense `get_spec_ptr` indices, default
  strict handlers, and obsolete `= 0` syntax rejection
- special-function boundary coverage in `test/driver.sh` for ordinary
  `get_spec_ptr` lower/upper bounds and named-default `get_spec_ptr` upper
  bounds
- manual out-of-range `get_spec_ptr` diagnostic check
- `nullptr` runtime coverage in `test/nullptr.c` for its distinct type,
  initialization, conversions, equality, conditional operands, side effects,
  object/function pointers, and register/stack variadic arguments
- `nullptr` constraint diagnostics in `test/driver.sh` and PPC pointer
  initialization in `test/ppc.c`; rules follow WG14 N3096 sections 6.3.2.4,
  6.5.4, 6.5.9, 6.5.15 and 7.21.2
- pointer-list coverage in `test/ppc.c`, `test/ppc-multifile`, and
  `test/driver.sh` for empty/closed lists, comma-separated pointer declarations,
  layout, static initialization, zero aliases, polymorphic copying, arrays,
  multimethod dispatch, separate extensions, and rejected declarations or
  assignments
- fixed-storage union coverage in `test/ppc.c`, `test/ppc-multifile`, and
  `test/driver.sh` for inferred, type-sized, expression-sized and explicitly
  aligned storage, mixed and concrete arrays, by-value copying, dispatch across
  translation units, evolutionary extension, and invalid layout or assignment
  diagnostics
- the original pointer-list example in `examples/pointer-list/main.c`, built
  and compared against `expected.txt` by the driver tests
- enum feature assignment coverage in `test/ppc.c`, `test/driver.sh` and
  `test/ppc-multifile` for preserved common fields, zero aliases, array lvalues
  with side effects, reused names, initialization and constructor ordering,
  runtime tag registration across translation units, and rejected receivers
