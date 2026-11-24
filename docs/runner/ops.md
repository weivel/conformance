# What each op is given, and what it answers

Every shape below is a contract. An adapter writes the keys it can and omits the rest; a key the vector does not
assert is ignored, and a key the vector asserts and the adapter omits fails the vector.

A `refusal` in an answer is `{ code, detail, fix, subject, hasFix }` or null. `code` is the core's refusal code in
lowercase without its prefix, so `WEIVEL_REFUSAL_UNDECLARED` is `undeclared`. `subject` is what the refusal is about
as the core's header says for that code (the id reached for, the need nothing meets, the constraint missed, the
import name), null when the code names nothing, and `hasFix` is true when the fix is non-empty. Both are read off the
core's own `subject` and `has_fix`, never out of the wording. A vector asserts the code, the subject and hasFix,
and never the wording.

## `supports`

Given `{ host: { applications, api }, facts }`, the manifest facts as the core holds them.
Answers `{ supported, refusal, axis: "application" | "api" | null }`. `axis` names which check refused first,
because a refusal that cites the api floor when the artifact was built for another application sends the author to
the wrong line.

## `boot`

Given `{ units: [{ id, provides, requires: [{ id, optional, degradable }] }] }`.
Answers `{ started: [id], refusal }`.
**`optional` and `degradable` are different axes.** `optional` decides whether an absence is tolerated;
`degradable` decides what a present-but-unsatisfying provider costs. A requirement that is `degradable` and not
`optional` still refuses when it is absent.

## `order`

Given `{ edgeSource: "capabilities" | "services", hostProvided?: [id], units: [{ id, provides: [{ id, kind }],
requires: [{ id, kind, optional }] }] }`.
Answers `{ order: [id], edges: [[provider, consumer]], cycle: [id] | null, refusal }`, `refusal` being the plan's
first refusal or null.
`edgeSource` is a parameter and not a convention: an engine that draws capability edges while asked for service
edges is answering a question nobody put to it. A cycle is reported with its members rather than thrown, and the
cycle's members are not in `order`, while every other unit the plan does not refuse still is. `cycle` names the
plan's first cycle only. How the core builds the plan is the core's `docs/plan.md`. `hostProvided`, when present,
lists service ids the host itself supplies: they meet a mandatory need exactly as a unit's offer would, and the host
never appears in `order`.

## `resolve`

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

## `revoke`

Given `{ service: { id }, steps }`, a script of `register`, `lookup`, `revoke` and `use`.
One plugin registers `service.id`, as a well-known id, and another declares it and looks it up; `revoke` ends the
registration and `use` uses the handle the last lookup returned. The handle is unusable once the core has revoked
it from the consumer, and usable only while the core still hands back that same registration.
Answers `{ lookups: [boolean], used, reason }`. `reason` is a token such as `revoked`, never a sentence: a
consumer holding a revoked handle has to learn what happened, and the kit must not pin how it is phrased.

## `validate`

Given `{ schema, manifest, wellKnownServices? }`, where `schema` names the published vocabulary's file. The
manifest names itself with `importName`; a repository's identity is supplied from outside it.
**An adapter validates against the published schema and never against a restatement of it**, and alongside it
applies the rules the standard's `docs/validation.md` states. `wellKnownServices` is the host's list for rule V2; absent means V2 is not applied,
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

## `identity`

Given `{ value }`, a string offered as a repository identity. Answers `{ valid }`, true when the whole string
matches `^(local:[^/\s]+|[a-z][a-z0-9-]*(@[a-z0-9.-]+)?:[0-9]+)(/(?!\.{1,2}(/|(?![\s\S])))[a-z0-9._-]+)*(?![\s\S])`:
`local:` followed by a name, or a forge (with an optional `@host`) and a numeric repository id, then any number of
path segments. A bare name, a non-numeric or empty id, an uppercase forge and a `.` or `..` segment are all
invalid; a segment that only starts with or contains dots, such as `.hidden` or `a..b`, is valid.

