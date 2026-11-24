# Layer `lifecycle` vectors

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
