# Layer `description` vectors

## The `description` op in a layer

A `description` vector loads a C library whose definitions declare a description (`weivel/description.h` of the
standard), through the layer's host. Its op is named after its directory, as every layer op is; it is not the `library`
op with more steps because its library, its steps and its answers are all different: a `library` vector loads the
core's `echo` and calls it by its hand-written header, and a `description` vector loads the standard's `described` and
reaches it only through what its description says.

Its library is the standard's test library `described` (`test/described/described.h` of the standard), the target
`weivel_standard_described` of the standard the language pins. A suite builds it from its standard sibling as it builds
`echo` from its core: it configures `siblings/standard` as a project of its own, which is the only way the standard
adds its tests, with `-DWEIVEL_CORE_SOURCE_DIR` set to its core sibling, builds that one target, and loads
`weivel_standard_described.dll`, `libweivel_standard_described.so` or `libweivel_standard_described.dylib` from the
build's `bin` directory, under `bin/<configuration>` for a multi-configuration generator. A layer that adds the
standard to its own build may define the target itself from the same two sources and the same `weivel_describe` call,
as weivel/c does. The library has two definition services:

- `described-mirror`, the `described_mirror` definition: `reflect` answers a deep copy of a `described_sample`, a struct
  holding a member of every kind; `tree` answers a value tree built from a sample; `find` answers a text the library
  keeps for `alpha` and `omega`, NONE for any other key and a refusal for `refuse`; `measure`, `negate`, `halve`,
  `invert`, `swap_shade`, `grow` and `weigh` each take one input of one kind and answer one; `outline` answers a deep
  copy of a `described_node`, a struct pointing at its own kind through `next` and holding an array of its own kind
  in `content`, each copy's `depth` set to how many pointers lie between it and the root, and refuses a root labelled
  `refuse`; `count_releases` answers how many releases of each kind, of both services, have freed something since
  the binary was opened;
- `described-walker`, the `described_walker` definition: `open` answers a handle, `use` takes it as an input, and
  `visit` takes it inside a struct; `wait` answers the ticks it was asked to wait, at once, and is the one operation the
  description marks `blocking`; `lend` answers a `described_loan` holding a `counter`, a definition of its own whose
  `add` answers 1000 plus its `amount`, REFUSED for the amount 13, with the library's usual refusal, or MALFORMED for
  an `amount` that would pass the largest u64. The counter has a `release_refusal` of its own, which counts into the
  refusals `count_releases` reads. The counter is one static definition the walker's description names in its
  `definitions`, so an answer carries a definition.

Every refusal it answers carries the detail `described refused on purpose` and the fix `pass anything but "refuse"`.
The binary also exports three broken descriptions of the walker: `described_broken_count_description`, which the
standard's `weivel_standard_description_check` refuses, `described_broken_definition_description`, which the check
refuses at the loan's `counter` because the description holds no nested description for the definition it names, and
`described_broken_offset_description`, which the check passes while its `described_visit.skip` offset disagrees with the
struct's real layout, so only a layer that lays the struct out itself can see it. It exports
`described_mirror_without_halve_definition` too, a mirror holding NULL for `halve` and every other operation the
mirror's own, described by `described_mirror_without_halve_description`, written from the same struct as the mirror's.
Each description a service declares names the function answering its definition in `answered_by`, so the mirror's,
the walker's and the mirror without `halve`'s each describe their own definition and no other.

**Which layers witness what.** A layer that binds descriptions, building its language's view of a definition from
its description as `weivel/typescript` and `weivel/java` do, witnesses every vector here. A layer that binds nothing,
as C, whose plugins cast a definition to its header's struct, witnesses the vectors directly in `description/`, which
only load the library and get its services, and is exempt from `description/binding/` (the `layer-exempt` line of
`checks/languages.txt`). Every layer checks a description at the load, with the standard's check or its equal, then
checks with `weivel_standard_description_answers` that it answers the definition its service's symbol answered, and
refuses the load when either fails; a layer that binds also compares every struct's size and every member's offset, and
every operation's offset in the definition's struct, with its own FFI layout and refuses the load on any difference.
A check that answers OUT_OF_MEMORY fails the load as out of memory, never as the caller's mistake.

