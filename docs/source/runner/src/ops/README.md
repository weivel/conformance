The adapters: one C function per op the runner answers, each given a vector's `given` and writing the answer [the op's contract](../../../../runner/ops.md) states. `ops.c` is the table of every op the kit knows, in report order; `lifecycle`, `description` and `host` sit in it with no adapter, so their vectors load and are counted while only a language's layer answers them. The core's ops drive a real core as a host would: `facts.c` builds the core's values from the vector's JSON, `cast.c` registers and opens each plugin the vector names, `session.c` creates the core and drains its event queue after every call, and `hearing.c` keeps every event the core queued. An adapter answers from that event log and from the core's returns, never from what it believes it asked for: how a want ended is read from its settlement event, which provider an implementation stands for from the handle the core handed back, and what was refused from the core's refusal events, never from diagnostic text. The core is handed time as numbers by the adapter, which never sleeps, so every `...Millis` in a vector is virtual. The standard's ops call the standard in-process: `identity` hands it the string as given, and `validate` and `library` re-serialise the vector's JSON through `json_text.c` and hand the text to the standard's readers, answering nothing for a value they cannot write back exactly. Refusal and kind names are tables keyed on the headers' constants, never on position, so a code appended in the core or the standard cannot shift a name. Every array an adapter builds for one call lives in an `arena`, released at once.

<!-- generated: source-children -->
- [adapters.h](../../../../../runner/src/ops/adapters.h) - every adapter's declaration, and the function-kind names the `library` answer uses
- [arena.c](../../../../../runner/src/ops/arena.c) - zeroed blocks owned by one arena, freed together
- [arena.h](../../../../../runner/src/ops/arena.h) - `arena`, and why one failed take fails every later one
- [carry.c](../../../../../runner/src/ops/carry.c) - `carry`: whether the payload a session hands back is the very object the host passed in
- [cast.c](../../../../../runner/src/ops/cast.c) - opening the plugins a vector names, finding one by name or handle, the provider and consumer pair, and the shared answer writers
- [cast.h](../../../../../runner/src/ops/cast.h) - `cast`, `session_open_pair` and the helpers the step-driven adapters share
- [consume.c](../../../../../runner/src/ops/consume.c) - `consume`: capabilities provided, verified, listed, watched, quarantined and released, each answer read off the core's implementations and ledger
- [facts.c](../../../../../runner/src/ops/facts.c) - building manifests, string and number lists, choices, and service wants and offers from JSON
- [facts.h](../../../../../runner/src/ops/facts.h) - the fact builders, and what an absent member defaults to
- [hearing.c](../../../../../runner/src/ops/hearing.c) - recording each drained event, the run's first refusal, and the lookups of a settlement or a reported reach refusal
- [hearing.h](../../../../../runner/src/ops/hearing.h) - `heard_event`, `hearing`, and why a refused activation check never becomes the run's first refusal
- [identity.c](../../../../../runner/src/ops/identity.c) - `identity`: the standard's parse of a repository identity
- [json_text.c](../../../../../runner/src/ops/json_text.c) - writing a JSON value back out as text, exactly or not at all
- [json_text.h](../../../../../runner/src/ops/json_text.h) - `json_text_write` and what it refuses to reproduce
- [library.c](../../../../../runner/src/ops/library.c) - `library`: the standard's reading of a C library's declaration, service by service, or the refused path
- [ops.c](../../../../../runner/src/ops/ops.c) - the op table, including the three layer-only ops with no adapter
- [ops.h](../../../../../runner/src/ops/ops.h) - `op_adapter`, the contract every adapter keeps, and the table lookups
- [plan.c](../../../../../runner/src/ops/plan.c) - `boot` and `order`: each unit registered as a manifest, and the core's activation plan, its edges and its cycle
- [reach.c](../../../../../runner/src/ops/reach.c) - `reach`: plugin and host steps through the reach fence, outcomes, served objects and reported refusals
- [resolve.c](../../../../../runner/src/ops/resolve.c) - `resolve`: a scripted provider polled under the host's vocabulary until the want settles, degrades or runs out of budget
- [revoke.c](../../../../../runner/src/ops/revoke.c) - `revoke`: register, lookup, revoke and use of one service between a provider and a consumer
- [scope.c](../../../../../runner/src/ops/scope.c) - `scope`: consumers registering, wanting and being released, each declaring exactly what its steps use
- [session.c](../../../../../runner/src/ops/session.c) - creating and draining a core, reading a handle's state, and the core's refusal names
- [session.h](../../../../../runner/src/ops/session.h) - `core_session`, why only waiting and watching are read off a handle, and the refusal answer
- [standard_refusal.c](../../../../../runner/src/ops/standard_refusal.c) - the standard's refusal names, and the refusal answer that refuses one missing its detail or fix
- [standard_refusal.h](../../../../../runner/src/ops/standard_refusal.h) - the standard refusal helpers
- [supports.c](../../../../../runner/src/ops/supports.c) - `supports`: the core's compatibility check of a host and a manifest, and the axis that refused first
- [unbound.c](../../../../../runner/src/ops/unbound.c) - `unbound`: the consumed ids the core cannot tie to a provider
- [validate.c](../../../../../runner/src/ops/validate.c) - `validate`: a manifest against the published schema and the standard's rules, with the well-known list when the vector gives one
- [want.c](../../../../../runner/src/ops/want.c) - `want`: a timed or untimed want on the adapter's clock, the deadline handed to the core before a registration due at the same moment
- [want_states.c](../../../../../runner/src/ops/want_states.c) - reading a want's settlement as `settled`, `rejected`, `served` or `refused`
- [want_states.h](../../../../../runner/src/ops/want_states.h) - why only the code the contract means reads as a rejection
<!-- end generated -->
