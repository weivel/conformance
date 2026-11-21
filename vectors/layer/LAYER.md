# Layer vectors

A vector here states something only a language layer can witness, because the core never calls a listener,
never throws and never runs host code. The runner loads these vectors and counts them, but no adapter in the
runner runs them: each language's own layer suite must contain a test that witnesses every vector in this
directory, naming it by id, its path here without `.json`. The kit's `layers` drift check fails a language whose tests
name one nowhere.

The one exception is a directory of vectors that ask for something a language's layer does not do by design:
`checks/languages.txt` names it for that language with a `layer-exempt` line, and `layers` then asks for none of its
vectors and refuses the line once no vector lies under it. Today there are two: `description/binding/`, which C is
exempt from because a C plugin reads a definition through its header and binds nothing (see the `description` op
below), and `host/`, which C and Java are exempt from because only a language that ships a host program sits it, and
TypeScript is the one that does (see the `host` op below).

## `consume/a-throwing-listener-does-not-silence-the-others`

A layer delivers a watch's changes to listeners it holds. A listener that throws on every call must not
stop the others hearing, and the throw must not be reported as a refusal.

The suite builds the plugins and steps of the vector's `given` through its layer, installs the listener of
the throwing watch so that it throws on each call after recording what it was handed, and then witnesses:

- every other listener still hears each change, in the order delivered, exactly as `heard` states;
- the throwing listener's own entry still records what it was handed before it threw;
- nothing is added to `reported`, and the first refusal of the run is none.

## `consume/a-refused-verify-quarantines-the-plugin`

A plugin whose activation check refuses is quarantined by the host before any event of the check is dispatched and
before the host starts another plugin.
The plugin never asks for it and no `quarantine` step names it: a layer whose host only reports the refused
check leaves a plugin running that the core will not verify.

The suite builds the plugins and steps of the vector's `given` through its layer, with no quarantine of its own
making, and then witnesses:

- the refused plugin's own watch stops at its quarantine, so it hears the provider joining and not the provider
  leaving afterwards, while the other consumer hears both, exactly as `heard` states;