Its `given` is `{ declaration, holders, steps }`. `declaration` is the library's declaration, as the standard's
`test/described/library.json` and its three broken variants are, handed to the host as the standard reads it with the
path of the built `described` library beside it; `holders` names plugins the suite opens before the first step, each
consuming every service id the declaration lists, exactly as in a [`library` vector](library.md). The steps run in
order, each once the layer has nothing left to run from the one before:

- `{ load: true }` loads the declaration through the host's public load call;
- `{ holder, get: id }` gets the service as that holder, which then holds it;
- `{ holder, call: id, operation, with, keep }` calls the operation of that service through the object the holder
  resolved, the language's bound object, with the inputs `with` holds, and records what it answered. `id` is a service
  id, or the name a definition was kept under, which calls through the language's bound object for that definition.
  `keep` names the handle or the definition an operation answering one hands back, which the holder then holds under
  that name; a step calling such an operation always names one;
- `{ holder, release: name }` lets go of the handle kept under that name, in the language's own way of releasing a
  handle. For a definition kept under that name it ends the answer that carried it: the definition lives as long as
  that answer, so a call through the name afterwards is the caller's mistake, calling nothing;
- `{ holder, blocking: id }` reads, from the bound object the holder resolved for the service `id`, or from the
  definition kept under the name `id`, which operations the layer reports blocking, and keeps the operation names for
  `blocking`;
- `{ holder, readReleases: true }` calls the mirror's `count_releases` through the object the holder resolved and
  keeps its answer for `released`, not for `calls`;
- `{ unload: true }` unloads the library through the host's public unload call.

A holding outlives the unload until its holder lets go of it, as `library/a-binary-stays-open-while-a-holding-lives`
holds, so a call step after the unload is a plain call into a binary that is still open, through a definition its
holder was told was revoked and still holds; a layer that refuses such a call fails the vectors that make one.

**How a value is written.** Every input and every answer is a JSON value the suite translates into its language's
own value of the kind the description gives it, and back:

- `with` is an object keyed by the operation's input parameters' names. Neither the context, nor an answer, nor a
  refusal is written, and a SIZE parameter that counts another one is never written either: the binding sets it from
  what it counts. An input the step leaves out is passed as its kind's zero: 0, a NULL text, pointer or handle, or an
  empty array, bytes or texts with a count of 0.
- U32 is a JSON integer, an enum's value included, written as its number. F64 is a JSON number, and always finite:
  JSON has no infinity or NaN, so no vector hands or expects one.
- U64 and I64, and a SIZE that counts nothing, are decimal strings (`"9007199254740993"`, `"-9223372036854775808"`),
  never JSON numbers, because a JSON reader is free to read a number as a double, which rounds every integer beyond
  2^53. The suite reads one into its language's 64-bit integer. `grow` hands and answers a SIZE beyond 2^32, so it
  assumes a 64-bit `size_t`: a platform whose `size_t` is 32 bits is not a target of these vectors.
- TEXT is a JSON string, or null for NULL. TEXTS is an array of strings and nulls. BYTES is an array of integers from
  0 to 255. An ARRAY is an array of its elements, each written by its own kind. A NULL array, texts or bytes is
  answered as `[]`, its count being 0 either way.
- STRUCT, held or pointed at, is an object keyed by its members' names. Its leading `size`, the U32 member called
  `size` at offset 0 of a struct that carries one, is never written: the binding sets it to the struct's size as the
  description states it, in an input, and does not answer it. Nor is a SIZE member that counts another, a CONTEXT or a
  STORAGE. A member an input leaves out is its kind's zero. An answer writes every other member, but for a POINTER that
  is NULL and a VALUE of kind NONE, which are left out.
- POINTER is what it points at, written by the element's kind, and left out when it is NULL.
- VALUE, a `weivel_standard_value`, is the JSON value of the same kind: NULL is null, BOOLEAN true or false, NUMBER a
  number, TEXT a string, LIST an array and RECORD an object of its entries. NONE has no JSON value, so it is left out
  where it stands, a member or an answer.
