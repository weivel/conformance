## What this is

The plugin engine's behaviour, written down as data rather than as a test suite inside one
implementation. A **vector** names a given state, one operation and the outcome any engine must
produce; a **driver** is a small adapter an implementation ships; the **runner** spawns a driver,
feeds it the vectors and prints what that implementation can and cannot do.

**The vectors live outside every implementation on purpose.** An engine whose own repository holds
its conformance suite defines its own exam, and the first thing it does when the suite disagrees with
it is change the suite.

## Layout

- `vector.schema.json`: the vector format, as draft-07. Every vector is validated against it before
  anything runs.
- `vectors/<op>/*.json`: the corpus, one file per vector, grouped by the operation it exercises.
- `DRIVER.md`: the driver contract. Read this before writing one.
- `runner/src/`: load and validate vectors, spawn a driver, match outcomes by id, report.

## Running it

```bash
npm install
npm test                      # the runner's own tests
npm run validate-vectors      # every vector against vector.schema.json
npx tsx runner/src/cli.ts run --driver "<command>" [--only <op>]
```

## What a result means

| status | meaning |
| --- | --- |
| `passed` | the driver answered, and the answer satisfies the vector |
| `failed` | the driver answered, and the answer does not |
| `errored` | the driver answered nothing for that vector |
| `unwitnessed` | the driver declared it cannot witness that vector |

**Only `passed` is a pass.** `unwitnessed` is a visible state and the run still exits non-zero: a
suite that lets a driver declare a vector inapplicable is a suite the implementation grades itself
against.
