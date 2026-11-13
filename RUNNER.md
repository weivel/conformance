# The runner

The runner is `weivel-conformance`, a C program in this repository. It walks a directory of vectors, asks an
adapter for each vector's answer, judges the answer against the vector's `then`, and reports. The vectors live
here, outside every implementation, so that no implementation defines its own exam.

```
weivel-conformance run <directory>
```

The exit code is 0 only when at least one vector ran and every vector passed, 1 when any did not, and 2 when the
corpus is refused or the command is misused. A corpus is refused, before anything runs, when it holds no vector,
a file that is not JSON, an op the kit does not know, a vector asserting nothing, an id declared twice, or an id
whose family is not its op.

## Tiers

A vector's directory says which suite judges it.

- `vectors/core/<op>/` is judged by the runner, through an adapter that drives the core in-process.
- `vectors/standard/<op>/` is judged by the runner against the weivel standard, the replaceable layer above the
  core, through an adapter that drives the standard's library (`siblings/standard`) in-process.
- `vectors/layer/<op>/` is witnessed by each language's own layer suite, because the core never calls a host
  language. `vectors/layer/LAYER.md` says what each suite must witness. The runner loads and counts these and
  does not run them; the report lists them as awaiting a layer.

## Adapters

An adapter is a C function, one per op, registered in `runner/src/ops/ops.c` and declared by `runner/src/ops/ops.h`.
It is given the vector's `given` as a read-only JSON node, drives the core or the standard in-process, and writes
its answer with the `answer_*` functions as one JSON object. It never reads a vector's `then`: an adapter that does
is marking its own paper. An op whose adapter does not exist, or an adapter that answers nothing, makes its vectors
`errored`, and an absence is never a pass.

Only `passed` is a pass. `failed`, `errored` and `awaiting a layer` fail the run, and the report keeps each in
its own column so a reader sees what was not run.

An adapter reads whether a want still waits, or a watch still hears, through `weivel_handle_state`, which changes
nothing, and only after the last step. **It relies on waiting or watching against anything else**, a refused
query included: the core forgets how a want or a watch ended once its handle's slot is reused, so how a want ended
is read from its `WANT_SETTLED` event and never from the query.

## Virtual time

`budgetMillis`, `pollMillis` and the other `...Millis` members of a vector are logical. An adapter hands the core
the time it wants the core to see and never sleeps in real time: a 30 second budget costing 30 seconds of wall
clock is a suite nobody runs.

## What each op is given, and what it answers

Every shape below is a contract. An adapter writes the keys it can and omits the rest; a key the vector does not
assert is ignored, and a key the vector asserts and the adapter omits fails the vector.

A `refusal` in an answer is `{ code, detail, fix, subject, hasFix }` or null. `code` is the core's refusal code in
lowercase without its prefix, so `WEIVEL_REFUSAL_UNDECLARED` is `undeclared`. `subject` is what the refusal is about
as the core's header says for that code (the id reached for, the need nothing meets, the constraint missed, the
import name), null when the code names nothing, and `hasFix` is true when the fix is non-empty. Both are read off the
core's own `subject` and `has_fix`, never out of the wording. A vector asserts the code, the subject and hasFix,
and never the wording.

### `supports`

Given `{ host: { applications, api }, facts }`, the manifest facts as the core holds them.
Answers `{ supported, refusal, axis: "application" | "api" | null }`. `axis` names which check refused first,
because a refusal that cites the api floor when the artifact was built for another application sends the author to
the wrong line.

### `boot`

Given `{ units: [{ id, provides, requires: [{ id, optional, degradable }] }] }`.
Answers `{ started: [id], refusal }`.
**`optional` and `degradable` are different axes.** `optional` decides whether an absence is tolerated;
`degradable` decides what a present-but-unsatisfying provider costs. A requirement that is `degradable` and not
`optional` still refuses when it is absent.

### `order`

Given `{ edgeSource: "capabilities" | "services", hostProvided?: [id], units: [{ id, provides: [{ id, kind }],
requires: [{ id, kind, optional }] }] }`.
Answers `{ order: [id], edges: [[provider, consumer]], cycle: [id] | null }`.
`edgeSource` is a parameter and not a convention: an engine that draws capability edges while asked for service
edges is answering a question nobody put to it. A cycle is reported with its members rather than thrown, and
`order` is then empty. `hostProvided`, when present, lists service ids the host itself supplies: they meet a
mandatory need exactly as a unit's offer would, and the host never appears in `order`.

