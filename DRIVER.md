# The driver contract

A driver is the adapter one engine implementation ships so this kit can ask it questions. The
vectors live here, outside every implementation, so that no implementation defines its own exam; the
driver lives in the implementation's own repository, because an adapter to an engine can be
maintained nowhere else.

## The pipe

A driver is any command. The runner spawns it once per run, writes **one vector per line of JSON to
stdin**, closes stdin, and reads **one outcome per line of JSON from stdout**.

```
{"id":"supports/refuses-another-application","op":"supports","covers":"12.1","given":{...},"then":{...}}
```

```
{"id":"supports/refuses-another-application","witnessed":true,"result":{"refusal":{"detail":"...","fix":"..."}}}
```

- **Outcomes are matched by `id`, never by position.** Answer in any order, and a missing answer
  shifts nothing.
- **An answer the runner cannot parse is reported, not ignored.** Write diagnostics to stderr, never
  to stdout.
- **`then` is for the runner, not for the driver.** A driver that reads it is marking its own paper;
  answer from the engine and let the runner judge.
- **The exit code is not the verdict, the answers are.** It is reported beside them, and a non-zero
  exit fails the run on its own, but a driver cannot pass a vector by exiting 0.
- **A driver that never answers is killed** by a per-run wall clock, and every unanswered vector is
  reported as errored. An absence is never a pass.

## Virtual time

`budgetMillis` and `pollMillis` in a vector are **logical**. A driver advances a clock it controls
and must not sleep in real time: a 30 second budget costing 30 seconds of wall clock is a suite
nobody runs. Both of today's implementations already have the seam, `ConsumerBoundDirectory`'s
injected clock and sleeper on the kernel side and `ManualScheduler` in bayonet.

## Partial witness

A driver that cannot answer a vector answers `{"id":"...","witnessed":false}`. This is **not a skip
and not a pass**: the runner prints it as `unwitnessed` and the run exits non-zero. Then say so in
the driver's own documentation, naming which families it cannot witness and why, because an
implementation that quietly declares a vector inapplicable is grading itself.

## What `then` can assert

A vector asserts decisions and never wording, so two implementations in two languages need not
phrase a message identically. Each key of `then` is compared against the same key of `result`:

| in `then` | holds when |
| --- | --- |
| `null` | the key is absent or null in the result |
| `{"mentions": ["a", "b"]}` | the result's text, its `detail`, `message` and `fix` joined, contains each substring |
| `{"hasFix": true}` | the result carries a non-empty `fix` |
| `{"before": [["a", "b"]]}` | the result is an array in which `a` precedes `b`, pairwise, because a valid topological order is not unique |
| `{"contains": [x]}` | the result is an array containing each element |
| `{"equals": x}` | the result equals `x` exactly |
| `{"absent": true}` | the key is absent or null |
| anything else | deep equality |

A key absent from `then` asserts nothing.
