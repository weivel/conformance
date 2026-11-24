# The drift checks

Five programs in `checks/`. Four take a repository root and the fifth a built core. They exit 0 when clean, 1 with
every offender listed, and 2 when they were given nothing to check (no files, no words, no modules, no vectors, no
symbols) or are misused.

The core's drift job reads this kit at a pinned commit, the `kit_ref` of the core's drift workflow, so a change to a
check, list or budget here reaches the core only when that pin moves, in the same wave.

```
weivel-purity <root> <role> <languages-file>
weivel-names <root> <words-file>
weivel-parity <root> <language> <languages-file>
weivel-layers <root> <layer-vector-directory> <language> <languages-file>
weivel-core-gate <rules-file> <msvc|gnu|apple> <tool> <shared-library> [<static-library>...]
weivel-core-gate --sources <rules-file> <core-root>
```

- Purity: the roles `c`, `core`, `kit` and `standard` are written in the language `checks/languages.txt` lists as
  `c`, so they allow what its lines allow (C, headers, CMake), JSON as data and prose, and refuse a build file of any
  other language in the list (`package.json`, `tsconfig.json`, `build.gradle`) wherever it sits. The C language
  implementation is the role `c` and the language `c`: one word, read by purity as the role and by parity and layers
  as the language. Any other role is a language named in `checks/languages.txt`, whose lines read
  `<language> <kind> <value>` with kind `extension`, `name`, `pattern`, `prefix`, `source-root`, `source-base`,
  `test-root`, `header-root` or `layer-exempt`; it allows its own files and prose, and a generated `.js` under a Java
  repository is an offender.
  Prose, in every role, is a `.md` or `.txt` file anywhere; every role also allows `.gitignore`, `.gitattributes`,
  `LICENSE`, `MODULES` and `repo.json` at the root, and `docs/VERSION`, the version file of the docs standard the
  repository's `docs/` follows, at that path only.
  A `pattern` holds one `*`, standing for any characters. A pattern that begins with it names files by their ending,
  as an `extension` does, and like one needs a name before that ending: `*.cmake` does not allow a file named
  `.cmake`. A star further in may stand for nothing, so `tsconfig*.json` allows `tsconfig.json`.
  `languages_list_holds_the_pinned_lines` holds the list to its exact lines, so what a role allows or refuses changes
  only together with that test.
- Names: no file may carry a word from the list, case-folded. `LICENSE`, `repo.json`, the list itself, binary
  files and, at the root only, the directories `.git`, the hosting-service directory beside it, `build`, `build-*`,
  `siblings` and `node_modules` are not read. Purity and the core's source scan skip the same root directories, and
  nothing deeper is skipped by name. A file is binary when a NUL byte occurs in its first 8 KB, and each skipped file is printed.
  The list always comes from the pinned kit commit, never from the tree under test, so a repository cannot shorten the
  list it is checked with; a file whose trimmed, non-empty lines are the list word for word is that list's copy and
  is not read, which is how the kit's own `checks/names.txt` passes. That skip holds in every role, and outside the
  kit a full copy of the list is itself the drift this rule forbids: a word list kept in the tree under test.
  A `package-lock.json` at any depth is read as JSON through the standard's reader, not as text, and only where it can
  name a dependency or its origin: the top-level `name`, every key of `packages`, each entry's `name`, `version` and
  `resolved`, and the keys and specs of its `dependencies`, `devDependencies`, `optionalDependencies`,
  `peerDependencies`, `peerDependenciesMeta` and `requires`, recursing into a legacy nested `dependencies` tree.
  `funding`, `license`, `integrity` and every other member name no dependency and are not read, so npm's own metadata
  does not trip the list. A lockfile that is not a JSON object is an offender, never passed unread.