### `resolve`

Given `{ requirement: { id, constraints, degradable }, provider: { advertised, attributesByPoll,
servesOnlyUnconstrained? }, budgetMillis, pollMillis, vocabulary, wellKnownServices }`. Each entry of
`attributesByPoll` is what the provider reports at that poll; once the list runs out the provider keeps reporting
its last entry.
**`vocabulary` and `wellKnownServices` are the host's configuration, and the vector states them** because the core
mints neither: `vocabulary` is `[{ constraint, attribute, bound: "lower" | "upper" }]`, each naming which attribute
a constraint is compared against and which way it bounds it, and `wellKnownServices` lists the ids any plugin may
register, so the provider can register `requirement.id` without a namespace. A vector missing either is not
answered. The provider and the consumer are two plugins opened on one core; time starts at 0 and advances by
`pollMillis`.
Answers `{ served, atPoll, degraded, asks: [boolean], report, refusal }`, and nothing at all when the want never
settled. `served` is true when the want settled with the provider under its constraints, not degraded; `atPoll` is
the poll, counted from 1, whose attributes led to that service, and null otherwise. `report` is
`{ text, subject, hasFix }` for a degraded settlement and null otherwise: `text` is the warning the core logs just
before it, and `subject` and `hasFix` are the settlement's own, so the constraint the want accepted unmet is asserted
by content and never parsed out of the warning. A degraded want carries no refusal, so its `hasFix` is false.
`asks` is one boolean per time the core asked the provider to serve, true when that ask carried the consumer's
constraints, so the degradable fallback asking once more with them before asking unconstrained reads as
`[true, false]` and is observable rather than inferred. It is a boolean rather than the constraint set itself
because a vector asserts a decision, and the set would pin how an engine spells what it already decided.
**An entry of `{}` means the provider has nothing live to add, not that it offers nothing**, so the descriptor's
advertised claim stands, per key.

### `revoke`

Given `{ service: { id }, steps }`, a script of `register`, `lookup`, `revoke` and `use`.
One plugin registers `service.id`, as a well-known id, and another declares it and looks it up; `revoke` ends the
registration and `use` uses the handle the last lookup returned. The handle is unusable once the core has revoked
it from the consumer, and usable only while the core still hands back that same registration.
Answers `{ lookups: [boolean], used, reason }`. `reason` is a token such as `revoked`, never a sentence: a
consumer holding a revoked handle has to learn what happened, and the kit must not pin how it is phrased.

### `validate`

Given `{ schema, manifest, wellKnownServices? }`, where `schema` names the published vocabulary's file. The
manifest names itself with `importName`; a repository's identity is supplied from outside it.
**An adapter validates against the published schema and never against a restatement of it**, and alongside it
applies the rules the standard's `VALIDATION.md` states. `wellKnownServices` is the host's list for rule V2; absent means V2 is not applied,
and an empty list refuses every bare id.
Answers `{ valid, paths: [field], refusal }`, where `refusal` is the first wrong field as `{ code, detail, fix, path }`
or null, and `code` is the standard's refusal code in lowercase without its prefix, so
`WEIVEL_STANDARD_REFUSAL_NOT_WELL_KNOWN` is `not_well_known`. The standard publishes one schema, so an adapter answers
nothing for a vector whose `schema` names any file but `plugin.schema.json`. A refusal with an empty `detail` or `fix`
is no answer either, so a standard cannot pass a vector while leaving them out.
The adapter re-serialises the vector's `manifest` before handing it over, so a reader property no re-serialised
text can carry (a duplicate key, a byte order mark) is guarded by the reader's own tests, never by a vector.
**`paths` names every field that is wrong, each once**: a `then` applies whatever else is wrong, as draft-07
applies it, and a repeated item is reported at the index of its second occurrence (`capabilities.provides[1]`),
the convention V1 states, rather than at the array.
An unknown field **validates** and is ignored: no object in the schema closes `additionalProperties`,
deliberately, so a manifest written against a later version still loads today. A vector rejecting it would invert
that decision.

### `identity`

