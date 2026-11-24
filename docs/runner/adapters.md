# Adapters

An adapter is a C function, one per op, registered in `runner/src/ops/ops.c` and declared by `runner/src/ops/ops.h`.
It is given the vector's `given` as a read-only JSON node, drives the core or the standard in-process, and writes
its answer with the `answer_*` functions as one JSON object. It never reads a vector's `then`: an adapter that does
is marking its own paper. An op whose adapter does not exist, or an adapter that answers nothing, makes its vectors
`errored`, and an absence is never a pass.

An adapter reads whether a want still waits, or a watch still hears, through `weivel_handle_state`, which changes
nothing, and only after the last step. **It relies on waiting or watching against anything else**, a refused
query included: the core forgets how a want or a watch ended once its handle's slot is reused, so how a want ended
is read from its `WANT_SETTLED` event and never from the query.

## Virtual time

`budgetMillis`, `pollMillis` and the other `...Millis` members of a vector are logical. An adapter hands the core
the time it wants the core to see and never sleeps in real time: a 30 second budget costing 30 seconds of wall
clock is a suite nobody runs.