- A RECORD's entries are compared as a JSON object is, by key and value, so their order is not witnessed: a layer
  keeps the provider's order, and no vector can tell it did.
- HANDLE is `{ "handle": name }`: as an input, the handle a step kept under that name; as an answer, the one the step
  keeps under the name its `keep` gives.
- A HANDLE member inside an answer struct is written the same way, and the handle it answers is kept under the step's
  `keep` as an answered handle is: a step whose answer holds one names `keep`, and an answer holds at most one.
- DEFINITION, a `const T *` whose `type` names a definition the description's `definitions` describe, is written
  `{ "definition": name }` as a member of an answer, the definition the step keeps under the name its `keep` gives,
  by the same rule as a handle: an answer holds at most one handle or definition. It is the language's bound object for
  the nested definition, built from the nested description as a service's is from its own, and callable through
  `{ holder, call: name }`. It is not an input of any vector: no operation of `described` takes one.

**What a layer that binds does beyond the values.**

- A name `with` holds that the operation does not take, a SIZE that counts another among them, or a name a struct
  holds that the caller does not write, is refused as the caller's mistake, and nothing is called.
- An operation the provider does not offer, its function pointer past the size the definition answered or NULL, is
  still on the bound object, and a call of it is the caller's mistake, calling nothing.
- Structs nest at most 1000 levels below the one a call takes or answers, `WEIVEL_STANDARD_VALUE_DEPTH_LIMIT` as for a
  value tree: a deeper input is the caller's mistake, a deeper answer the provider's fault. A struct may reach its own
  kind, as `described_node` does, so a layer lays such a struct out once and follows its pointers and arrays as deep
  as the data goes, never as deep as the description, which is finite. No vector reaches the limit: the kit's JSON
  reader nests no deeper than 256 levels.
- An operation answering several answers hands them back in its language's idiom: TypeScript an object keyed by each
  answer's name, Java `DescribedAnswer.Answered.answers()`, a map of each answer by its name.
- A language refuses the load of a description its own restrictions cannot bind: TypeScript an operation named `then`,
  which would make the bound object a thenable every `await` calls, and Java an operation of more than 7 parameters.

**What "reports blocking" means.** A layer that binds exposes, on the bound object of a definition, which of its
operations are blocking, read from the description's `blocking` names and from nothing else; a name the description does
not list as blocking is not reported, and a definition whose description lists none reports none. A vector cannot name
an event loop or a thread, so it witnesses only that the operation answers as any other does and that the fact is
reported; a layer whose callers must not wait proves in its own test that a call of a blocking operation leaves its
calling thread free.

After the last step the suite reads `loaded`, `callersMistake`, `got`, `sameObject` and `reported` as a
[`library` vector](library.md) defines them, the object `sameObject` reads being the language's bound object for a
described definition, which carries the address the definition's symbol answered as a registered definition does. It
also reads `calls`, one entry per call step in order, for what the operation answered:

- `{ "status": "ok", "answer": value }`, the answer parameter's value, `answer` left out for an answer of kind NONE;
- `{ "status": "refused", "detail": text, "fix": text }`, the refusal's two texts;
- `{ "status": "none" }` and `{ "status": "malformed" }`, for an operation that answered NONE or MALFORMED;
- `{ "callersMistake": true }`, for a call the layer refused as the caller's mistake before calling anything, in the
  language's own way of refusing one.

The status is the `weivel_status` the operation returned, lowercase without its prefix, read off the core's constants,
whichever way the language hands it to its caller.

A vector with `blocking` steps also reads `blocking`: one entry per `blocking` step in order, each the list of
operation names the layer reported for that service, in the description's order, `[]` for none.