- Parity: `MODULES` in the core lists the modules a language implements, one per line, with ` optional` after the
  ones a repository may omit. Parity reads it from the core the repository under test pins, at `siblings/core/MODULES`
  under its root, where its dependency checkout puts that core, and never from the kit's own core: a language pinned to
  an older core is judged by the list it adopted. A repository without that file is refused with 2. A language
  repository carries one top-level directory per module, named exactly as listed, under its `source-root`. A language
  may name several source roots, and then a module present under any one of them is present. A missing required
  module, a directory not on the list, a module directory that holds no file and a loose file directly under a source
  root are all offenders. Every directory directly under each source root is judged, whatever its name. A declared
  source root missing on disk is refused with 2, the message naming the root, the line that declared it
  (`java source-root src/main/java22/com/weivel`) and the list it came from. A module holds a file when one sits in it at any depth, except under a
  directory directly inside the module that carries a name skipped at a repository root (`build`, `build-*`,
  `siblings`, `node_modules`, `.git` and the hosting-service directory), so a module holding only build output holds
  no file. A language `checks/languages.txt` does not list, or lists without a `source-root`, is refused rather than
  read as `src`, so a new language is added to the list before its repository is checked. A language whose modules
  sit below a package root names the directory its sources lie in with a `source-base`, as Java does with
  `src/main/java` around its source root `src/main/java/com/weivel`: every file under the base, at any depth and in a
  directory of any name, must lie under the source root, and any other (`src/main/java/Main.java`,
  `src/main/java/org/weivel/core/Core.java`) is an offender. Each directory on the way down from the base to the
  source root must hold no directory but the root's next step, so an empty `src/main/java/org` or
  `src/main/java/com/other`, which no file walk sees, is an offender too. Moving the source root below the base
  therefore refuses everything that was refused before. A base that does not hold a source root strictly, one equal
  to it (the root `src/main/java/` under the base `src/main/java` too) or beside it under a longer name
  (`src/main/java` around `src/main/javax/...`), is refused with 2, and so are a base holding several source roots and,
  where a language names any base, a source root no base or several bases hold; each such refusal names the base and
  the roots, or the root and the bases, it fails to pair. Java names two trees this way, the
  second `src/main/java22` around `src/main/java22/com/weivel`, where its sources for Java 22 and later lie: each is
  judged by every rule above, so a file outside the package root there is an offender and a module present only there
  is present. Under the directory holding a source base (`src/main`), a file carrying one of the language's
  `extension`s that lies under no base is an offender, so an undeclared `src/main/java23/...` tree or a loose
  `src/main/D.java` cannot hide beside the declared ones; any other file there, `src/main/resources/notes.txt`, is
  not read. Parity reads nothing of Java outside the directories holding its source bases: its tests under `src/test/java` and the plugins its tests load from a jar, under `src/plugins/java`, are no
  modules, and purity still allows them as Java sources. A language that publishes its interface apart from its
  sources names where with a `header-root`, as C does with `include/weivel/c`. Every file under the header root is
  read, at any depth and in a directory of any name: nothing is skipped there, `build` and `siblings` included. Each
  must carry one of the language's `extension`s after a name (`.c` or `.h` for C, so `core.json`, `core.md` and a file
  named `.h` are offenders) and belong to a listed module, by its own name without the extension (`core.h`) or by the
  first directory it sits under (`host/entry.h`); anything else is an offender. A header root that is missing or holds
  no file is refused with 2, because nothing was checked, and so is a language given a `header-root` and no
  `extension`. The header root refuses strays and asks no header of any module: a module with no header there, or with
  several, passes. Parity reads nothing else under `include/`, so a file beside the header root, such as the one
  header that includes the rest, is not read by it.
- Layers: every layer vector's id, its path under `vectors/layer` without `.json`
  (`consume/a-throwing-listener-does-not-silence-the-others`), must appear in some source file of the language, one
  with an `extension` the list gives it, under its `test-root`, standing on its own: a longer id, a file path that
  contains it, or a vector copied under the tests as a `.json` fixture does not count. A missing id is an offender.
  Under the layer vector directory nothing is skipped, so a vector in a directory of any name is asked for, except
  under a directory a `layer-exempt` line gives the language (`c layer-exempt description/binding`): its vectors ask
  for what that language's layer does not do by design, as [the layer vectors' docs](vectors/layer/README.md) say of
  each such directory, so none of them is asked for and the summary counts them apart. The line names a directory
  under the layer vector directory, and covers the vectors whose ids begin with it and a `/`, so
  `description/bindings/...` is still asked for. An exemption no vector lies under is refused with 2, since a stale
  line would exempt whatever directory is given its name next. Under the test root a directory directly beneath it
  that carries a name skipped at a repository root (`build`, `build-*`, `siblings`, `node_modules`, `.git` and the
  hosting-service directory) is not read, so build output names no vector. A language the list does not name, or
  names without a `test-root` or an `extension`, is refused, never read as the whole repository, because sources and
  fixtures outside the tests would then satisfy it. It runs for a language implementation, never
  for `core`, `standard` or `kit`, and only proves each vector is named; the language's own coverage test, which reads
  its describe names, stays the strong check.