- the plugins the layer's ledger shows quarantined after the last step are exactly `quarantined` (`flaky` alone),
  and the first one's reason, as its ledger row shows it, carries what `quarantineReason` states: the code
  `activation_mismatch`, the subject `flaky` (the plugin's import name) and a fix;
- the refused check is not a refusal of the run: nothing is added to `reported`, and the first refusal is none.

## The `want` op's clock and scheduler

A `want` vector runs on a clock and a scheduler the suite owns and hands the layer in place of the language's own. The
scheduler runs a task the layer gave it only when the suite moves time forward over the task's due time, at that
time, and never on its own. `fired` lists, in order, the clock's time each time the suite's scheduler actually ran a
callback the layer had scheduled and not cancelled: it witnesses what the timer did, where `timer` witnesses only what
the layer said it did. With `schedulerRunsBeforeRegistering` false, the suite moves its clock to `registerAtMillis`
without running any task its scheduler holds, so no timer of the layer's fires before the registration; absent, the
scheduler runs every task due on the way.

## `want/a-registration-at-the-deadline-loses`

A want whose deadline and a provider's registration fall at the same moment times out: the deadline is handed to
the core before the registration is made, so the registration settles nothing. A layer that runs the want on its
own clock must keep that order, because an event loop that delivers the registration first serves a want the
core has already timed out.

The suite builds the provider and the consumer through its layer, asks with the vector's timeout on a clock it
controls, registers the provider at `registerAtMillis`, and then witnesses:

- the want ends timed out at the deadline, exactly as `timedOut` and `atMillis` state, and is never served;
- the want's refusal is `timed_out`, its subject the service id it waited for (`accounts`), with a fix.

The suite's scheduler runs the task due on the way, so two things hand the core the deadline before the
registration: the timer firing at it, and the wrapper's check before the registration. Either alone keeps the order,
so the vector fails only a layer in which both are broken. The timer alone is witnessed by
`want/a-timed-want-arms-the-timer-at-its-deadline` and the check alone by
`want/a-registration-past-an-unfired-deadline-loses`; that each holds when the two fall due at the same moment is
held by review.

## `want/a-timed-want-arms-the-timer-at-its-deadline`

A layer arms its timer from the core's `WEIVEL_EVENT_DEADLINE` alone: the latest one drained is the deadline to arm,
and one that announces none disarms. It keeps no list of the calls that move a deadline, and it never advances the
core on a timer of its own reckoning.

The suite builds the consumer through its layer, asks with the vector's timeout on a clock it controls, lets nothing
register, runs the clock to `horizonMillis` without calling the core's advance itself, and records each move the
layer makes to its timer as `[at, deadline]`: the clock's time when the layer moved it, and the deadline it armed, or
null for a disarm. It then witnesses:

- `timer` exactly: armed at 1000 when the want was made at 0, and disarmed at 1000, once the want had timed out and
  the core announced that no deadline is left;
- the want ends timed out at 1000, its refusal `timed_out` with the subject `accounts` and a fix, because the timer
  firing at the announced deadline is what handed the core that time;
- `fired` exactly: the scheduler ran the layer's callback once, at 1000.

## `want/a-settled-want-disarms-the-timer`

A want that settles before its deadline takes the deadline with it, and the layer disarms on the event that says so.

The suite builds the provider and the consumer through its layer, asks with the vector's timeout on a clock it
controls, registers the provider at `registerAtMillis`, runs the clock to `horizonMillis`, records the timer's moves
as above, and witnesses:

- `timer` exactly: armed at 1000 when the want was made at 0, and disarmed at 500, when the registration settled the
  want, so the timer never fires;
- the want is served at 500 and never times out, with no refusal;
- `fired` exactly empty: the scheduler never ran a callback of the layer's, so a timer that reports the disarm but
  leaves its callback scheduled fails here.

## `want/a-registration-past-an-unfired-deadline-loses`

A layer hands the core a deadline that has fallen due before it makes any other call, whether or not its timer has
fired. The clock can pass the deadline before the scheduler takes its turn, and a registration made then must not
serve a want the core would have timed out: the wrapper reads its clock before the call and advances the core first.
The vector binds a layer only in a suite whose scheduler runs no task before the registration: in any other harness,
one that fires due tasks on the way among them, the timer hands the core the time and the wrapper's check is never
witnessed, so there the rule is held by review.

The suite builds the provider and the consumer through its layer, asks with the vector's timeout on a clock it
controls, moves the clock to `registerAtMillis` without running a task (`schedulerRunsBeforeRegistering` false),
registers the provider, runs the clock to `horizonMillis`, records the timer's moves as above, and witnesses:

- the want ends timed out when the registration is made, at 1500, never served, its refusal `timed_out` with the
  subject `accounts` and a fix;
- `timer` exactly: armed at 1000 when the want was made at 0, and disarmed at 1500 by the advance the layer made
  before the registration;
- `fired` exactly empty: the timer never fired, so only the layer's own check before the call handed the core the
  time.

## The `library` op in a layer

A `library` vector here loads a C library through the layer's host, which only a layer can do, because the core
never opens a binary. Its library is the core's test library, `echo`, built beside the core (`test/echo/echo.h`):

- `echo_service_definition` and `echo_other_service_definition` each answer an `echo_definition` of their own, whose
  `echo` repeats its input when handed the definition's own `context` and whose `count_unloads` hands the binary a
  counter it adds one to when it is closed;
- `echo_floor_definition` answers an `echo_floor`, a struct whose `size` is exactly `2 * sizeof(void *)`, the
  smallest a host loads;
- `echo_null_definition` answers NULL, and `echo_one_byte_short_definition` a struct whose `size` is
  `2 * sizeof(void *) - 1`, one byte below the floor of the platform it was built for;
- `echo_echo` is a bytes function that answers a copy of its input, freed by `echo_release`.

Its `given` is `{ declaration, owners, holders, steps }`, `declaration` and `owners` each optional.
`declaration` is the library's declaration, handed to the host as the standard reads it or as the suite builds it,
with the path of the built echo library beside it. `owners` is `[{ id, provides: [id] }]`, plugins the suite opens on
the host before the first step that each register their own services, `provides` going into the manifest under
`services` exactly as written. `holders` names plugins the suite opens on the host before the first step, after the
owners, through the capabilities edge source, each consuming every service id the declaration lists and every id an
owner provides, so neither the activation plan (the library is not loaded yet) nor the reach fence decides a step.
The steps run in order, and each runs only once the layer has nothing left to
run from the one before, as an `activate` step does in a `lifecycle` vector, so every event a step queued has been
dispatched when the next one starts:

- `{ load: true }` loads the declaration through the host's public load call;
- `{ holder, get: id }` gets the service as that holder, which then holds it;
- `{ holder, echo: text }` calls the definition's `echo` through the object that holder resolved, with its `context`
  and the text's UTF-8 bytes, and releases the answer through the definition's own `release`;
- `{ holder, call: id, function, text, keep: true }` calls that bytes function of the service through the object that
  holder resolved, with the text's UTF-8 bytes, and keeps the answer the layer handed back unreleased;
- `{ holder, drop: id }` lets go of the answer that holder kept from that service, in the language's own way, so the
  layer releases it through the library's release symbol;
- `{ holder, countUnloads: true }` hands the binary, through the definition that holder resolved, a counter the suite
  owns that outlives the binary;
- `{ unload: true }` unloads the library through the host's public unload call;
- `{ holder, releaseHolding: id }` lets go of that holder's revoked holding, in the language's own way of saying a
  holder no longer uses an object, so the layer ends it with `weivel_release_holding`;
- `{ owner, registerAwaitingReleases: id }` registers that service as that owner, through its context, as a
  registration that awaits releases (`weivel_register_service_awaiting_releases`), with a listener that records each
  `WEIVEL_EVENT_HOLDINGS_RELEASED` the layer hands the owner for it;
- `{ releaseHolder: plugin }` releases that holder or owner through the host, and `{ quarantine: plugin }`
  quarantines it, each ending whatever the plugin still holds without a `releaseHolding`;
- `{ forgetHolder: plugin }` forgets that plugin, released before, through the host, so the core revokes and ends
  whatever it came to hold through late calls. A host whose release forgets the plugin as its last step leaves a late
  call only one window, while the released plugin hears the events its own release queued, so the steps a vector
  places between that plugin's `releaseHolder` and its `forgetHolder` are made from that plugin's listener on the
  first such event, and `forgetHolder` then checks that the host no longer holds it;
- `{ readUnloads: true }` reads the counter.

A vector that reads the counter is judged only while nothing in the process but the layer holds the echo binary
open: the suite, and every other test the process runs, keeps no handle of its own on it across the vector's steps.
A suite that opens the binary itself, to read the address a definition's symbol answers for `sameObject`, does so
right after the layer's load, while the layer holds the binary, and closes it again before the next step: opened
before the layer's load and closed, the binary may map at another address and a correct layer reads false, and a
handle kept across steps would keep the binary open after the layer closed it, so the counter would never move.

After the last step, once the layer has nothing left to run, the suite reads:

- `loaded`: whether the load answered success;
- `callersMistake`: whether the load was refused as the caller's mistake, the language's answer to a misused call
  (`WEIVEL_C_STATUS_MISUSE` in C, the thrown misuse error elsewhere), and not a refusal of the core's;
- `got`: one boolean per `get` step, true when it was served;
- `sameObject`: whether every object a `get` served stands for the very address the definition's symbol answered:
  the pointer itself in C, and an object carrying that native address elsewhere;
- `echoed`: the text each `echo` step answered;
- `revoked`: each `[holder, id]` revocation the layer handed a holder, sorted;
- `unloads`: the counter at each `readUnloads` step;
- `holdingsReleased`: each `[owner, id]` release event an owner's listener heard, in the order heard;
- `reported`: every line the layer handed its error sink or its log from the vector's first step on, the core's own
  log events among them, since the layer writes those to the host's log; what the suite's setup of the holders
  produced does not count. A caller's mistake reaches neither.

## `library/a-definition-is-the-providers-own-pointer`

A host registers a definition as the pointer its symbol answered, with no wrapper, count or copy, so a consumer's call
through it is a plain native call.

The suite builds the vector's `given` through its layer and witnesses:

- the load succeeds, and `agenda`'s get is served with the very address `echo_service_definition` answers, exactly as
  `sameObject` states: a host that copied the struct, so that what `agenda` reads lives at another native address,
  fails here, while an object of the language's own that carries the provider's own address passes, as the contract
  says a host outside C registers one;
- calling the definition's `echo` through what `agenda` resolved, with its own `context`, answers `ping`, as `echoed`
  states, so the context the consumer reads is the library's own.

## `library/a-null-definition-fails-the-load`

A definition whose symbol answers NULL refuses the load as the caller's mistake, undone as a load that fails partway.

The suite builds the vector's `given` through its layer, a bytes service declared before the definition, and
witnesses:

- the load fails, refused as the caller's mistake, as `loaded` and `callersMistake` state;
- neither the bytes service nor the definition is registered afterwards: both of `agenda`'s gets miss, as `got`
  states, so a host that registered `echo-bytes` before it called the definition and left it there fails here;
- nothing reaches the layer's error sink or log, as `reported` states.

## `library/a-definition-below-its-size-fails-the-load`

A definition whose struct's `size` is below `2 * sizeof(void *)`, the `size` and one pointer under the platform's
alignment, refuses the load exactly as a NULL definition does.

The suite builds the vector's `given` through its layer and witnesses what `library/a-null-definition-fails-the-load`
does, with `echo_one_byte_short_definition`: the load refused as the caller's mistake, nothing registered, nothing
reported. Its `size` is one byte below the floor, so a host that compares against 8, the floor of a 32-bit target
only, loads it on a 64-bit one and fails here.

## `library/a-definition-at-its-size-loads`

A definition whose struct's `size` is exactly `2 * sizeof(void *)` is the smallest a host loads.

The suite builds the vector's `given` through its layer, with `echo_floor_definition`, and witnesses that the load
succeeds and `agenda`'s get is served, as `loaded` and `got` state, so a host that refuses a size at the floor rather
than below it fails here. Beside `library/a-definition-below-its-size-fails-the-load` it pins the floor from both
sides.

## `library/an-unload-revokes-every-holding-of-a-definition`

A definition is called directly, so no call to it can be refused after its library's unload: the unload unregisters
it at once, and the core revokes it to every holder.

The suite builds the vector's `given` through its layer and witnesses:

- `agenda` and `calendar` are each told of the revocation, as `revoked` states, by the event the core queued and the
  layer handed on, never by a check the layer makes on a call;
- `notes`'s get after the unload misses, as the last of `got` states, so the definition is gone from the registry at
  the unload and not at its binary's close.

## `library/a-binary-stays-open-while-a-holding-lives`

The binary of a library with definition services is closed only once every holding of each of them has been released
since the unload, and then at once, when the core's `WEIVEL_EVENT_HOLDINGS_RELEASED` for the last of them is drained.

The suite builds the vector's `given` through its layer, hands the binary its counter through `agenda`'s object,
unloads with both holdings live, and witnesses `unloads` exactly:

- 0 after the unload, while `agenda` and `calendar` both still hold the definition: a host that closes the binary at
  the unload, as it may for a library of bytes functions alone, fails here;
- 0 once `agenda` has released its holding, while `calendar` still holds it;
- 1 once `calendar` has released the last holding, so the binary closed on the release event and not only when the
  host is freed.

## `library/a-binary-closes-once-every-definition-service-is-released`

A library may declare several definition services, and the core queues one `WEIVEL_EVENT_HOLDINGS_RELEASED` for each.
The binary stays open until it has drained the event of every one of them.

The suite builds the vector's `given` through its layer, `echo` and `echo-other` each a definition, `agenda` holding
both and `calendar` holding `echo-other`, hands the binary its counter through `agenda`'s `echo`, unloads, and
witnesses `unloads` exactly:

- 0 after the unload;
- 0 once `agenda` has released both its holdings, so that `echo` is wholly released and its event drained while
  `calendar` still holds `echo-other`: a host that closes the binary on the first such event fails here;
- 1 once `calendar` has released `echo-other`, the last holding of the last definition service.

## `library/a-binary-with-no-holding-closes-at-the-unload`

A definition service nothing holds when its library is unloaded has its `WEIVEL_EVENT_HOLDINGS_RELEASED` queued at the
unregistration itself, so the binary closes with the unload.

The suite builds the vector's `given` through its layer: `agenda` gets the definition and hands the binary its
counter, and is then released, which ends its holding while the service is still registered. It witnesses `unloads`
exactly: 0 before the unload, and 1 straight after it, so a host that waits for a release that will never come, or
closes only when the host is freed, fails here.

## `library/a-holding-ended-with-its-holder-closes-the-binary`

A holding is ended by the core, without a `weivel_release_holding`, when its holder is released, quarantined or
forgotten, and that end counts towards the binary's close exactly as a release does.

The suite builds the vector's `given` through its layer, `agenda` and `calendar` each holding the definition, hands
the binary its counter through `agenda`'s object, unloads, and witnesses `unloads` exactly:

- 0 after the unload;
- 0 once the host has released `agenda`, while `calendar` still holds the definition;
- 1 once the host has quarantined `calendar`, so a host that counts only the holdings its plugins released themselves
  never closes the binary and fails here. Forgetting a plugin ends its holding the same way, and is held by review and
  the language's own tests.

## `library/a-held-bytes-answer-keeps-the-binary-open-past-the-last-holding`

A binary is closed only once the last definition holding has been released and no bytes it answered are still held,
whichever ends last.

The suite builds the vector's `given` through its layer, a bytes service `echo-bytes` and a definition `echo`, both held
by `agenda`. `agenda` hands the binary its counter, calls `echo_echo` and keeps the answer, the library is unloaded,
and the suite witnesses `unloads` exactly:

- 0 after the unload;
- 0 once `agenda` has released its holding of the definition, the last one, while the bytes answer is still held: a
  host that closes the binary on the definition's event alone frees the memory under that answer, and fails here;
- 1 once `agenda` has dropped the answer, so the binary closed when the later of the two ended.

The vector binds a layer that answers bytes in place and releases them when their owner drops them, as C does. A layer
that copies a bytes answer once and releases it before the call answers, as a collected language must, holds no bytes
after the call: it witnesses instead that the binary closes at the last definition release, and the ordering itself is
held by the language's own tests. A call still running in the binary cannot be held across a step, so that half of
the rule is held by review and the language's own tests.

## `library/an-owner-hears-the-release-its-own-release-queued`

An owner released while nothing holds its awaiting registration has the release event queued by its own release call,
as the unregistration it makes, and so hears it, as it hears every event its release queued.

The suite builds the vector's `given` through its layer: `calendar` registers `calendar:feed` awaiting releases and
nothing gets it, then the host releases `calendar`. It witnesses:

- `calendar`'s listener hears the event for `calendar:feed` once, as `holdingsReleased` states: a layer that drops
  every event for an owner released since, rather than only those queued after its release, fails here;
- nothing reaches the layer's error sink or log, as `reported` states.

## `library/a-release-event-after-its-owner-was-quarantined-reaches-no-one`

An owner quarantined while a consumer still holds its awaiting registration hears nothing of a release that comes
later: the event is queued after the events its quarantine queued, so the layer drops it.

The suite builds the vector's `given` through its layer: `calendar` registers `calendar:feed` awaiting releases,
`agenda` gets it, the host quarantines `calendar`, and `agenda` then releases its revoked holding. It witnesses:

- `agenda` is told of the revocation the quarantine made, as `revoked` states;
- the release event its later release queued reaches no listener of `calendar`'s, as `holdingsReleased` states, and
  the layer drops it without a word to its error sink or log, as `reported` states.

## `library/a-forgotten-holder-ends-its-late-holding`

A released holder is still a plugin until it is forgotten, so its late get is let through and starts a holding of a
live definition. Forgetting it revokes and ends that holding, so nothing is left to keep the binary open.

The suite builds the vector's `given` through its layer: `agenda` gets the definition and hands the binary its
counter, is released, gets the definition again as a late call, and is forgotten. It witnesses:

- both gets are served, the second as a late call, as `got` states;
- `unloads` exactly: 0 after the forget, while the library is still loaded, and 1 straight after the unload, so a
  layer whose forget leaves the late holding standing keeps the binary open past the unload and fails here.

## The `description` op in a layer

A `description` vector loads a C library whose definitions declare a description (`weivel/description.h` of the
standard), through the layer's host. Its op is named after its directory, as every op here is; it is not the `library`
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
mirror's own, which the mirror's description describes.

**Which layers witness what.** A layer that binds descriptions, building its language's view of a definition from
its description as `weivel/typescript` and `weivel/java` do, witnesses every vector here. A layer that binds nothing,
as C, whose plugins cast a definition to its header's struct, witnesses the vectors directly in `description/`, which
only load the library and get its services, and is exempt from `description/binding/` (the `layer-exempt` line of
`checks/languages.txt`). Every layer checks a description at the load, with the standard's check or its equal, and
refuses the load when it fails; a layer that binds also compares every struct's size and every member's offset, and
every operation's offset in the definition's struct, with its own FFI layout and refuses the load on any difference.
A check that answers OUT_OF_MEMORY fails the load as out of memory, never as the caller's mistake.

Its `given` is `{ declaration, holders, steps }`. `declaration` is the library's declaration, as the standard's
`test/described/library.json` and its three broken variants are, handed to the host as the standard reads it with the
path of the built `described` library beside it; `holders` names plugins the suite opens before the first step, each
consuming every service id the declaration lists, exactly as in a `library` vector. The steps run in order, each once
the layer has nothing left to run from the one before:

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

After the last step the suite reads `loaded`, `callersMistake`, `got`, `sameObject` and `reported` as a `library`
vector defines them, the object `sameObject` reads being the language's bound object for a described definition, which
carries the address the definition's symbol answered as a registered definition does. It also reads `calls`, one
entry per call step in order, for what the operation answered:

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
`described_mirror_without_halve_definition` under the mirror's description, calls `halve` and then `negate`, and
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

## The `lifecycle` op

A `lifecycle` vector is answered only by a layer, because only a layer calls a plugin's `activate` and `deactivate`.
The runner knows the op so it can load and count the vectors, and no adapter answers it.

Its `given` is `{ edgeSource, plugins, steps }`. Each plugin is `{ id, provides, consumes, capabilities?, activate,
deactivate? }`: `id` is its import name, `provides` and `consumes` are service ids that go into its manifest under
`services` exactly as written, `capabilities`, when present, is `{ provides, consumes }` and goes into its manifest
exactly as written, and `activate` and `deactivate` are the actions its own entry runs, in order, when the host
calls them. A plugin without `deactivate` offers none. An action is one of:

- `{ register: id }`, recorded as `registered <plugin> <id>` once it returns;
- `{ want: id }`, which waits for the want to end before the next action, and `{ want: id, await: false }`, which
  starts it and goes on; either is recorded as `settled <plugin> <id>` when it is served or
  `rejected <plugin> <id> <code>` when it is refused, the code as a vector writes it (`stopped`);
- `{ get: id }`, recorded as `got <plugin> <id>` when it is served and `missed <plugin> <id>` otherwise;
- `{ watch: id }`, which installs a watch of the service and goes on, recorded as `heard <plugin> <id>` each time
  its listener hears the service registered; an unregistration it hears is not recorded;
- `{ provide: id }`, which provides the capability with an implementation that names its provider, recorded as
  `provided <plugin> <id>` once it returns;
- `{ all: id }`, which lists the capability's implementations, recorded as `listed <plugin> <id>` followed by the
  provider each implementation the layer handed back names, in the order handed back, each after one space, so a
  listing of none is `listed <plugin> <id>` alone.

The entry records `activate <plugin>` and `deactivate <plugin>` the moment the host calls each, before its first
action. The host's `steps` run in order, each made through the layer's public host:

- `{ activate: [plugin] }` activates those plugins with `edgeSource`, and the next step runs once the layer has
  nothing left to run, without waiting for an activation that cannot settle yet;
- `{ release: plugin }`, `{ destroy: true }`, and `{ verify: plugin }`, which asks the host to verify that plugin's
  activation;
- `{ plugin, get: id }`, a get made through that plugin's context and recorded as an action is.

After the last step, once the layer has nothing left to run, the suite reads:

- `plans`: one entry per `activate` step, the plugins in the order the layer's plan started them;
- `trace`: every record above, in the order made, judged with `before` because two plugins' records may interleave;
- `active`: the plugins the layer reports active, sorted;
- `forgotten`: the plugins the layer holds no row for, of those `given` names, sorted;
- `pending`: each want a plugin started that never ended, as `<plugin> <id>`, sorted;
- `verifies`: one entry per `verify` step, `quiet` when the call neither threw to the suite nor wrote to the layer's
  log or error sink, `loud` otherwise;
- `loud`: how many errors the layer threw to the suite or wrote to its log or error sink from the `destroy` step on.

In a language whose calls answer a status rather than throw, a call throws to the suite when it answers anything but
an answer to a well-made call: success, the core's refusal, nothing to answer, or that the host is destroyed.

## `lifecycle/an-awaited-want-does-not-hold-back-a-later-plugin`

The host starts each plugin's activation in plan order and never awaits one plugin's `activate` before starting the
next: a plugin whose `activate` waits for a service a later plugin registers would otherwise hold back every plugin
behind it for good, because a want has no deadline unless it asks for one.

The suite builds the vector's `given` through its layer and witnesses:

- the plan starts `agenda` before `calendar`, as `plans` states, with no edge between them under `capabilities`;
- `calendar`'s `activate` is called while `agenda`'s still waits, `calendar` registers, and only then is `agenda`'s
  want served, as the `trace` pairs state;
- both plugins end active, and no want is left pending.

## `lifecycle/events-of-an-activate-wait-for-every-activate-of-the-plan`

Activation is one step of a dispatch: with no dispatch running, the activating thread starts the plan as the
dispatcher, so what an activate's own calls queue is dispatched once every activate of the plan has been called,
never between one activate and the next. A layer that dispatches each activate's events as it returns lets an earlier
plugin's listener run ahead of a later plugin's activate, and so in another order than in a language that does not.

The suite builds the vector's `given` through its layer, so `agenda`'s `activate` installs a watch of
`calendar:events` and `calendar`'s registers it, and witnesses:

- the plan starts `agenda`, `calendar` and `notes` in that order, as `plans` states, with no edge between them under
  `capabilities`;
- `agenda`'s listener hears the registration only after `notes`'s `activate` has been called, the last of the plan,
  though `calendar` registered before it, as the `trace` pairs state;
- every plugin ends active, and no want is left pending.

A plan started while a dispatch runs is a call made inside that dispatch, so its events join the end of that queue
instead; the vector starts its plan outside any dispatch and does not witness that case, which is held by review and
the language's own tests.

## `lifecycle/an-activate-that-settles-at-once-is-verified-before-the-next-starts`

An activate that settles before it returns is verified before the host starts the next plugin of the plan, so the
next plugin's `activate` already sees what the verification admitted. A layer that verifies on a later turn, once a
result it answers has resolved or once every activate has been called, hands that plugin an empty listing.

The suite builds the vector's `given` through its layer, so `calendar`'s `activate` provides `screens` and returns,
and `agenda`'s, started after it by the capability edge, lists `screens`, and witnesses:

- the plan starts `calendar` before `agenda`, as `plans` states;
- `agenda`'s listing, made in its own `activate`, holds `calendar`'s implementation, as the `trace` pairs state;
- both plugins end active.

## `lifecycle/an-activate-refused-at-once-is-quarantined-before-the-next-starts`

An activate that settles before it returns and whose verification the core refuses is quarantined before the host
starts the next plugin of the plan, so what the quarantine takes from the plugin is gone when the next plugin's
`activate` runs.

The suite builds the vector's `given` through its layer, so `flaky`'s `activate` registers `flaky:feed` and
provides `screens` and returns, having declared `extras` too and never provided it, and `agenda`'s, started after it
by the capability edge, gets `flaky:feed`, and witnesses:

- the plan starts `flaky` before `agenda`, as `plans` states;
- `flaky` registered `flaky:feed` before it provided `screens`, as the first `trace` pair states. The miss below
  proves the quarantine only once the registration is known to have been made: a layer whose register fails
  without a word would miss it too, and pass without quarantining anything in time;
- `agenda`'s get, made in its own `activate`, misses `flaky:feed`, because the quarantine ended the registration
  before `agenda` was started, as the `trace` pairs state; a layer that quarantines later lets it be served;
- `agenda` alone ends active, `flaky` being quarantined.

## `lifecycle/a-release-deactivates-then-releases-then-forgets`

Releasing a plugin calls its `deactivate`, then releases it in the core, then hands it every event that release
queued, then ends the plugin's own entries, then forgets it. Each step is observable from outside.

The suite builds the vector's `given` through its layer and witnesses:

- `calendar`'s `deactivate` runs while it is still whole: its get of its own service is served there;
- after the `deactivate`, its want that never settled is rejected `stopped` and that rejection reaches the plugin,
  which it could not once the plugin was forgotten;
- once the release returns, `agenda`'s get of `calendar:events` is missed, because the registration ended with its
  plugin;
- `calendar` is forgotten, `agenda` stays active, and no want is left pending.

The vector witnesses the deactivate before the core release and the rejection reaching the plugin, but not that the
plugin's entries end and the forget is made only once the release's events have been dispatched: a layer that ends
them and forgets as soon as the core release returns still passes, and the vector fails only a layer that ends the
entries before the core release. That order, and every release made inside a dispatch, is held by review and the
language's own tests.

## `lifecycle/destroy-releases-a-plugin-whose-activate-is-in-flight`

Destroying the host releases every plugin it opened, one whose `activate` is still running included, before it
unloads libraries and destroys the core. A host that releases only the active plugins leaves the in-flight one's
want pending in a core that no longer exists.

The suite builds the vector's `given` through its layer, so `agenda`'s `activate` waits on a want nothing provides,
destroys the host, then asks it to verify `agenda`, and witnesses:

- `agenda` is released while its `activate` is in flight: its `deactivate` is called, then its want is rejected
  `stopped`, as the `trace` pairs state;
- no want is left pending;
- the verify after the destroy answers quietly, and nothing the destroy set off, `agenda`'s `activate` ending with
  it, throws to the suite or reaches the layer's log or error sink.

## The `host` op

A `host` vector runs a language's host program, the program `weivel/host.h` of the standard describes, against the
kit's stub provider `stub-host`, and judges what the provider was told. Only a language that ships a host program sits
these vectors; `checks/languages.txt` exempts C and Java from `host/` with `layer-exempt` lines, and the runner knows
the op so the vectors load, with no adapter to answer it. Every host vector covers `host`, the host contract.

**The library.** `stub-host` is `test/stub-host/` of the kit, the target `stub-host`: `stub-host.dll`,
`libstub-host.so` or `libstub-host.dylib` in the build's `bin` directory, under `bin/<configuration>` for a
multi-configuration generator. A suite builds it by configuring `test/stub-host` as a project of its own with
`-DWEIVEL_STANDARD_SOURCE_DIR` set to its standard sibling and `-DWEIVEL_CORE_SOURCE_DIR` to its core sibling, and
building that one target; inside the kit's own build it is built beside the kit's other fixtures. Its declaration is
`test/stub-host/library.json`, and it serves two definitions:

- `weivel.host`, the definition `weivel_standard_host`, whose description the library answers from
  `stub_host_description`, a one-line forwarder to the standard's `weivel_standard_host_description`. The forwarder
  exists because the library links the standard statically, so on Windows the standard's symbol is not among the
  library's exports, and a host finds a declared description only among those;
- `weivel.scope`, the definition `weivel_standard_scopes`, described by `stub_host_scopes_description`, the forwarder to
  `weivel_standard_scopes_description`.

It names nothing of any application, and it keeps everything in memory except its record.

**`stub-host.json`.** `start` reads its first argument as a directory and reads `stub-host.json` there. A path in the
file is joined to the directory with a `/` unless it is absolute; every path the plan answers is joined so. The file
is an object:

```json
{
  "mode": "serve",
  "libraries": [{ "binary": "lib/one.dll", "declaration": "lib/library.json" }],
  "plugins": [
    {
      "name": "alpha",
      "directory": "alpha",
      "manifest": "alpha/manifest.json",
      "entry": "alpha/entry.js",
      "scope": true,
      "settings": { "greeting": "hello", "count": 3, "on": true }
    },
    { "name": "beta", "directory": "beta", "manifest": "beta/manifest.json", "entry": "beta/entry.js",
      "scopeRefused": "beta declares a setting of no known kind" }
  ],
  "action": { "plugin": "alpha", "name": "echo", "arguments": { "text": "hi" } },
  "ticks": [40, 80],
  "exit": 0,
  "refuse": { "start": "told to refuse", "started": "will not run on", "tick": 2 },
  "malformedTick": 3
}
```

- `mode` is `serve` or `action`, and `plugins` a list, possibly empty; both are required. `libraries` is optional.
- A plugin's `name` is lowercase letters, digits and hyphens, once per file, and names it in the record. `directory`,
  `manifest` and `entry` are required. `scope: true` gives it a scope; `scopeRefused` refuses its scope with that
  detail; with neither, its scope opens and offers nothing. `settings` holds texts, numbers and booleans only, its
  config's starting values.
- `action` is required in `action` mode and refused in `serve` mode: the plugin by name, the action's name, and its
  `arguments`, any JSON, answered as the plan's value tree, kind NONE when absent.
- `ticks` are milliseconds after the `now` of the first tick, each later than the one before it: the first tick
  answers `now + ticks[0]`, each later one the first tick's `now` plus the next entry, and once they are spent a tick
  answers none.
- `exit` is the code `stop` answers, 0 when absent, 0 to 255.
- `refuse.start` makes `start` refuse with that detail, after recording itself; `refuse.started` makes `started`
  refuse after recording its outcomes; `refuse.tick`, a number from 1, makes that tick refuse after recording itself.
- `malformedTick`, a number from 1, makes that tick answer, after recording itself, a `has_next` of 2, which
  `weivel/host.h` calls malformed; with `refuse.tick` naming the same tick, the refusal wins.

A file that breaks this shape makes `start` refuse, its detail naming the member.

**What the provider does.** `start` answers the plan: the libraries, then the plugins in the file's order, the mode, and
in `action` mode the action's plugin by its manifest path as the plan names it. A second `start` while a run is open,
from a `start` that answered OK until its `stop`, is the caller's mistake: it is recorded and answers MALFORMED. After a
`stop`, or a `start` that did not answer OK, `start` opens a new run, so a suite may run every vector in one process.
`started` records the outcomes and answers OK. `tick` records how it came, answers the next time, and once it has
answered none never answers another. `finished` records how the action ended. `stop` records itself and answers `exit`.
`open` finds the plugin by its manifest path exactly as the plan named it, and answers a scoped plugin a config of its
`settings`, a secrets store that starts empty, paths (`plugin` its directory, `cache` and `config` the directories of
those names in it) and a log, each the plugin's own and none shared with another plugin. Every call is recorded under
one lock, so a call from another thread, as a blocking `tick` is, never splits a line. Its log writes its record line
synchronously, which a test fixture may and a provider may not, since `log.write` is not blocking
(`weivel/services.h`) and a provider buffers an entry or hands it off.

**`record.txt`.** `start` empties `record.txt` in the directory, then every call appends one line, in the order the
calls were made. A text that is not a name is written as a JSON string, and a value as compact JSON: a record's
members in order, a number in the shortest form that reads back as the same double. The lines are:

| line | written when |
| --- | --- |
| `start` then ` "<argument>"` for each argument after the directory | `start` was called with a readable file |
| `start again` | `start` was called a second time |
| `open <plugin> scoped`, `open <plugin> empty` or `open <plugin> refused` | `open` answered the plugin's scope, an empty one, or refused it; `unknown` for a manifest the plan never named |
| `log <plugin> <level> "<message>"`, then ` <fields>` when fields were given | the plugin's log wrote an entry; the level is `trace`, `debug`, `info`, `warn` or `error` |
| `set <plugin> "<key>" <value>` | the plugin's config stored a setting |
| `started <count>` | `started` was called with that many outcomes |
| `outcome <plugin> <state> detail` or `... no-detail` | one per outcome, in the order handed in: `activated`, `refused`, `failed`, or `state-<n>`; `detail` when the outcome carried a non-empty detail |
| `tick <n> first`, `tick <n> on-time`, `tick <n> early` or `tick <n> unasked` | the `n`th tick: the first, one whose `now` reached the time the previous tick answered, one that came before it, or one after a tick answered none |
| `finished result <value>`, `finished none`, `finished failure` or `finished malformed` | the action answered a value, answered nothing (kind NONE), threw, or `finished` was handed both or neither |
| `stop` | `stop` was called |

**The order the vectors judge.** The vectors judge the record with `equals`, so every order in them is one
`weivel/host.h` states:

- `start`, once, then the plan's libraries loaded in order: a library that will not load ends the run, with no further
  library loaded and no plugin activated, and `stop` is still called, as `weivel/host.h` says of the plan; a `mode`
  other than SERVE or ACTION is malformed and ends the run the same way, before any library is loaded, which no
  vector judges since `stub-host` answers no other mode;
- for each plugin of the plan in order, its scope opened immediately before its activation, and the next plugin's scope
  opened only after that activation ended; the scope is asked of whichever library the program loaded serves a
  described `weivel.scope`, the library it was started with included, the first loaded winning when two do, the
  library it was started with first, which for these vectors is `stub-host` itself; a plugin
  whose scope is refused is not activated and is told as `refused`, one whose activation throws is told as `failed`, and
  in both an outcome carries a non-empty detail whenever the refusal or the failure carried one; a plugin whose
  manifest cannot be read, or is refused by the program's standard or its core, is not activated either, and gets no
  scope, the program reading and checking a manifest before it opens that plugin's scope as `weivel/host.h` says, and
  it is told as `refused` with the detail saying why, and the plan goes on with the next plugin;
- no plugin is offered `weivel.host` or `weivel.scope`, as `weivel/host.h` says: the program loads and binds a
  library serving either, and a plugin that consumes them finds both absent;
- a library serving `weivel.scope` declares none of `weivel.config`, `weivel.secrets`, `weivel.paths` and
  `weivel.log`, as `weivel/host.h` says; when a loaded library declares one of them and a plugin's scope answers it,
  that scope cannot be offered, so the plugin is offered none of it and is not activated, its scope released, and it
  is told as `refused` with a detail, and the plan goes on with the next plugin;
- `started`, once, with every plugin's outcome in the plan's order, each plugin of the plan attempted;
- in `serve` mode, `tick` at once after `started` answered OK, and then whenever the time it answered comes: the
  program reads its clock again before each tick and waits again while it still reads before `next`, so no tick of a
  vector is `early`; a `has_next` of neither 0 nor 1 is malformed and ends the run, and `stop` comes next;
- in `action` mode, no tick at all: the action runs, and `finished` is told how it ended; an action whose plugin was
  not activated, or which that plugin does not offer, is told to `finished` as a failure;
- `stop` last, once, whatever happened: after a refused `started` or `tick`, after the action, and on a stop request.
  The program then exits with the code `stop` answered, 0 to 255, and with 1 when `stop` does not answer OK, which
  no vector judges since `stub-host`'s `stop` always does. When `start` refuses, the program calls nothing else and
  exits non-zero. Before `stop` the program deactivates every plugin it activated and releases every scope it opened,
  and it unloads the libraries after `stop`; no record line witnesses either, so no vector judges them.

**A stop request.** A program is asked to end a run by a stop request, which it takes through its own interface and,
from its command line, from its platform's terminate signals (SIGINT and SIGTERM on POSIX systems, SIGINT and SIGBREAK
on Windows), as `weivel/host.h` says. What it does depends on when it arrives:

