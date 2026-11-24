The runner folder builds `weivel-conformance`. Everything but `main.c` goes into the static library `conformance_runner`, which links the pinned `weivel_core` and `weivel_standard` publicly, so the kit's unit tests drive the very loader, matcher and adapters the program runs, and an adapter calls the real core and standard in-process rather than a model of them. The contract the program keeps with an adapter and with a vector is in [the runner's docs](../../runner/README.md).

<!-- generated: source-children -->
- [CMakeLists.txt](../../../runner/CMakeLists.txt) - the `conformance_runner` library, every adapter in it, and the `weivel-conformance` program
- [src/](src/) - the loader, the matcher, the report and the adapters
<!-- end generated -->
