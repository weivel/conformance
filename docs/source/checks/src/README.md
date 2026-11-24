The sources of the five drift check programs. Each program is one file with its own `main` (`purity.c`, `names.c`, `parity.c`, `layers.c`, `core_gate.c`); the rest are the helpers they share, built once as the static library `checks_tree` (`tree`, `languages`, `tool`) or added to the one program that needs them (the core gate's rules, source scan, header scan and tokenizer, and the names check's lockfile reader, which reads JSON through the standard's reader and so links the standard). Exit codes, skip lists and what each program reads are [the drift checks doc](../../../checks.md)'s. `tree.c` is the one place that walks a repository, so every program skips the same root directories. The core gate starts the platform's tool and the C compiler through `tool.c`, which passes each argument through unchanged whatever spaces or quotes a path holds. A source file that must spell a forbidden word spells it in pieces, since the names check reads this folder too.

<!-- generated: source-children -->
- [core_gate.c](../../../../checks/src/core_gate.c) - `weivel-core-gate`: the shared library's imports and exports and each object's symbols read through the platform tool, judged against the rules, with lock calls attributed through the objects, and `--sources`
- [core_header.c](../../../../checks/src/core_header.c) - the public header scan: every declarator that makes a function pointer type, a parameter of function type, and a typedef of a function type
- [core_header.h](../../../../checks/src/core_header.h) - `core_header_scan` and why the core's interface may hand it nothing it could call
- [core_rules.c](../../../../checks/src/core_rules.c) - reading `core_gate.txt`: each rule's kind, reason, form and pattern, the mutex lines, and matching a symbol against a rule
- [core_rules.h](../../../../checks/src/core_rules.h) - the rule kinds and forms, a rule set, and the token helpers the core gate shares
- [core_sources.c](../../../../checks/src/core_sources.c) - the scan of the core's `src` and `include` for `forbid-source` names, lock names outside the mutex source, and each public header
- [core_sources.h](../../../../checks/src/core_sources.h) - `core_sources_scan`, which sees what an import table cannot: an intrinsic, an inline system call, inline assembly
- [header_tokens.c](../../../../checks/src/header_tokens.c) - preprocessing a header with its `#include` lines blanked, and tokenizing only the lines the line markers give back to it
- [header_tokens.h](../../../../checks/src/header_tokens.h) - a header's tokens, each with its line in the header, string and character literals dropped
- [languages.c](../../../../checks/src/languages.c) - reading `languages.txt` for one language, or for every language but one
- [languages.h](../../../../checks/src/languages.h) - a language's rules: extensions, names, patterns, prefixes, source roots and bases, test and header roots, and layer exemptions
- [layers.c](../../../../checks/src/layers.c) - `weivel-layers`: every layer vector's id named on its own in a language's tests, apart from the directories it is exempt from
- [lockfile.c](../../../../checks/src/lockfile.c) - the names check's structural read of an npm lockfile, member by member
- [lockfile.h](../../../../checks/src/lockfile.h) - which members of a lockfile can name a dependency or its origin, and so are read
- [names.c](../../../../checks/src/names.c) - `weivel-names`: every text file read for a listed word, case-folded, binaries skipped and printed, and the list's own copy recognised by its content
- [parity.c](../../../../checks/src/parity.c) - `weivel-parity`: a language's module directories against the pinned core's `MODULES`, its source bases and its header root
- [purity.c](../../../../checks/src/purity.c) - `weivel-purity`: what a role may hold, its own language and prose, and never another language's build file
- [tool.c](../../../../checks/src/tool.c) - starting a program with its arguments quoted for Windows or POSIX and capturing what it prints
- [tool.h](../../../../checks/src/tool.h) - `tool_run`
- [tree.c](../../../../checks/src/tree.c) - listing a repository's files and directories, the root skip list, and reading a file or its lines
- [tree.h](../../../../checks/src/tree.h) - the text list, the walks with and without the root skip list, and the readers
<!-- end generated -->