Given `{ value }`, a string offered as a repository identity. Answers `{ valid }`, true when the whole string
matches `^(local:[^/\s]+|[a-z][a-z0-9-]*(@[a-z0-9.-]+)?:[0-9]+)(/(?!\.{1,2}(/|(?![\s\S])))[a-z0-9._-]+)*(?![\s\S])`:
`local:` followed by a name, or a forge (with an optional `@host`) and a numeric repository id, then any number of
path segments. A bare name, a non-numeric or empty id, an uppercase forge and a `.` or `..` segment are all
invalid; a segment that only starts with or contains dots, such as `.hidden` or `a..b`, is valid.

### `unbound`

Given `{ manifest }`, a manifest whose `services.consumes` holds ids in the string form or the object form
`{ id, optional?, provider? }`. Answers `{ services: [id] }`, the consumed ids the core cannot tie to a provider
and so must report: a namespaced id (`calendar:events`) with no `provider`. An id bound by a `provider` is
omitted, and so is a bare id, because a bare id names a host or shared service and has no plugin to bind to. The
list keeps the manifest's order.

### `carry`

Given `{ manifest }`. Answers `{ sameObject }`. The adapter builds the core's manifest facts with `manifest` as
their payload, opens a plugin session from those facts, and answers whether the payload the session hands back is
the very object it passed in, not an equal copy. Invariant 4: the core carries the host's manifest through
uninspected, which is what lets one model serve hosts whose manifest types are unrelated.

### `want`

Given `{ service: { id }, registerAtMillis, timeoutMillis, horizonMillis }`, all in virtual time. One consumer
asks for `service.id` at time 0, with a timeout only when `timeoutMillis` is non-null; a second plugin registers it
at `registerAtMillis`, or never when that is null; the adapter then advances the time it hands the core to
`horizonMillis`. **The adapter lets that plugin register `service.id` in whatever form it has**, declaring it
well-known if the core asks: the vector is about waiting, and a namespacing refusal would fail it for a reason
unrelated to invariant 8.
Answers `{ settled, timedOut, atMillis, refusal }`, `atMillis` being the virtual time the ask settled or timed out,
and null when neither happened by the horizon, and `refusal` the timed-out want's refusal, null otherwise.
**A registration at the very moment of the deadline loses**: the adapter hands the core the deadline before it
makes the registration, so the want times out and the registration settles nothing. A layer running a want on its
own clock must keep that order; `vectors/layer/want/` holds the vector its suite witnesses.
**An absent timeout means none, never a default.** Invariant 8: waiting is unbounded unless the consumer asks
otherwise, because a provider that cannot exist today may exist once a node joins.

### `scope`

Given `{ steps }`, run in order, each one of `{ consumer, register: id }`, `{ consumer, want: id }`,
`{ release: consumer }` and `{ lookup: id }`.
Answers `{ lookups: [boolean], wants: ["settled" | "pending" | "rejected"] }`: one entry per `lookup` step, true
when the id resolves at that step, and one per `want` step, read after the last step. A `lookup` is made as the
host, and each consumer's manifest declares exactly the ids its own steps register and want, so the reach fence,
which `reach` asks about, never decides a scope step. Invariant 11: a consumer's
view of the registry is bound to it, so releasing one consumer removes exactly what it registered and rejects
exactly what it still waits for.

### `reach`

Given `{ hostProvided: [id], plugins: [{ id, importName?, consumes: [id | { id, optional, provider? }], provides:
[id] }], steps }`. `importName` defaults to `id`: it is the name a plugin's own services are namespaced by, so a
plugin with id `forge:2` and `importName` `calendar` registers `calendar:events`. A step names its plugin by `id`.
A `consumes` object with a `provider` is served only by the plugin with that id; without one it is served by
whichever plugin registered the service.
**The adapter drives a core with plugins opened on it, never a bare service registry**: it builds a manifest per
plugin with `consumes` and `provides` under `services`, opens each plugin in the order given, and registers each
`hostProvided` id as a host service before the first step. The refusal lives in the fence the core builds from the
manifest, so an adapter that bypasses it answers a question about an engine that never refuses. A `consumes` entry
goes into the manifest exactly as written, so the object form reaches the core's own reading of it.
Steps run in order, each one of `{ plugin, register: id }`, `{ plugin, get: id }`, `{ plugin, want: id }`,
`{ plugin, watch: id }` and `{ plugin, ids: true }`, every call made as that plugin. A vector registers only an id
the plugin lists under `provides`.
Two more steps are made as the host itself, never fenced: `{ host: true, register: id }` registers `id` as a host
service, and `{ host: true, unregister: id }` ends the host's live registration of it. A `register` step, the
plugin's or the host's, may name the `object` it registers.
Answers `{ outcomes, objects, reported: [[plugin, id]], refusal }`:

