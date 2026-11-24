The kit's own tests, in two kinds. The C tests (`*_test.c`, each an executable the root CMake registers, failing through the `CHECK` macro of `check.h`) drive the runner's library directly: the answer writer, the matcher, judging and reporting, every adapter against the real pinned core and standard, the corpus as committed, and the stub provider `stub-host`. The CMake scripts (`*.cmake`, run with `cmake -P` by the tests `checks/` and `benchmarks/` register) plant a tree in the build directory, run one program over it and assert its exit code and what it printed, one named scenario per run. Every violation a check must refuse is planted at test time and never committed, since a committed file naming a forbidden word or carrying another language would make the kit's own drift fail on the kit; for the same reason a script that must spell a listed word spells it in pieces. `corpus_test.c` pins how many vectors each directory holds, every op the kit knows and every section and invariant `covers` may name, so a vector added or lost shows in a diff of the test. The two list tests pin `names.txt` and `languages.txt` line for line, so what a role allows changes only together with its test. Non-code fixtures (vector trees, budget files, opaque bytes) sit under `fixtures/` beside the core gate's C fixtures.

<!-- generated: source-children -->
- [adapters_test.c](../../../test/adapters_test.c) - each adapter driven with a `given` and its answer checked: refusal names and subjects, plan edges, degraded wants, virtual time, the standard's refusals and the library reading
- [answer_test.c](../../../test/answer_test.c) - the answer writer: nesting, separators, escaping, and a document that is not complete
- [benchmark_test.cmake](../../../test/benchmark_test.cmake) - runs the benchmark program in one mode against one budget and asserts its exit code, skipping a Release-only test in any other build
- [check.h](../../../test/check.h) - the `CHECK` macro, the text comparison, and the summary every C test exits with
- [checks_test.cmake](../../../test/checks_test.cmake) - every purity, names, parity and layers scenario, each planting its tree and asserting the exit code and output
- [core_gate_test.cmake](../../../test/core_gate_test.cmake) - every core gate scenario against the fixture libraries and the real core, skipped on a platform it was not written for
- [corpus_test.c](../../../test/corpus_test.c) - the committed corpus: counts per directory, `covers`, every op, each refusal of a corpus, tiers by path, and every standard vector passing
- [fixtures/](fixtures/) - what the tests plant or load, the core gate's C fixtures among them
- [language_budget_test.cmake](../../../test/language_budget_test.cmake) - a language budget file read line by line and held to its format, its required gates on every platform and backend, and its margins
- [languages_list_test.cmake](../../../test/languages_list_test.cmake) - pins `checks/languages.txt` to its exact lines
- [match_test.c](../../../test/match_test.c) - every `then` assertion, its reasons, and the assertions that assert nothing
- [names_list_test.cmake](../../../test/names_list_test.cmake) - pins `checks/names.txt` to its nine words
- [runner_test.c](../../../test/runner_test.c) - judging and reporting: passed, failed, errored and awaiting, the exit code, and an empty run
- [stub-host/](stub-host/) - the stub provider of `weivel.host` and `weivel.scope` a language's host program is run against
- [stub_host_test.c](../../../test/stub_host_test.c) - `stub-host` driven through its definitions (the plan, the run file, the record, the scope), and its built library checked against each declaration it ships
<!-- end generated -->
