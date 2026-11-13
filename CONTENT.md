## What this is

The plugin core's behaviour, written down as data rather than as a test suite inside one implementation. A
**vector** names a given state, one operation and the outcome any engine must produce. The **runner**,
`weivel-conformance`, is a C program that loads the vectors, asks an **adapter** (a C function that drives the core,
or the weivel standard, in-process) for each answer, and prints what each can and cannot do.

**The vectors live outside every implementation on purpose.** An engine whose own repository holds its conformance
suite defines its own exam, and the first thing it does when the suite disagrees with it is change the suite.

## Layout

- `vectors/core/<op>/*.json`: vectors the runner judges against the core.
- `vectors/standard/<op>/*.json`: vectors the runner judges against the weivel standard, the replaceable layer above
  the core.
- `vectors/layer/<op>/*.json`: vectors only a language layer can witness, described in `vectors/layer/LAYER.md`.
- `RUNNER.md`: the runner and adapter contract, each op's given and answer, and what `then` can assert. Read this
  before writing an adapter.
- `runner/src/`: the loader, matcher, report, and the adapters under `ops/`.
- `test/`: the runner's own tests and fixtures.

## Running it

The core is built from a checkout linked at `siblings/core`, and the standard from one linked at `siblings/standard`.

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
build/bin/weivel-conformance run vectors
```

`run <directory>` accepts the whole corpus or any directory inside it. A vector's tier comes from the last `core`,
`standard` or `layer` directory in its path, and a vector under none is refused.

The kit's own tests sit the pinned core against `vectors/core` and the pinned standard against `vectors/standard`,
and the core's drift job sits the core under test against the same corpus, so a core change that breaks a vector
fails in the core's own CI.

## What a result means

| status | meaning |
| --- | --- |
| `passed` | the adapter answered, and the answer satisfies the vector |
| `failed` | the adapter answered, and the answer does not |
| `errored` | no adapter answered the vector, or its answer could not be read |
| `awaiting a layer` | a layer vector, to be witnessed by a language's own layer suite |

**Only `passed` is a pass.** Every other status is reported in its own column and the run exits non-zero: a suite
that counts a vector it did not run as a pass reports a green it has not earned.

## The core control call benchmark

`weivel-benchmark-control-call measure` prints the median, p99 and spread of one `weivel_get_service` on a warm core;
`gate <budget-file>` exits 1 when the median is over `benchmarks/budget.txt`, and 2 when the budget is missing or
malformed or nothing was measured. It warms with 200000 calls, then times 2000 batches of 1000 calls. A timing verdict
from an unoptimised build means nothing, so the committed-budget test runs in a Release build only and skips elsewhere.

The committed budget, `median_nanoseconds 360`, is 2.5 times the slowest CI runner's highest median. The core's
drift job measured these medians in Release on 2026-11-12, four readings per runner over two runs: Windows 133.0 to
143.9 ns, macOS 85 to 97 ns, Linux 63.2 to 69.3 ns. The factor is wide enough for runner noise and narrow enough that
an added system call, allocation or sleep in the call path trips it.

The core's drift job reads this kit at a pinned commit, the `kit_ref` of the core's drift workflow, so a change to a
check, list or budget here reaches the core only when that pin moves, in the same wave.