- Core gate: reads the built core with the platform's own tool (`dumpbin` beside the MSVC compiler, `nm` elsewhere):
  the shared library's imports and exports, and, when given, each object of the static library built from the same
  sources. `checks/core_gate.txt` holds the rules, one `<kind> <reason> <symbol>` per line: `forbid` names what reads
  the world (file access, environment, clock, process, code loading, network), `forbid-name` a name no symbol may
  carry (`json`), and the `allow-*` kinds what the core may import: the C library everywhere, lock calls only in the
  object defining the symbol on the `mutex-object-defines` line, what the compiler emits, and what the toolchain's
  startup code draws into a shared library. Anything else is an offender. An import table cannot say which object
  calls a lock, so lock calls are attributed through the objects, and without them the gate says they were not.
  The MSVC startup code imports a clock and the environment (`QueryPerformanceCounter`, `_configure_narrow_argv` and
  others), so such an import passes only when the objects are given and none of them references it. The objects
  must be the ones the library was built from: every export must be defined by exactly one of them (a `startup-export`
  line names what the toolchain exports by itself), and an object with no symbol table, such as one compiled for
  link-time code generation, is refused.
- Core gate, `--sources`: an import table cannot see an intrinsic, an inline system call or inline assembly, so the
  same rules file also lists `forbid-source` names (`<reason> <word|call|prefix|text> <name>`) found anywhere under
  the core's `src` and `include`, and `source-in-mutex-only` lock names refused outside the `mutex-source` file. The
  core never calls out, so its public interface may hand it nothing it could call: each `public-header` line
  (`<reason> <path>`, `include/weivel/core.h`) names a header whose every function pointer type is an offender.
  The header is read after the C compiler the kit was built with has preprocessed it (`cl /E` under MSVC, `cc -E`
  elsewhere), so its own macros are expanded and its comments gone. Its `#include` lines are blanked first, so no
  system header is read and none is needed, and a macro only a system header defines stays unexpanded. Of what the
  header's own lines become, the gate tokenizes the code and drops string and character literals, then refuses:
  - a parenthesized pointer declarator followed by a parameter list: any number of stars, with any of `const`,
    `volatile`, `restrict`, `__restrict`, `_Atomic` and the MSVC pointer qualifiers among them, any `__attribute__`,
    `__declspec` or `_Alignas` group anywhere in it, any number of words before the stars, and after them a name, an
    array of them or a function returning one: `(*name)(`, `(**name)(`, `(* volatile name)(`, `(*name[4])(`,
    `(*name(void))(`, `(API CALL *name)(`;
  - the same with nothing after the stars when every word before them is a calling convention (`__cdecl`,
    `__stdcall`, `__fastcall`, `__vectorcall` and the like) or there is none: `(*)(`, `(__cdecl *)(`;
  - a parameter of function type, a name inside parentheses with a type before it and a parameter list after it:
    `void f(void callback(int))`;
  - a typedef of a function type, at the line of its `typedef`: `typedef int name(int);`.
  A cast such as `(uint32_t *)(x)` has a type word and no name around its star and is not one, and the operand of
  `sizeof`, `_Alignof`, `typeof` or `__builtin_*` and the inside of an attribute are never read as declarators, so
  `typedef int x __attribute__((aligned(8)));` and `typedef char pad[sizeof(int)];` pass. Each offending line is
  reported once, by its line in the header. A named header that is missing, or that its preprocessor refuses, is
  refused with 2, and so is a rules file with no `public-header` line, since the interface would then go unread.
- Under sanitizers: the job built with address and undefined behaviour sanitizers leaves out every test that reads
  the built core (`core_gate_*` and `core_reads_nothing_of_the_world`). Instrumentation makes the core import
  `__asan_*` and `__ubsan_*`, which the gate rightly refuses, and those tests say nothing about the runner. A
  repository copying the job copies the exclusion, and a new test that reads a built binary is added to it, or it
  fails under instrumentation.