A vector that reads releases also reads `released`: one entry per `readReleases` step after its first, each the
counts that step read minus the counts the first one read, per kind, as decimal strings
(`{ reflections, trees, refusals, cursors, nestedRefusals, outlines }`, `nestedRefusals` the refusals the lent
counter's own release freed, which no other release counts). The library keeps its counts for the life of its binary in the process,
so only a difference says what this run released; a suite that keeps the binary mapped between vectors reads the same
differences as one that maps it afresh.

What the counts can catch: each counts a release that freed something, so an answer the layer never releases reads one
fewer, a leak. A second release of a reflection, a tree or a refusal finds it empty, which the convention makes a no-op
that counts nothing, so releasing one of those twice is caught only by the sanitizers. A cursor's release counts every
call, so releasing a handle twice reads one more.

## `description/a-checked-description-registers-the-providers-own-pointer`

A definition whose description passes the check is registered as the pointer its symbol answered, in a layer that
binds as in one that does not.

The suite builds the vector's `given` through its layer and witnesses that the load succeeds, both gets are served
and each serves an object standing for the very address its definition's symbol answers, as `loaded`, `got` and
`sameObject` state, with nothing reported: a layer that checks the description and then registers a copy, or a
wrapper standing at another address, fails here.

## `description/a-broken-count-description-fails-the-load`

A description the standard's check refuses refuses the load, exactly as a NULL definition does.

The suite builds the vector's `given` through its layer, the walker declaring `described_broken_count_description`,
whose `described_visit.steps` is counted by a member it does not have, and witnesses that the load fails as the
caller's mistake, that neither service is registered afterwards, though the mirror's description is sound, and that
nothing is reported, as `loaded`, `callersMistake`, `got` and `reported` state. A layer that loads a description
without checking it, or that registers the mirror before it checks the walker and leaves it there, fails here.

## `description/a-definition-described-without-its-nested-description-fails-the-load`

A description naming a definition it does not describe refuses the load, as one the standard's check refuses does.

The suite builds the vector's `given` through its layer, the walker declaring `described_broken_definition_description`,
whose `described_loan.counter` is a DEFINITION naming `described_counter` while the description holds no nested
description for it, and witnesses what `description/a-broken-count-description-fails-the-load` does: the load refused
as the caller's mistake, neither service registered, nothing reported. A layer that loads such a description would meet
a definition in an answer it cannot bind, and it fails here, C included, since the check is every layer's.

## `description/a-description-answering-another-definition-fails-the-load`

A description refuses the load of a definition it does not name, even when the definition has the very struct it
describes.

The suite builds the vector's `given` through its layer, the mirror declaring `described_mirror_definition` under
`described_mirror_without_halve_description`, whose `answered_by` answers the mirror without `halve`, and witnesses what
`description/a-broken-count-description-fails-the-load` does: the load refused as the caller's mistake, neither service
registered, nothing reported. Both definitions are `described_mirror`, so the description passes the check and every
size and offset a layer compares; only the comparison of the two answers refuses it. A layer that loads it binds one
definition through another's description, and it fails here, C included, since the comparison is every layer's.

## `description/a-definition-declared-without-its-description-loads-as-before`

A definition whose service declares no description is loaded and registered as every definition was before
descriptions: as the provider's own pointer, opaque to a layer that binds, which builds nothing for it.

The suite builds the vector's `given` through its layer, the mirror declared alone and without its `description`, and
witnesses that the load succeeds and the get is served with the very address `described_mirror_definition` answers,
as `loaded`, `got` and `sameObject` state, with nothing reported. A layer that requires a description of every
definition fails here.

## `description/binding/a-broken-offset-description-fails-the-load`

A layer that binds refuses a description whose offsets disagree with its own layout of the structs it describes, even
when the standard's check passes it.

The suite builds the vector's `given` through its layer, the walker declaring `described_broken_offset_description`,
whose `skip` lies four bytes past where `described_visit` holds it, and witnesses what
`description/a-broken-count-description-fails-the-load` does: the load refused as the caller's mistake, nothing
registered, nothing reported. A layer that trusts a description's offsets fails here, and would otherwise read and
write every member after the one that moved at the wrong address.

## The `description/binding/` call vectors

Each of these calls one or two operations of one service through the language's bound object, with inputs written as
above, and witnesses `calls` exactly, every get served. Together they reach every member and parameter kind the
description has, as an input and as an answer:

- `description/binding/reflect-answers-every-member-kind`: a sample holding every member kind, a u64 beyond 2^53, the
  lowest i64, a NULL tag among texts, a NULL label in a struct held by value, a struct pointed at, an array of structs,
  bytes holding 0 and 255, a value tree holding every kind but NONE, a value pointed at and an array of values, answered
  back member for member; then an empty sample, answered with every member at its zero, no `best`, no `held` and no
  `extra`, which is how a layer's zeros and its leaving out of NULL and NONE are held;
- `description/binding/tree-answers-a-value-tree`: a VALUE answer, a record holding a text, a list with a null, a
  number and a copy of the sample's held value, then the record of an empty sample, whose held value is null;
- `description/binding/find-answers-a-text-the-provider-keeps`: a text input and a text answer nothing releases;
- `description/binding/measure-takes-bytes-and-their-length`: bytes and the SIZE counting them, which the binding sets,
  and a u64 answer, for four bytes and for none;
- `description/binding/negate-takes-and-answers-a-signed-64-bit-integer`: an i64 by value and as an answer, at the
  highest i64, and MALFORMED for the lowest, which the provider refuses;
- `description/binding/halve-takes-and-answers-a-double`: an f64 by value and as an answer;
- `description/binding/invert-takes-and-answers-a-32-bit-flag-word`: a u32 by value and as an answer, reaching its top
  bit;
- `description/binding/swap-shade-takes-and-answers-an-enum-value`: a u32 bound to an enum by value and as an
  answer, written as its number, and MALFORMED for a number the enum does not hold;
- `description/binding/grow-takes-and-answers-a-size`: a SIZE that counts nothing, by value beyond 2^53 and as an
  answer;
- `description/binding/weigh-takes-a-value-by-pointer`: a value handed by pointer, a list holding a record, and a
  text;
- `description/binding/open-answers-a-handle-and-use-takes-it`: a handle answered, kept, handed back as an input and
  released by its holder;
- `description/binding/visit-takes-a-handle-inside-a-struct`: a handle as a struct's member beside a u64 and an array
  of u64, then the same struct with only its handle written;
- `description/binding/outline-copies-nodes-holding-nodes-both-ways`: a struct reaching its own kind, as an input and
  inside an answer: a node holding two nodes, the first holding one that holds one, three levels of `content` below
  the root, and a chain of two through `next`, answered back with every `depth` the provider set, the one handed in
  ignored, then a lone node; `released` reads two outlines. A layer that lays a struct out by following its members
  into themselves never finishes binding the mirror, one that marshals a struct to a fixed depth loses the deepest
  node, and one that leaks the answer reads fewer outlines.

## `description/binding/a-refusal-arrives-with-its-detail-and-fix`

A refused operation reaches its caller as a refusal carrying the provider's own texts.

The suite builds the vector's `given` through its layer, calls `find` with `refuse`, `reflect` with a sample labelled
`refuse` and `open` with `refuse`, and witnesses `calls` exactly: three refusals, each with the detail and the fix
the library answers, and no handle kept. A layer that hands the binding no refusal, so the provider answers REFUSED
with no texts, or that answers its own wording, fails here.

## `description/binding/a-none-answer-arrives-as-none`

An operation that answers NONE reaches its caller as none, neither an answer nor a refusal.

The suite builds the vector's `given` through its layer, calls `find` with `beta` and with an empty key, and
witnesses `calls` exactly: none, twice.

## `description/binding/a-missing-required-input-answers-malformed`

A required input the caller leaves out is passed as NULL, and the provider's MALFORMED reaches the caller as such.

The suite builds the vector's `given` through its layer, calls `find` with no key, `reflect` with no sample and
`visit` with no visit, and witnesses `calls` exactly: malformed, three times. A layer that fills a missing input with
a value of its own making, so the provider answers something else, fails here.

## `description/binding/an-input-the-operation-does-not-take-is-the-callers-mistake`

A name the caller writes that the operation or its struct does not take is refused before anything is called.

The suite builds the vector's `given` through its layer, the mirror declared alone, and calls `find` with a `keys`
beside its `key`, `measure` with the `length` the binding sets itself, and `reflect` with a sample holding a `colour`
no `described_sample` has, then `find` with its `key` alone, and witnesses `calls` exactly: the caller's mistake three
times, then the first letter. A layer that drops a name it does not know, so a misspelt input reaches the provider at
its zero, fails here.

## `description/binding/an-operation-its-provider-does-not-offer-is-the-callers-mistake`

An operation the description lists and the provider holds NULL for is the caller's mistake to call, and every other
operation of the same definition still answers.

The suite builds the vector's `given` through its layer, the mirror declared by
`described_mirror_without_halve_definition` under its own description, `described_mirror_without_halve_description`,
which lists `halve` as the mirror's does, calls `halve` and then `negate`, and
witnesses `calls` exactly: the caller's mistake, then `-5`. A layer that calls through a NULL function pointer, or
refuses the load of a definition offering less than its description lists, fails here.

## `description/binding/every-answer-is-released-exactly-once`

A layer releases every answer it copies back to its caller exactly once, through the release the description names
for it: a filled answer after it is copied, a refusal after its texts are, a handle when its holder lets go of it, and
nothing for an answer the description names no release for.

The suite builds the vector's `given` through its layer and calls, in order, `reflect` and `tree`, each answering
storage; `find` and `open`, each refused; `find` answering a text the library keeps; `open` answering a handle, which
the holder releases; then reads releases, unloads, and reads them again through the mirror it still holds, having
read them once before its first call. It witnesses `calls` exactly, and `released` exactly: both differences read one
reflection, one tree, two refusals and one handle. A layer that leaks an answer reads one fewer of its kind, and one
that releases the handle again at the unload, after its holder released it, reads two handles.

## `description/binding/a-handle-the-plugin-never-released-is-released-at-the-unload`

A handle its holder never lets go of is released by the layer at the unload, through the release the description
names for it, while its binary is still open.

The suite builds the vector's `given` through its layer, reads releases, opens a handle and uses it, reads releases
again, unloads, and reads them a third time through the mirror it still holds. It witnesses `calls` exactly, and
`released` exactly: no handle released before the unload and one after it. A layer that leaves a held handle to the
language's collector, or releases it after the binary closed, fails here.

## The blocking and nested-definition vectors

Three vectors of `description/binding/` witness what a description's `blocking` and `definitions` mean to a layer that
binds:

- `description/binding/a-blocking-operation-answers-as-any-does-and-is-reported-blocking`: the walker's `wait` called
  with `9007199254740993` ticks answers them, as `calls`, so a layer that runs a blocking operation by another path
  still answers it as a direct one does; then `blocking` is read for the walker and for the mirror and witnessed
  exactly: `["wait"]`, then `[]`. A layer that reports every operation, or none, or one name more than the description
  lists, fails here;
- `description/binding/an-answer-carrying-a-definition-is-callable-until-released`: `lend` answers the loan with its
  counter kept, whose reported `blocking` is read and witnessed as `[]`; `add` through it answers `1005`, then the
  largest u64 for an `amount` that reaches it exactly, then MALFORMED for one more; releasing the counter and calling it
  again is the caller's mistake. A layer that hands back a definition it cannot call, one whose `add` reads the wrong
  context, one that reports the walker's blocking operations for it, or one still callable after its answer ended
  fails here;
- `description/binding/a-nested-definitions-answers-are-released-exactly-once`: the counter refuses the amount 13 with a
  refusal only its own `release_refusal`, handed its own context, frees and counts as a nested refusal, so the suite
  reads releases, calls `lend`, `add` with 5 and `add` with 13, releases the counter, reads releases, unloads and reads
  them again, and witnesses `released` exactly: one nested refusal both times and nothing else. A layer that never
  releases the nested refusal reads none, and one that releases it through the walker's or the mirror's release, or
  with the walker's context, reads none as well, since those free nothing of the counter's. A refusal released twice is
  not caught here, only by a sanitizer.
