# Layer `host` vectors

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
- `action` is required in `action` mode and refused in `serve` mode: the plugin by name, the action's `name`, and its
  `arguments`, any JSON, answered as the plan's value tree, kind NONE when absent. `service` and `method`, each a
  text, are answered as the plan's `action_service` and `action_method`, and with either of them `name` may be left
  out, answered as NULL; one of the two without the other, or an empty one, is answered as given, so a vector can
  judge the program refusing that plan. A plan whose `size` cannot hold a member the run sets, from a program
  compiled before it, makes `start` refuse, after recording itself, with a detail saying the program is too old for
  the run; nothing is ever written beyond a plan's size.
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
in `action` mode the action's plugin by its manifest path as the plan names it, and the action's name, service and
method as the file gives them. A second `start` while a run is open,
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
  not activated, or which that plugin does not offer, is told to `finished` as a failure. Which action runs is
  `weivel/host.h`'s rule: with `action_service` and `action_method` both set, the method of that service the plugin
  itself registered, `action_name` unread, so a service another plugin registered, or a method the service does not
  offer, is a failure and never falls back to the entry's actions, and a method that throws is a failure as an action
  that throws is; with neither set, `action_name` among the entry's actions, whatever services the plugin provides;
  with one set and the other not, or either one empty, the plan is malformed and ends the run as a malformed mode
  does, before any library is loaded, so `stop` follows `start` at once;
- a plugin's provide of a capability that a loaded library publishes a contract for, under `contracts` in its
  declaration, is refused when the implementation does not offer every operation of the contract's description but its
  releases, a release being an operation of the description that answers NOTHING and every other one answering a
  STATUS (`weivel/description.h`), so the operations to offer follow from the description alone and never from their
  names. The refusal's code is `contract-mismatch` and its subject the bare name of the first operation to offer, in
  the order the description lists them, that the implementation does not offer, and the capability is not provided:
  a plugin whose manifest provides it and whose provide was refused has not provided it when its activation ends, so
  the core's check of the activation refuses it and it is told as `failed` with a detail. An operation that answers a handle and the operation taking that handle back, as a stream's
  `next` takes the run its `run` answered, are both operations of the implementation itself, so the whole check is
  made at the provide. An implementation offering more than that, a release or an operation the description does not
  list, is accepted. A provide of a capability no loaded library publishes a contract for is never checked against
  one;
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
  get, unless the plugin carries `consumes`, a list of service ids its manifest consumes instead. A plugin's
  `provides`, when it carries one, maps each service id it provides, namespaced by its `name` as the manifest's
  `importName` (`alpha:accounts` for `alpha`), to that service's methods by name, each method written as an action
  below is and taking the action's arguments as an action does. Its manifest provides those ids, and its activation
  registers each one, an object offering those methods, before it runs its steps. A plugin's `implements`, when it
  carries one, maps each capability id it provides to the operations its implementation offers by name, each written
  as an action is; no vector calls one. Its manifest provides those ids as capabilities, and its activation provides
  each only at a `provide` step naming it. A step is one of:
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
  - `{ throw: <text> }`, which throws an error of that text;
  - `{ provide: <capability id> }`, which provides the plugin's `implements` of that id, an implementation offering
    those operations, through its layer, and writes at `info` through `weivel.log` `provided <capability id>` when the
    provide succeeds, or `refused <code> <subject>` when it is refused, and never throws. Only `contract-mismatch` is a
    code a vector judges, so a language writes any other refusal in whatever form it names it.

  A library of `stubHost`'s `libraries` may be `{ stubHostDeclaration: <file name> }` instead of a binary and a
  declaration: the suite writes it as the path of the `stub-host` library it runs and the path of that file in the
  kit's `test/stub-host/`, so the plan loads `stub-host` a second time under another declaration. The kit ships two:
  `library-declares-log.json`, which declares `weivel.log` as `stub_host_log_definition`, a log that keeps no entry;
  and `library-declares-a-contract.json`, which declares that log under `stub-host.log`, an id no plugin consumes, and
  publishes a contract under the capability `stub-host.tally`, described by `stub_host_tally_description`. That
  description is `stub_host_tally` of `test/stub-host/stub_host_tally.h`, written by `weivel-describe`, which
  `stub-host` implements nowhere: `count`, a plain operation; `run`, which answers a `stub_host_tally_run` handle;
  `next`, which takes that handle back; and the releases `release_run` and `release_refusal`. So an implementation
  of `stub-host.tally` offers `count`, `run` and `next`.

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
- `host/an-action-calls-a-method-of-a-service-its-plugin-provides`: the plan names `alpha`'s service `alpha:accounts`
  and its method `login`, which logs `login` and answers its arguments, and those reach `finished`; the entry's own
  `login` action, which answers something else, never runs;