- before `started` is called, activation being one phase, including before `start` answered OK, when the program
  keeps it, it takes effect once `started` has been told the outcome of
  every plugin of the plan, each attempted: `stop` comes next, with no tick and no action;
- during a later call, as during a blocking `tick`, it takes effect once that call returns: `stop` comes next, and no
  further tick;
- while an `action` run's action runs, it takes effect once `finished` has been told how the action ended: `stop` comes
  next.

Once the suite has sent its stop request it writes `stop-sent`, an empty file, in the run directory: in process,
after the program has taken the request, and as a child, after the signal is sent. A vector that holds a step open
follows `{ waitFor: "stop-sent" }` with `{ pause: 200 }`, which covers only a child's signal delivery. A suite never
sends a stop request to a run that has already ended.

A vector only says that the suite sends the program its stop request: a suite may send the signal to the program it
ran as a child, or ask the program it runs in process through that interface.

**A vector's `given`.** `{ stubHost, plugins, arguments?, stopAfter? }`:

- `stubHost` is the `stub-host.json` to write, without any plugin's `entry`. The suite makes a fresh directory, writes
  each plugin's entry and manifest as its own language writes them, at the plugin's `directory` and `manifest`, sets
  each plugin's `entry` to the path of the entry it wrote, relative to the directory, and writes the file;