- `outcomes` holds one entry per `get`, `want`, `watch` and `ids` step, in step order; `register` answers nothing.
  A `get` is read at its step, a `want` and a `watch` after the last step.
- A `get`, `want` or `watch` answers `served`, `absent` or `refused`. `served`: the `get` returned the service, the
  `want` settled with it, the `watch` heard it registered. Otherwise a `want` that was rejected is `refused` and
  one still waiting is `absent`; a `get` or `watch` is `refused` when the core reported that plugin and id, and
  `absent` when it did not. **A refused `want` is reported too**, exactly as a refused `get` or `watch` is.
- An `ids` step answers the ids that plugin may list at that step, **sorted**, so a vector does not pin the order
  the core keeps them in. An id is listed only when it is registered AND that plugin may reach it, so a declared
  id that nothing provides is not listed.
- `objects` holds one entry per `get`, `want` and `watch` step, in step order: the `object` of the registration
  that served it, null when none did or that registration named no object. **It is read off the registration
  handle the core handed back**, which stands for the object because the core holds none, never off which
  registration the script made last, so a core that still serves a replaced host service is caught.
- `reported` holds one `[plugin, id]` pair per refusal the core reported, in the order reported, so a pair
  reported twice appears twice. **It is read from the core's refusal events, each carrying a plugin and a service
  id, and never parsed out of diagnostic text.**
- `refusal` is the first refusal of the run, null when there was none.

Invariant 12: a consumer reaches only the services it declared under `services.consumes` or offers under
`services.provides`, host services included. The refusal is data and is reported once per plugin and id, so a
polling plugin does not flood the sink, and a declared id that nothing provides is absence, not a refusal. The
declaration check comes before the late (no longer running) check for `get`, `want` and `watch`, so an undeclared
call from a plugin that is gone is refused and reported as undeclared, and only a declared one takes the late
path; `ids` ignores lateness.

### `consume`

Given `{ plugins: [{ id, capabilities: { provides: [id], consumes: [id] } }], steps }`. A step names its plugin by
`id`. `capabilities` goes into the manifest exactly as written, so the core reads the declaration itself.
**The adapter drives a core with plugins opened on it, never a bare registry**: it builds a manifest per plugin,
opens each plugin in the order given, and makes every call as that plugin. The refusal and the verified boundary
both live in the core, so an adapter that bypasses it answers a question about an engine that never refuses.
**Each `provide` step hands the core an implementation that names its provider**, so a consumer's answer is read
off the implementations the core returned and never inferred from the steps the adapter ran.
Steps run in order, each one of `{ plugin, provide: id }`, `{ plugin, verify: true }`, `{ plugin, all: id }`,
`{ plugin, watch: id }`, `{ plugin, quarantine: true }` and `{ plugin, release: true }`:

- `provide` provides the named capability as the plugin. A vector provides only an id the plugin lists under
  `capabilities.provides`.
- `verify` runs the core's own activation check for that plugin, which compares what it provided with what it
  declared. A plugin whose check passes is verified. **A plugin whose check refuses is quarantined exactly as a
  host would.** A vector produces a refused activation the real way: the plugin lists a capability under
  `provides` that no step provides, and a `verify` step follows. The adapter has no flag that forces a refusal.
  **The quarantine follows the refused check in the same step, before the next one runs**, made by the host and
  never named by a step, so the refused plugin's own watches stop there. A layer's host must quarantine the same
  way; `vectors/layer/consume/` holds the vector its suite witnesses.
- `quarantine` marks the plugin broken, as a host does for a plugin that fails at run time. `release` stops the
  plugin and releases what it holds, the watches it installed included, so a released consumer hears nothing
  afterwards.
- `all` reads the implementations of that capability as the plugin at that step.
- `watch` installs a watch as the plugin at that step, recording each `[providerId, event]` the core reports for
  it, `event` being `"provide"` or `"withdraw"`.

