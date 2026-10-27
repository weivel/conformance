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
injected clock and sleeper on the kernel side and `ManualScheduler` in `weivel/java`.

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

Given `{ edgeSource: "capabilities" | "services", hostProvided?: [id], units: [{ id, provides:
[{ id, kind }], requires: [{ id, kind, optional }] }] }`.
Answers `{ order: [id], edges: [[provider, consumer]], cycle: [id] | null }`.
`edgeSource` is a parameter and not a convention: an engine that draws capability edges while asked
for service edges is answering a question nobody put to it. A cycle is **reported** with its members
rather than thrown, and `order` is then empty.
`hostProvided`, when present, lists service ids the host itself supplies: they meet a mandatory
need exactly as a unit's offer would, and the host never appears in `order`.

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

Given `{ schema, manifest, wellKnownServices? }`, where `schema` is the published vocabulary's URL.
**A driver validates against the published schema and never against a restatement of it**, then
applies `weivel/model`'s `VALIDATION.md` rules. `wellKnownServices` is the host's list for rule V2;
absent means V2 is not applied, and an empty list refuses every bare id.
Answers `{ valid, paths: [field], refusal: { detail, fix } | null }`.
**`paths` names every field that is wrong, each once**: a `then` applies whatever else is wrong, as
draft-07 applies it, and a repeated item is reported at the index of its second occurrence
(`capabilities[1]`), the convention V1 states, rather than at the array. A validator whose own
error for `uniqueItems` names the array maps it to that index.
An unknown field **validates** and is ignored: no object in the schema closes
`additionalProperties`, deliberately, so a manifest written against a later version still loads
today. A vector rejecting it would invert that decision.

### `carry`

Given `{ manifest }`. Answers `{ sameObject }`. The driver builds the engine's manifest facts with
`manifest` as their payload, opens a plugin session from those facts, and answers whether the
payload the session hands back is the very object it passed in, not an equal copy. Invariant 4: the
engine carries the host's manifest through uninspected, which is what lets one model serve hosts
whose manifest types are unrelated.

### `want`

Given `{ service: { id }, registerAtMillis, timeoutMillis, horizonMillis }`, all in virtual time.
One consumer asks for `service.id` at time 0, with a timeout only when `timeoutMillis` is non-null;
a second plugin registers it at `registerAtMillis`, or never when that is null; the driver then
advances its clock to `horizonMillis`. **The driver lets that plugin register `service.id` in
whatever form it has**, declaring it well-known if the engine asks: the vector is about waiting, and
a namespacing refusal would fail it for a reason unrelated to invariant 8.
Answers `{ settled, timedOut, atMillis }`, `atMillis` being the virtual time the ask settled or
timed out, and null when neither happened by the horizon.
**An absent timeout means none, never a default.** Invariant 8: waiting is unbounded unless the
consumer asks otherwise, because a provider that cannot exist today may exist once a node joins.

### `scope`

Given `{ steps }`, run in order, each one of `{ consumer, register: id }`, `{ consumer, want: id }`,
`{ release: consumer }` and `{ lookup: id }`.
Answers `{ lookups: [boolean], wants: ["settled" | "pending" | "rejected"] }`: one entry per
`lookup` step, true when the id resolves at that step, and one per `want` step, read after the last
step. Invariant 11: a consumer's view of the registry is bound to it, so releasing one consumer
removes exactly what it registered and rejects exactly what it still waits for.

### `reach`

Given `{ hostProvided: [id], plugins: [{ id, consumes: [id], provides: [id] }], steps }`.
**The driver drives a HOST, never a bare service hub**: it builds a manifest per plugin with
`consumes` and `provides` under `services`, opens each plugin's context through the host in the
order given, and registers each `hostProvided` id as a host service before the first step. The
refusal lives in a fence the host builds from the manifest, so a driver that calls the hub directly
answers a question about an engine that never refuses.
Steps run in order, each one of `{ plugin, register: id }`, `{ plugin, get: id }`,
`{ plugin, want: id }`, `{ plugin, watch: id }` and `{ plugin, ids: true }`, every call made through
that plugin's own context. A vector registers only an id the plugin lists under `provides`.
Answers `{ outcomes, reported: [[plugin, id]], refusal: { detail, fix } | { message } | null }`:

- `outcomes` holds one entry per `get`, `want`, `watch` and `ids` step, in step order; `register`
  answers nothing. A `get` is read at its step, a `want` and a `watch` after the last step.
- A `get`, `want` or `watch` answers `served`, `absent` or `refused`. `served`: the `get` returned
  the service, the `want` settled with it, the `watch` listener heard it registered. Otherwise a
  `want` that was rejected is `refused` and one still waiting is `absent`; a `get` or `watch` is
  `refused` when the host reported that plugin and id, and `absent` when it did not. **A refused
  `want` is reported too**, exactly as a refused `get` or `watch` is, so all three appear in
  `reported`.
- An `ids` step answers the ids that plugin's context lists at that step, **sorted**, so a vector
  does not pin the order an engine keeps them in. The context lists an id only when it is
  registered AND that plugin may reach it, so a declared id that nothing provides is not listed.
- `reported` holds one `[plugin, id]` pair per refusal the host reported, in the order reported, so
  a pair reported twice appears twice. **It is read from a structured refusal record the engine
  keeps, each entry a plugin id and a service id, and never parsed or matched out of diagnostic
  text**: a driver that infers pairs from wording would pass an engine that only prints.
- `refusal` is the first refusal of the run as text, the rejection of a `want` or the diagnostic a
  `get` or `watch` emitted. It exists only for the `mentions` check of the fix and decides nothing
  about `outcomes` or `reported`.

Invariant 12: a consumer reaches only the services it declared under `services.consumes` or offers
under `services.provides`, host services included. The refusal is data and is reported once per
plugin and id, so a polling plugin does not flood the sink, and a declared id that nothing provides
is absence, not a refusal.

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