- `plugins` says what each plugin's entry does, by `name`, in a form every language realises: `activate` is the steps
  its activation runs in order, and `actions` the actions it offers by name. The manifest consumes `weivel.config`,
  `weivel.secrets`, `weivel.paths` and `weivel.log`, so whatever the program offers the plugin under those ids it may
  get, unless the plugin carries `consumes`, a list of service ids its manifest consumes instead. A step is one of:
  - `{ log: <message> }`, an entry at `info` through `weivel.log`, with no fields;
  - `{ logSetting: <key> }`, the key read through `weivel.config`'s `get` and written at `info` as `<key>=<value>`: a
    text as it is, a number in the shortest form that reads back, a boolean as `true` or `false`, and `none` when the
    get answers none;
  - `{ set: <key>, to: <value> }`, the value stored through `weivel.config`'s `set` as a text, number or boolean;
  - `{ absent: <service id> }`, which throws unless the plugin's get of that id answers nothing;
  - `{ waitFor: <file name> }`, which waits until that file exists in the run directory, yielding to its language's
    event loop where it has one, and throws once the 30-second bound below has passed; a vector holds an activation or
    an action open with it until the suite has sent its stop request;
  - `{ pause: <milliseconds> }`, which waits that long, a whole number from 1 to 10000, in whatever way its language
    waits;
  - `{ throw: <text> }`, which throws an error of that text.

  A library of `stubHost`'s `libraries` may be `{ stubHostDeclaration: <file name> }` instead of a binary and a
  declaration: the suite writes it as the path of the `stub-host` library it runs and the path of that file in the
  kit's `test/stub-host/`, so the plan loads `stub-host` a second time under another declaration. The kit ships one,
  `library-declares-log.json`, which declares `weivel.log` as `stub_host_log_definition`, a log that keeps no entry.

  A plugin whose `unreadableManifest` is true has its entry written as any other's but its manifest not written at its
  `manifest` path, so the program cannot read it.

  An action is `{ echo: true }`, answering its arguments as they came, `{ answer: <value> }`, answering that value,
  `{ throw: <text> }`, throwing, or `{}`, answering nothing; any of them may carry `steps`, run first in order, so
  `{ steps: [{ log: "working" }, { waitFor: "stop-sent" }], answer: "done" }` logs, waits, then answers, and an action
  of `{ steps: [...] }` alone answers nothing, like `{}`;
