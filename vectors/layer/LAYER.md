# Layer vectors

A vector here states something only a language layer can witness, because the core never calls a listener,
never throws and never runs host code. The runner loads these vectors and counts them, but no adapter in the
runner runs them: each language's own layer suite must contain a test that witnesses every vector in this
directory, naming it by id, its path here without `.json`. The kit's `layers` drift check fails a language whose tests
name one nowhere.

## `consume/a-throwing-listener-does-not-silence-the-others`

A layer delivers a watch's changes to listeners it holds. A listener that throws on every call must not
stop the others hearing, and the throw must not be reported as a refusal.

The suite builds the plugins and steps of the vector's `given` through its layer, installs the listener of
the throwing watch so that it throws on each call after recording what it was handed, and then witnesses:

- every other listener still hears each change, in the order delivered, exactly as `heard` states;
- the throwing listener's own entry still records what it was handed before it threw;
- nothing is added to `reported`, and the first refusal of the run is none.

## `consume/a-refused-verify-quarantines-the-plugin`

A plugin whose activation check refuses is quarantined by the host in the same step, before anything else runs.
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

## `want/a-registration-at-the-deadline-loses`

A want whose deadline and a provider's registration fall at the same moment times out: the deadline is handed to
the core before the registration is made, so the registration settles nothing. A layer that runs the want on its
own clock must keep that order, because an event loop that delivers the registration first serves a want the
core has already timed out.

The suite builds the provider and the consumer through its layer, asks with the vector's timeout on a clock it
controls, registers the provider at `registerAtMillis`, and then witnesses:

- the want ends timed out at the deadline, exactly as `timedOut` and `atMillis` state, and is never served;
- the want's refusal is `timed_out`, its subject the service id it waited for (`accounts`), with a fix.

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
  firing at the announced deadline is what handed the core that time.

## `want/a-settled-want-disarms-the-timer`

A want that settles before its deadline takes the deadline with it, and the layer disarms on the event that says so.

The suite builds the provider and the consumer through its layer, asks with the vector's timeout on a clock it
controls, registers the provider at `registerAtMillis`, runs the clock to `horizonMillis`, records the timer's moves
as above, and witnesses:

- `timer` exactly: armed at 1000 when the want was made at 0, and disarmed at 500, when the registration settled the
  want, so the timer never fires;
- the want is served at 500 and never times out, with no refusal.

## The `lifecycle` op

A `lifecycle` vector is answered only by a layer, because only a layer calls a plugin's `activate` and `deactivate`.
The runner knows the op so it can load and count the vectors, and no adapter answers it.

Its `given` is `{ edgeSource, plugins, steps }`. Each plugin is `{ id, provides, consumes, activate, deactivate? }`:
`id` is its import name, `provides` and `consumes` are service ids that go into its manifest under `services`
exactly as written, and `activate` and `deactivate` are the actions its own entry runs, in order, when the host
calls them. A plugin without `deactivate` offers none. An action is one of:

- `{ register: id }`, recorded as `registered <plugin> <id>` once it returns;
- `{ want: id }`, which waits for the want to end before the next action, and `{ want: id, await: false }`, which
  starts it and goes on; either is recorded as `settled <plugin> <id>` when it is served or
  `rejected <plugin> <id> <code>` when it is refused, the code as a vector writes it (`stopped`);
- `{ get: id }`, recorded as `got <plugin> <id>` when it is served and `missed <plugin> <id>` otherwise.

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

## `lifecycle/an-awaited-want-does-not-hold-back-a-later-plugin`

The host starts each plugin's activation in plan order and never awaits one plugin's `activate` before starting the
next: a plugin whose `activate` waits for a service a later plugin registers would otherwise hold back every plugin
behind it for good, because a want has no deadline unless it asks for one.

The suite builds the vector's `given` through its layer and witnesses:

- the plan starts `agenda` before `calendar`, as `plans` states, with no edge between them under `capabilities`;
- `calendar`'s `activate` is called while `agenda`'s still waits, `calendar` registers, and only then is `agenda`'s
  want served, as the `trace` pairs state;
- both plugins end active, and no want is left pending.

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
