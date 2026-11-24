# Benchmarks

## The core control call benchmark

`weivel-benchmark-control-call measure` prints the median, p99 and spread of one `weivel_get_service` on a warm core;
`gate <budget-file>` exits 1 when the median is over `benchmarks/budget.txt`, and 2 when the budget is missing or
malformed or nothing was measured. It warms with 200000 calls, then times 2000 batches of 1000 calls. A timing verdict
from an unoptimised build means nothing, so the committed-budget test runs in a Release build only and skips elsewhere.

The committed budget, `median_nanoseconds 360`, is 2.5 times the slowest CI runner's highest median. The core's
drift job measured these medians in Release on 2026-11-12, four readings per runner over two runs: Windows 133.0 to
143.9 ns, macOS 85 to 97 ns, Linux 63.2 to 69.3 ns. The factor is wide enough for runner noise and narrow enough that
an added system call, allocation or sleep in the call path trips it.

## The language benchmark rows

Beside `control-call`, a language that binds descriptions carries the `described-call` row, and a language that moves
a blocking call off its calling thread carries `blocking-call`: the described fixture's walker `wait` through a bound
object, over the same `wait` called directly. Both bounds live in the language's file under `benchmarks/`, and the
budgets below state how each row is measured.

## Benchmark budgets

Every timing bound lives in this kit, so no implementation sets its own. `benchmarks/budget.txt` is the core's: one line,
`median_nanoseconds <number>`, the median a Release build of the core's control call may take, held by
`weivel-benchmark-control-call gate`.

A language implementation reads its bounds from `benchmarks/<language>.txt` of the kit commit it pins. Each line is
fields separated by one space:

- `<gate> <platform> <number>`: `control-call` is weivel's control call over the same core function called through
  the bare binding, `library-call` a C library function reached through weivel over the same function reached
  directly, with the same arguments and its answer released the same way. A host that registers definitions adds a
  definition row to `library-call`: an operation of the echo library's definition called through the definition a
  consumer resolved, over the same operation called through the pointer the definition's symbol answered. The two
  are the same pointer, so the ratio is 1 by construction and bounds only the measurement's noise, provided that:
  each path is handed the one address itself, never a place of its own that holds it, since a load from a path's own
  place can alias the operation's stores on that path alone; and both paths read the operation's function pointer out
  of the struct the same way, both once before the timed loop or both on every call, since one load more per call on
  one side is a cost and not noise. A language may sample this row in rounds and batch lengths of its own, unlike its
  same-language pair's, stating its sampling in its docs. The row is judged by its platform's noise bounds
  and `noise-margin`, as the same-language pair is, carries no line of its own, and its gate is the identity
  `library/a-definition-is-the-providers-own-pointer` witnesses. `described-call` is a language binding a described
  definition generically: an operation of the standard's `described` fixture called through the bound object a
  consumer resolved, over the same operation called through a bare binding of the fixture's structs written by hand,
  with the same input and its answer and refusal released the same way. It is required of every language that
  `checks/languages.txt` does not list as `layer-exempt` from `description/binding`, since each of those binds
  descriptions, and on every platform of every backend its file names; a language exempt from it, as C is, need not
  carry it, and one that carries it carries it on every platform of every backend. `blocking-call` is a language
  that calls a `@blocking` operation off its calling thread: the described fixture's walker `wait` through the bound
  object a consumer resolved, each call awaited before the next, over the same `wait` called directly on the calling
  thread. It is optional, carried by a language whose binding moves a blocking call off its calling thread, and on
  every platform of every backend once carried. A C layer crosses no foreign interface, so its bare call is the same
  control call made through the core's function table without the layer's wrapper, and `control-call` bounds what
  the wrapper adds. The platform is the runner's system, `linux`, `windows` or `macos`, whatever the language itself
  calls it. The number is the highest per-round median ratio CI measured there, and the bound is that number times
  `ratio-margin`. A platform with no line has no bound, and a gate run there fails. A language that reaches C through
  more than one backend names the backend after the gate and a hyphen, `control-call-ffm`, and bounds each backend's
  gates on every platform; its noise lines stay per platform, since the same-language pair crosses no backend.
- `ratio-margin <number>`: how far over its highest measured ratio a run may go.
- `quiet-noise <platform> <number>`: how far from 1 the ratio of two paths of identical cost is on a quiet run there.
- `worst-noise <platform> <number>`: the noisiest such deviation measured there, times a stated margin; a run whose
  two identical direct paths differ by more is refused as too noisy to judge, which fails the gate. The noise is per
  platform because the runners differ by more than a margin can carry: in C, macOS's two identical paths differ by
  about twelve times what Linux's and Windows' do at the median and over twenty times at the worst, so one bound either
  refuses a quarter of macOS runs or barely judges Linux and Windows.
- `noise-margin <platform> <number>`: how many times its own noise, or its platform's quiet noise when it was
  quieter, a same-language pair there may differ by. It is per platform for the reason the noise is: one margin is
  set by the noisiest platform's largest sample, and in Java's samples macOS's would let a quiet Linux pair differ by
  half again what Linux's own samples ever needed.