- `arguments` are passed to the program after the directory, so it hands `start` the directory and then these;
- `stopAfter` is a record line: once `record.txt` holds it, the suite sends the program its stop request and waits for
  the run to end. A line counts once the newline after it is written. A `serve` run that refuses nothing ends no other
  way, so every such vector names one.

The suite runs `<program> <stub-host library> <directory> <arguments...>` as its program takes them, or the same run
through the program's interface, and waits for the run to end. One bound, 30 seconds, holds every wait: a
`stopAfter` line that has not appeared within 30 seconds of the run starting, or a run that has not ended within 30
seconds of its stop request or, without one, of starting, fails the vector. The suite then witnesses:

- `record`, the lines of `record.txt`, exactly as `equals` states;
- `exit`, the program's exit code, exactly; or, where the vector has `exitsNonZero: true`, that the code is not 0.

## The `host/` vectors

- `host/start-hands-its-arguments-and-activates-the-plan-in-order`: the plan lists `gamma`, `alpha`, `beta`, each
  logging `up` as it activates, and the program is given `kit-token`. `start` is handed `kit-token` after the
  directory, each scope is opened and its plugin activated in the plan's order, not the alphabet's, and `started` is
  told three activations in that order;
- `host/a-plugin-reads-its-own-scoped-config`: `alpha` and `beta` hold different `greeting`s; each reads its own,
  `alpha` stores `seen` and reads it back, and `beta` reads `count` but finds no `seen`, so a scope is the plugin's
  alone and a setting stored by one never reaches the other;
