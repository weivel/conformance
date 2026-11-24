# Layer `consume` vectors

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
