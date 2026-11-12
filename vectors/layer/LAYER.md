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
