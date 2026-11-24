The sources the core gate is proven against: small libraries shaped like the core, each built twice by `checks/CMakeLists.txt`, as a shared library (whose import table the gate reads) and as a static library (whose objects attribute each symbol), so every scenario of `test/core_gate_test.cmake` runs the gate on a binary that holds exactly one known fault. Every fixture library is `base.c` plus `mutex.c` plus one `plant_<name>.c`: `base.c` allocates as the core may, `mutex.c` defines `weivel_mutex_lock` and `weivel_mutex_unlock`, the one object the rules let call a lock, and the plant adds the one import or symbol the scenario asks about, or nothing at all for the clean fixture the gate must pass. A lock taken in any other object, an environment read, a file opened, the clock read, an import off the allow list, a parser linked from another library and a parser compiled inside each have a plant. `parser_provider.c` is the other library that plant imports from, and `silent_tool.c` a stand-in tool that prints nothing, which the gate must refuse rather than read as an empty, clean import table. The fixtures are compiled with the kit's strict warnings, so a plant is a real call the gate must find, never dead text.

<!-- generated: source-children -->
- [base.c](../../../../../test/fixtures/core-gate/base.c) - the clean part every fixture library shares: an allocation and its release, which the rules allow everywhere
- [fixture.h](../../../../../test/fixtures/core-gate/fixture.h) - the export macro and the functions every fixture library exports
- [mutex.c](../../../../../test/fixtures/core-gate/mutex.c) - the mutex object: the only place a fixture may take a lock, as the core's own mutex object is
- [parser_provider.c](../../../../../test/fixtures/core-gate/parser_provider.c) - a separate shared library exporting a JSON parser, for the plant that imports one
- [plant_fopen.c](../../../../../test/fixtures/core-gate/plant_fopen.c) - opens a file, which the gate must refuse
- [plant_getenv.c](../../../../../test/fixtures/core-gate/plant_getenv.c) - reads the environment, which the gate must refuse
- [plant_lock_elsewhere.c](../../../../../test/fixtures/core-gate/plant_lock_elsewhere.c) - takes a lock outside the mutex object, which the gate must attribute and refuse
- [plant_nothing.c](../../../../../test/fixtures/core-gate/plant_nothing.c) - adds nothing, so the fixture is clean and the gate must pass it
- [plant_parser_import.c](../../../../../test/fixtures/core-gate/plant_parser_import.c) - imports a JSON parser from another library, which the gate must refuse by name
- [plant_parser_inside.c](../../../../../test/fixtures/core-gate/plant_parser_inside.c) - compiles a JSON parser into the library and exports it, which the gate must refuse by name
- [plant_qsort.c](../../../../../test/fixtures/core-gate/plant_qsort.c) - imports `qsort`, a C library call off the allow list
- [plant_time.c](../../../../../test/fixtures/core-gate/plant_time.c) - reads the clock, which the gate must refuse
- [silent_tool.c](../../../../../test/fixtures/core-gate/silent_tool.c) - a tool that prints nothing, standing in for a `dumpbin` or `nm` that read nothing
<!-- end generated -->
