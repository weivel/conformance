# Layer `library` vectors

## The `library` op in a layer

A `library` vector under `vectors/layer/library/` loads a C library through the layer's host, which only a layer can
do, because the core never opens a binary. Its library is the core's test library, `echo`, built beside the core
(`test/echo/echo.h`):

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