## `library`

Given `{ declaration }`, a C library's declaration. The adapter re-serialises it, as `validate` does its manifest, and
hands it to the standard's reader. Answers `{ read, release, services, path, refusal }`:

- `read` is true when the reader accepted the declaration. `release` and `services` are answered only then:
  `release` is the declared release symbol, null when the declaration names none, and `services` lists
  `[{ id, functions: [{ name, symbol, kind }], description? }]` in the declaration's order, `kind` being `"bytes"` or
  `"definition"`, read off the standard's constants by value, or the number for one the kit does not name, and
  `description` the symbol the service declares for its definition's description, answered only when it declares one.
- `path` is the refused field as the reader names it (`services[0].functions[1]`, `release`), null when read, and
  `refusal` is the standard's refusal as for `validate`, or null.

A service's `id` is lowercase words joined by single hyphens or dots, so a library can serve a standard id such as
`weivel.host`; an empty word, as in `weivel..host`, `.a` or `a.`, is refused at the `id` as `invalid_value`. The
`importName` follows the same rule and, as a plugin's does, refuses a Windows device name with or without a dot after
it (`con`, `nul`, `com1.x`), at the `importName` as `invalid_value`.

A definition is its service's only function, and only a library declaring a bytes function needs a release: the
reader refuses a definition beside another function at the function that brings the two together, as
`conflicting_field`, a `context` on a definition at that `context`, as `conflicting_field` too, since a definition
carries its context inside its own struct, and a declaration with a bytes function and no release at `release`, as
`missing_field`. A third kind is refused at its `kind`, so naming a second kind opened no others. A `description` is a
C identifier, refused at it as `wrong_type` when it is not a string and `invalid_value` when it is empty, and only a
definition is described, so one beside a bytes function is refused there as `conflicting_field`.

The same op under `vectors/layer/library/` loads a declaration through a language's host, and is witnessed as
[the layer `library` vectors](../vectors/layer/library.md) say. A declaration whose services declare their
`description` is loaded through a language's host by the layer op `description`, under `vectors/layer/description/`,
on the standard's `described` test library: every layer checks its descriptions at the load, and a layer that binds
descriptions also calls it through what they say, as [the layer `description` vectors](../vectors/layer/description.md)
state. A description may mark operations `blocking` and describe the
definitions its answers carry, and those vectors read two facts beyond a call's answer: the operations a layer reports
blocking, and a nested definition called until the answer that carried it is released.

## `unbound`

Given `{ manifest }`, a manifest whose `services.consumes` holds ids in the string form or the object form
`{ id, optional?, provider? }`. Answers `{ services: [id] }`, the consumed ids the core cannot tie to a provider
and so must report: a namespaced id (`calendar:events`) with no `provider`. An id bound by a `provider` is
omitted, and so is a bare id, because a bare id names a host or shared service and has no plugin to bind to. The
list keeps the manifest's order.

## `carry`

Given `{ manifest }`. Answers `{ sameObject }`. The adapter builds the core's manifest facts with `manifest` as
their payload, opens a plugin session from those facts, and answers whether the payload the session hands back is
the very object it passed in, not an equal copy. Invariant 4: the core carries the host's manifest through
uninspected, which is what lets one model serve hosts whose manifest types are unrelated.

## `want`

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

## `scope`

Given `{ steps }`, run in order, each one of `{ consumer, register: id }`, `{ consumer, want: id }`,
`{ release: consumer }` and `{ lookup: id }`.
Answers `{ lookups: [boolean], wants: ["settled" | "pending" | "rejected"] }`: one entry per `lookup` step, true
when the id resolves at that step, and one per `want` step, read after the last step. A `lookup` is made as the
host, and each consumer's manifest declares exactly the ids its own steps register and want, so the reach fence,
which `reach` asks about, never decides a scope step. Invariant 11: a consumer's
view of the registry is bound to it, so releasing one consumer removes exactly what it registered and rejects
exactly what it still waits for.

## `reach`

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

## `consume`

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