- `host/a-throwing-method-reaches-finished-as-a-failure`: the method the plan names throws, and reaches `finished` as
  a failure, though the plan also names the entry's action `login`, which answers;
- `host/a-service-another-plugin-provides-reaches-finished-as-a-failure`: the plan names `alpha` and the service
  `beta:accounts`, which `beta` registered, so `alpha` does not provide it: `finished` is told a failure, and neither
  `beta`'s method nor `alpha`'s own `login` action runs;
- `host/a-method-the-service-does-not-offer-reaches-finished-as-a-failure`: the plan names the method `logout` of a
  service offering only `login`, and `finished` is told a failure even though the entry offers an action `logout`;
- `host/an-action-named-alone-runs-the-entrys-action-beside-a-provided-service`: the plan names only `login`, so the
  entry's action `login` runs and its answer reaches `finished`, though the plugin's service has a method `login` too;
- `host/a-plan-naming-a-service-without-a-method-is-malformed`: the plan sets `action_service` alone, so the run ends
  before anything is loaded: `stop` follows `start`, and the program exits with the code it answered, 5;
- `host/a-plan-naming-a-method-without-a-service-is-malformed`: the same with `action_method` alone.
- `host/a-method-of-a-service-no-plugin-registered-reaches-finished-as-a-failure`: the plan names `alpha:missing`,
  which no plugin registered, and `finished` is told a failure, the entry's own `login` action never running;
- `host/a-method-run-whose-plugin-was-not-activated-reaches-finished-as-a-failure`: `alpha`'s scope is refused, so it
  is never activated, and the method the plan names reaches `finished` as a failure;
- `host/a-plan-naming-an-empty-service-is-malformed`: `action_service` is the empty text beside a method, so the run
  ends as a malformed plan does, `stop` following `start`, exit 5;
- `host/a-malformed-plan-loads-none-of-its-libraries`: the plan sets `action_service` alone and carries `stub-host`
  under `library-declares-log.json` as a library; the run ends with `stop` after `start`, exit 5. No record line
  witnesses a library loading, since a program need call nothing of a library as it loads it, so the vector judges
  that a plan carrying a library still ends at once with nothing activated, and the load itself is not witnessed.

## The `host/contract/` vectors

Each runs one plugin, `alpha`, whose activation provides `stub-host.tally` at a `provide` step and then logs `up`, so
the record shows the provide's outcome and that the activation went on past it. Each refusal vector's outcome is
`failed`, since `alpha`'s manifest provides the capability its refused provide never provided, and each accepted
one's is `activated`.

- `host/contract/an-implementation-offering-every-operation-is-accepted`: with `library-declares-a-contract.json`
  loaded, `alpha` offers `count`, `run` and `next` and none of the releases, and the provide succeeds;
- `host/contract/an-implementation-missing-an-operation-is-refused-at-provide`: `alpha` offers `run` and `next` but
  not `count`, and the provide is refused as `contract-mismatch` with the subject `count`;
- `host/contract/an-implementation-lacking-next-is-refused-at-provide`: `alpha` offers `count` and `run`, which
  answers a run, but not `next`, which reads it, and the provide is refused as `contract-mismatch` with the subject
  `next`: reading a stream is an operation of the implementation, checked at the provide like any other;
- `host/contract/the-first-missing-operation-the-description-lists-is-named`: `alpha` offers `run` alone, missing both
  `count` and `next`, and the provide is refused as `contract-mismatch` with the subject `count`, the first the
  description lists, never `next`;
- `host/contract/an-implementation-offering-more-than-the-contract-is-accepted`: `alpha` offers `count`, `run` and
  `next`, the release `release_run`, and `reset`, which the description does not list, and the provide succeeds;
- `host/contract/a-capability-no-loaded-library-publishes-is-provided-unchecked`: the plan loads no library, so
  nothing publishes `stub-host.tally`, and `alpha`'s implementation offering `count` alone is provided.
