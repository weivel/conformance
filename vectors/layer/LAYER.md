# Layer vectors

A vector here states something only a language layer can witness, because the core never calls a listener,
never throws and never runs host code. The runner loads these vectors and counts them, but no adapter in the
runner runs them: each language's own layer suite must contain a test that witnesses every vector in this
directory, naming it by id.

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
- the refused plugin's state reads quarantined, its reason the activation mismatch, whose subject is the plugin's
  import name (`flaky`);
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