Answers `{ outcomes, heard, reported: [[plugin, id]], refusal, quarantined: [plugin], quarantineReason }`:

- `outcomes` holds one entry per `all` step, in step order: the provider ids of the implementations the core
  handed back, **in the order handed back**, which is the order the plugins' activations were verified (the
  `verify` step order), neither the order `provide` was called nor the order plugins were opened. An undeclared
  `all` answers `[]`.
- `heard` holds one entry per `watch` step, in step order, read after the last step: the `[providerId, event]`
  pairs that watch was told, in the order told. An undeclared or late `watch` is inert and answers `[]`. A
  `"withdraw"` names the provider whose implementation left.
- `reported` is read from the core's refusal events, one `[plugin, id]` pair per refusal reported, exactly as for
  `reach`.
- `refusal` is the first refusal of the run, null when there was none. An activation check that refuses is
  quarantine, not a refusal of the run, so a vector whose activation is refused and which asserts `refusal: null`
  is unambiguous.
- `quarantined` lists the plugins whose ledger row reads quarantined after the last step, in the order the vector
  names them, and `quarantineReason` is the first one's reason as its row shows it, a refusal as above, null when
  none is quarantined. The adapter quarantines a refused activation with no reason of its own, so the row shows the
  refusal the check answered, whose subject is the plugin's import name; the reason is read off the core's ledger,
  never off the step the adapter ran.

Invariant 14: an implementation joins `all` when its plugin's activation is verified, not when it is provided, and
leaves when the plugin is quarantined or released, so a consumer is never handed a provider the host is about to
refuse. A `watch` reports changes from its own installation onwards and never replays. `all` and `watch` answer
for an id the plugin lists under `capabilities.consumes` or `capabilities.provides` and nothing else; the refusal
is reported once per plugin and id, so an empty answer from an undeclared `all` is never mistaken for absence. The
declaration check comes before the lateness check: an undeclared call from a plugin that is no longer running is
refused and reported as undeclared, and only a declared one takes the late path, where `all` is let through and
`watch` is inert and not reported.

## What `then` can assert

A vector asserts decisions and never wording, so two implementations in two languages need not phrase a message
identically. Each key of `then` is compared against the same key of the answer:

| in `then` | holds when |
| --- | --- |
| `null` | the key is absent or null in the answer |
| `{"code": "x"}` | the answer's value is a refusal carrying the code `x` |
| `{"subject": x}` | the answer's value carries the subject `x`; a null `x` holds only when `subject` is answered and null |
| `{"hasFix": b}` | the answer's value answers `hasFix` as `b`, never inferred from a `fix` |
| `{"before": [["a", "b"]]}` | the answer is an array in which `a` precedes `b`, pairwise, because a valid topological order is not unique |
| `{"contains": [x]}` | the answer is an array containing each element |
| `{"equals": x}` | the answer equals `x` exactly |
| `{"absent": true}` | the key is absent or null |
| `{"present": true}` | the key is present and not null |
| anything else | deep equality |

A key absent from `then` asserts nothing, and a `then` with no key at all, or `present: false` or `absent: false`, is refused when the corpus loads.

## The drift checks

Five programs in `checks/`. Four take a repository root and the fifth a built core. They exit 0 when clean, 1 with
every offender listed, and 2 when they were given nothing to check (no files, no words, no modules, no vectors, no
symbols) or are misused.

```
weivel-purity <root> <role> <languages-file>
weivel-names <root> <words-file>
weivel-parity <root> <modules-file> <language> <languages-file>
weivel-layers <root> <layer-vector-directory> <language> <languages-file>
weivel-core-gate <rules-file> <msvc|gnu|apple> <tool> <shared-library> [<static-library>...]
weivel-core-gate --sources <rules-file> <core-root>
```

- Purity: the roles `c`, `core`, `kit` and `standard` allow C, headers, CMake, JSON and prose, and refuse a build file of any language in `checks/languages.txt`
  (`package.json`, `tsconfig.json`, `build.gradle`) wherever it sits. Any other role is a
  language named in `checks/languages.txt`, whose lines read `<language> <kind> <value>` with kind `extension`, `name`,
  `pattern`, `prefix`, `source-root` or `test-root`; it allows its own files and prose, and a generated `.js` under a Java
  repository is an offender.