Each required gate and each optional gate a file carries, each noise bound and each noise margin on each platform, and
the ratio margin, appears exactly once, every number positive; `language_budget_of_<language>_is_well_formed` holds
each file to that. A platform with no noise line has no noise bound, and the same-language gate run there fails.

**The same-language gate in every language is the identity check**: the service a consumer resolves is the
provider's own object, pointer equality in C and reference identity elsewhere. The noise bounds only judge the
timing a language reports beside it. A C layer also gates its control call through its wrapper against the core's
own `budget.txt`, and its `c.txt` bounds what it adds over the core's table and over a C library's own function.

`typescript.txt` carries `weivel/typescript`'s ratio bounds as its Test workflow measured them on 2026-11-14 in Test
runs 43592067436, 43592102705, 43592105961, 43592109026, 43592111939 and 43592115298: the control call,
weivel_get_service through the wrapper with the core's timer armed, over the same weivel_get_service on the same core
through the very koffi function the wrapper calls, and the echo library's call through the host's library service over
`echo_echo` with the same input through a bare koffi binding, its answer released as the host releases it. The commit
that sets them lists the samples. Its `described-call` bounds are as its Test workflow measured them on 2026-11-20,
seven samples a platform, in attempts 1 to 6 of Test run 44739578781 and attempt 1 of Test run 44741595646: the
described mirror's `reflect`, a sample holding a text and a counted array of three texts in and its reflection out,
through the bound object a consumer resolved, over the same operation through a bare koffi binding of the fixture's
structs declared by hand. The commit that sets them lists the samples. Its `blocking-call` bounds are as its Test
workflow measured them on 2026-11-20 and 2026-11-21, one sample a platform in each of Test runs 44840186687,
44847576351 and 44852851668: the walker's `wait` through the bound object, which runs it on a worker and settles a
promise, over the same `wait` through a bare koffi binding called on the calling thread. The commit that sets them
lists the samples. Its noise bounds are as they stood on
2026-11-13, written once per platform, the same number on each since they were set from all platforms at once, in Test
runs 43438389364 and 43441441323.

`c.txt` carries `weivel/c`'s bounds as its Release `benchmarks` job measured them on 2026-11-14, ten repetitions a run
in Test runs 43573264428, 43573863406, 43574266777, 43574269766, 43574787768, 43574795017 and 43574798533: the
control call through the wrapper over the same call through the bare table, and the echo library's call through the
host's library service over the same function called directly. The noise is of million-call batches, where the two
direct paths and the call through what the consumer resolved run the very same instructions, in the last three of
those runs and in Test runs 43575892921, 43577159522, 43578132489, 43579040223, 43579429710, 43579838546,
43580238459, 43580495133, 43580741472, 43581113400 and 43581462702, re-measured once the macOS runner refused runs as
too noisy against one bound for every platform. The commit that adds the file lists the ratio samples, and the commit
that makes the noise bounds per platform lists every noise sample and how each noise bound follows from them.

`java.txt` carries `weivel/java`'s bounds as its Test workflow's `benchmarks` job measured them on 2026-11-15, ten
repetitions a run on each of its two backends, in Test runs 43665043985, 43665044351, 43665047536, 43665050907,
43665054380, 43665057974, 43665060963 and 43665064420. A ratio gate names its backend: `-ffm` is the foreign function
API on Java 22 and later, `-jna` is JNA on Java 8 to 21. The control call is weivel_get_service through the wrapper,
with the core's timer armed as a host arms it, over the same function of the core's table on the same core, plugin and
id called bare; the library call is the echo library's `echo_echo` through the host's library service with a heap
array, over the same function called bare with the same bytes copied in and its answer copied out and released as the
host does. The noise is of million-call batches on the provider's own object, each batch on a path object made for it,
pooled over both backends since the pair crosses none. The commit that adds the file lists every sample, how each
bound follows from them, and what the outliers were.
Its `described-call-ffm` and `described-call-jna` bounds are as its Test workflow's `benchmarks` job measured them on
2026-11-20, one gating sample a backend and platform in each of attempts 1 to 6 of Test run 44775350720 and attempt 1
of Test run 44776432788, macOS one fewer since the first attempt's macOS job was cancelled: the described
mirror's `reflect`, a sample holding a text and a counted array of three texts in and its reflection out, through the
bound operation a consumer resolved, over the same operation through the mirror's own function pointers with the
fixture's structs declared by hand, every member the binding answers copied out into Java values and the reflection
released as the binding releases it. The samples of Test run 44765952616 are not among them: they measured the
binding before its answers became shared-key values, a path that no longer exists. Java's hand control writes per call
only the members it changes, where TypeScript's koffi control encodes its whole struct each call, so the Java gate is
slightly stricter and never falsely green. Its `worst-noise` on every platform was re-derived on the same day, the
noisiest noise sample times 1.5 as before, from every noise sample of those runs, of Test run 44765952616 attempts 1
to 6 and of Test run 44752588723 attempt 1. Three definition or same-language samples had tripped the old bounds: on
Linux 0.004118 in 44752588723 and 0.009986 in 44776432788, and on Windows 0.014668 in attempt 4 of 44765952616.
macOS's stands, since none of the new samples is as noisy as the samples it was set from. The commit that sets them
lists every sample.
