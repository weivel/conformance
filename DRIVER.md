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

## What each op is given, and what it answers

Every shape below is a contract both drivers read. A driver answers the keys it can and omits the
rest; a key the vector does not assert is ignored, and a key the vector asserts and the driver omits
fails the vector.

### `supports`

Given `{ host: { applications, api }, facts }`, the manifest facts as the engine holds them.
Answers `{ supported, refusal: { detail, fix } | null, axis: "application" | "api" | null }`.
`axis` names which check refused first, because a refusal that cites the api floor when the artifact
was built for another application sends the author to the wrong line.

### `boot`

Given `{ units: [{ id, provides, requires: [{ id, optional, degradable }] }] }`.
Answers `{ started: [id], refusal: { detail, fix } | null }`.
**`optional` and `degradable` are different axes.** `optional` decides whether an absence is
tolerated; `degradable` decides what a present-but-unsatisfying provider costs. A requirement that is
`degradable` and not `optional` still refuses when it is absent.

### `order`

Given `{ edgeSource: "capabilities" | "services", units: [{ id, provides: [{ id, kind }],
requires: [{ id, kind, optional }] }] }`.
Answers `{ order: [id], edges: [[provider, consumer]], cycle: [id] | null }`.
`edgeSource` is a parameter and not a convention: an engine that draws capability edges while asked
for service edges is answering a question nobody put to it. A cycle is **reported** with its members
rather than thrown, and `order` is then empty.

### `resolve`

Given `{ requirement: { id, constraints, degradable }, provider: { advertised, attributesByPoll },
budgetMillis, pollMillis }`. Time is virtual: each entry of `attributesByPoll` is what the provider
reports at that poll, and a driver advances a clock it controls.
Answers `{ served, atPoll, degraded, attributes, asks: [boolean], report, refusal }`.
`asks` is one boolean per time the engine asked the provider to serve, true when that ask carried
the consumer's constraints, so the degradable fallback asking once more with them before asking
unconstrained reads as `[true, false]` and is observable rather than inferred. It is a boolean
rather than the constraint set itself because a vector asserts a DECISION, and the set would pin
how an engine spells what it already decided.
**An entry of `{}` means the provider has nothing live to add, not that it offers nothing**, so the
descriptor's advertised claim stands, per key.

### `revoke`

Given `{ service: { id }, steps }`, a script of `register`, `lookup`, `revoke` and `use`.
Answers `{ lookups: [boolean], used, reason }`. `reason` is a token such as `revoked`, never a
sentence: a consumer holding a revoked handle has to learn what happened, and the kit must not pin
how it is phrased.

### `validate`

Given `{ schema, manifest }`, where `schema` is the published vocabulary's URL. **A driver validates
against the published schema and never against a restatement of it.**
Answers `{ valid, paths: [field], refusal: { detail, fix } | null }`.
An unknown field **validates** and is ignored: no object in the schema closes
`additionalProperties`, deliberately, so a manifest written against a later version still loads
today. A vector rejecting it would invert that decision.

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