- `host/a-refused-scope-quarantines-that-plugin-alone`: `beta`'s scope is refused, so `beta` never activates and is
  told as `refused` with a detail, while `alpha` before it and `gamma` after it activate;
- `host/an-activation-that-throws-is-told-as-failed`: `alpha` throws after logging, and is told as `failed` with a
  detail, while `beta` after it activates;
- `host/a-plugin-without-a-scope-is-offered-nothing`: `alpha`'s scope opens empty, and its activation, which throws if
  any of the four standard ids answers it something, succeeds;
- `host/tick-is-called-at-each-time-it-answered`: the ticks answer 40 and 80 milliseconds after the first, and the
  program calls each no earlier than answered; after the third tick answered none, no tick comes before the stop
  request;
- `host/an-action-runs-and-its-result-reaches-finished`: `alpha`'s `echo` answers its arguments, which reach
  `finished` as the value the plan carried, with no tick in the run;
- `host/an-action-answering-nothing-reaches-finished-as-none`: an action that answers nothing reaches `finished` as a
  value of kind NONE, never as NULL or a failure;
- `host/a-throwing-action-reaches-finished-as-a-failure`: a throwing action reaches `finished` as a failure;
- `host/stop-answers-the-exit-code`: `stop` answers 7, and the program exits with 7;
- `host/a-stop-request-stops-cleanly-while-a-tick-waits`: the next tick is ten minutes away when the stop request
  comes, and the program calls `stop` once, ticks no more and exits 0;