- Names: no file may carry a word from the list, case-folded. `LICENSE`, `.weivel/repo.json`, the list itself, binary
  files and, at the root only, the directories `.git`, the hosting-service directory beside it, `build`, `build-*`,
  `siblings` and `node_modules` are not read. Purity and the core's source scan skip the same root directories, and
  nothing deeper is skipped by name. A file is binary when a NUL byte occurs in its first 8 KB, and each skipped file is printed.
  The list always comes from the pinned kit commit, never from the tree under test, so a repository cannot shorten the
  list it is checked with; a file whose trimmed, non-empty lines are the list word for word is that list's copy and
  is not read, which is how the kit's own `checks/names.txt` passes.
  A `package-lock.json` at any depth is read as JSON through the standard's reader, not as text, and only where it can
  name a dependency or its origin: the top-level `name`, every key of `packages`, each entry's `name`, `version` and
  `resolved`, and the keys and specs of its `dependencies`, `devDependencies`, `optionalDependencies`,
  `peerDependencies`, `peerDependenciesMeta` and `requires`, recursing into a legacy nested `dependencies` tree.
  `funding`, `license`, `integrity` and every other member name no dependency and are not read, so npm's own metadata
  does not trip the list. A lockfile that is not a JSON object is an offender, never passed unread.
- Parity: `MODULES` in the core lists the modules a language implements, one per line, with ` optional` after the
  ones a repository may omit. A language repository carries one top-level directory per module, named exactly as
  listed, under its `source-root`. A missing required module, a directory not on the list, a module directory that
  holds no file and a loose file directly under the source root are all offenders. A language `checks/languages.txt`
  does not list, or lists without a `source-root`, is refused rather than read as `src`, so a new language is added to
  the list before its repository is checked.
- Layers: every layer vector's id, its path under `vectors/layer` without `.json`
  (`consume/a-throwing-listener-does-not-silence-the-others`), must appear in some file under the language's `test-root`,
  standing on its own: a longer id or a file path that contains it does not count. A missing id is an offender. A
  language the list does not name, or names without a `test-root`, is refused, never read as the whole repository,
  because sources and fixtures outside the tests would then satisfy it. It runs for a language implementation, never
  for `core`, `standard` or `kit`, and only proves each vector is named; the language's own coverage test, which reads
  its describe names, stays the strong check.
- Core gate: reads the built core with the platform's own tool (`dumpbin` beside the MSVC compiler, `nm` elsewhere):
  the shared library's imports and exports, and, when given, each object of the static library built from the same
  sources. `checks/core_gate.txt` holds the rules, one `<kind> <reason> <symbol>` per line: `forbid` names what reads
  the world (file access, environment, clock, process, code loading, network), `forbid-name` a name no symbol may
  carry (`json`), and the `allow-*` kinds what the core may import: the C library everywhere, lock calls only in the
  object defining the symbol on the `mutex-object-defines` line, what the compiler emits, and what the toolchain's
  startup code draws into a shared library. Anything else is an offender. An import table cannot say which object
  calls a lock, so lock calls are attributed through the objects, and without them the gate says they were not.
  The MSVC startup code imports a clock and the environment (`QueryPerformanceCounter`, `_configure_narrow_argv` and
  others), so such an import passes only when the objects are given and none of them references it. The objects
  must be the ones the library was built from: every export must be defined by exactly one of them (a `startup-export`
  line names what the toolchain exports by itself), and an object with no symbol table, such as one compiled for
  link-time code generation, is refused.
- Core gate, `--sources`: an import table cannot see an intrinsic, an inline system call or inline assembly, so the
  same rules file also lists `forbid-source` names (`<reason> <word|call|prefix|text> <name>`) found anywhere under
  the core's `src` and `include`, and `source-in-mutex-only` lock names refused outside the `mutex-source` file.
- Under sanitizers: the job built with address and undefined behaviour sanitizers leaves out every test that reads
  the built core (`core_gate_*` and `core_reads_nothing_of_the_world`). Instrumentation makes the core import
  `__asan_*` and `__ubsan_*`, which the gate rightly refuses, and those tests say nothing about the runner. A
  repository copying the job copies the exclusion, and a new test that reads a built binary is added to it, or it
  fails under instrumentation.
