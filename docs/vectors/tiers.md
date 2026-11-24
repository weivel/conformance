# Tiers

A vector's directory says which suite judges it.

- `vectors/core/<op>/` is judged by the runner, through an adapter that drives the core in-process.
- `vectors/standard/<op>/` is judged by the runner against the weivel standard, the replaceable layer above the
  core, through an adapter that drives the standard's library (`siblings/standard`) in-process.
- `vectors/layer/<op>/` is witnessed by each language's own layer suite, because the core never calls a host
  language. [The layer vectors' docs](layer/README.md) say what each suite must witness. The runner loads and counts
  these and does not run them; the report lists them as awaiting a layer. The `lifecycle`, `description` and `host`
  ops are asked only here: the kit knows them so their vectors load, and no adapter answers them, so a core vector of
  any of them would be errored. A `host` vector runs a language's host program as a child process against the kit's
  stub provider `stub-host` (`test/stub-host/`), and judges the lines the provider appended to its `record.txt`; the
  shape of its `stub-host.json` and of every record line is in [the `host` op](layer/host.md).

`weivel-conformance run <directory>` accepts the whole corpus or any directory inside it. A vector's tier comes from
the last `core`, `standard` or `layer` directory in its path, and a vector under none is refused.

The kit's own tests sit the pinned core against `vectors/core` and the pinned standard against `vectors/standard`,
and the core's drift job sits the core under test against the same corpus, so a core change that breaks a vector
fails in the core's own CI. Each language implementation's own suite witnesses `vectors/layer`, and the kit's
[`layers` check](../checks.md) fails a language whose tests name a layer vector nowhere.

A vector's id is its family, which is its op, a `/`, and its name, every part lowercase words joined by hyphens. A
vector in a subdirectory of its op's directory carries that subdirectory in its id, as
`description/binding/a-none-answer-arrives-as-none` does, so the id is still its path under its tier without `.json`.