- `host/a-refused-start-calls-nothing-else`: `start` refuses, so nothing else is called, `stop` included, and the
  program exits non-zero;
- `host/a-refused-started-still-stops`: `started` refuses, so the program calls `stop` at once, without a tick, and
  exits with the code it answered;
- `host/a-refused-tick-still-stops`: the first tick refuses, so the program calls `stop` at once, ticks no more, and
  exits with the code `stop` answered, 3;
- `host/an-unreadable-manifest-is-refused-and-the-rest-activate`: `alpha`'s manifest is not written, so `alpha` is told
  as `refused` with a detail, no scope is opened for it, and `beta` after it activates;
- `host/a-plan-library-that-will-not-load-still-stops`: the plan's one library names a binary that does not exist, so
  the program activates nothing, calls `stop` at once and exits with the code it answered, 4;
- `host/a-stop-request-during-activation-waits-for-started`: the stop request comes while `alpha`'s activation waits for
  `stop-sent`, and `beta` still activates, `started` is told both outcomes, and `stop` follows with no tick;
- `host/a-malformed-next-ends-the-run`: the first tick answers a `has_next` of 2, so the program calls `stop` next,
  ticks no more and exits with the code `stop` answered, 6;
- `host/a-stop-request-during-an-action-waits-for-finished`: the stop request comes while `alpha`'s action waits for
  `stop-sent`, and `finished` is told its result before `stop`;
- `host/a-plugin-is-offered-no-scope-or-host-service`: `alpha`'s manifest consumes the four standard ids and
  `weivel.scope` and `weivel.host` too, and its activation finds both of those absent, then logs `up` through its scope;
- `host/a-library-declaring-a-standard-id-makes-the-scope-unofferable`: the plan's one library is `stub-host` under
  `library-declares-log.json`, so `alpha`'s scope, which answers a log, cannot be offered: `alpha` is told as
  `refused` with a detail and never activates, while `beta` after it, whose scope opens empty, activates.
