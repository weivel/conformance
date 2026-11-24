The runner itself, in the order a run goes through it: `main.c` takes `run <directory>`; `walk.c` lists every `.json` file under it, sorted, so a report never depends on the file system's order; `vectors.c` reads each into a vector, decides its tier from its path as [the tiers doc](../../../vectors/tiers.md) states, and refuses the whole corpus for the reasons [the runner's docs](../../../runner/README.md) list; `runner.c` hands each core or standard vector's `given` to its op's adapter (under `ops/`) and marks each layer vector awaiting a layer; `answer.c` is how an adapter writes its answer, as JSON text that `match.c` reads back with the standard's JSON reader and compares against the vector's `then`; `report.c` prints the table and the reasons and decides the exit code. `runner.c` and `report.c` keep [what a result means](../../../runner/results.md): an adapter that answers nothing, an answer that is incomplete or not an object, and an op with no adapter all reach `errored` there. `match.c` holds no assertion that reads a message; its assertions are [what `then` can assert](../../../vectors/then.md).

<!-- generated: source-children -->
- [answer.c](../../../../runner/src/answer.c) - the answer writer: containers, keys, scalars, separators, and a writer marked broken by a call out of order
- [answer.h](../../../../runner/src/answer.h) - `answer_writer`, and why an answer is text the matcher reads back
- [main.c](../../../../runner/src/main.c) - the command line: `run <directory>`, exit 2 for a refused corpus or misuse
- [match.c](../../../../runner/src/match.c) - every `then` assertion (`code`, `subject`, `hasFix`, `before`, `contains`, `equals`, `absent`, `present`, deep equality), a reason per failure, and the refusal of a `then` that asserts nothing
- [match.h](../../../../runner/src/match.h) - `match_then`, `match_refuse_meaningless` and the renderer that quotes an answer in a reason
- [ops/](ops/) - one adapter per op the core or the standard answers, and the helpers they share
- [report.c](../../../../runner/src/report.c) - the table by op and status, the failed and errored vectors with their reasons, the layer vectors not run, and the exit code
- [report.h](../../../../runner/src/report.h) - `report_write`, and why an awaiting vector is never counted as passed
- [runner.c](../../../../runner/src/runner.c) - judging one vector with its adapter, and a whole corpus
- [runner.h](../../../../runner/src/runner.h) - the verdict statuses and the judging calls
- [text.c](../../../../runner/src/text.c) - a growable text buffer that answers an empty string after an allocation failure
- [text.h](../../../../runner/src/text.h) - `text_builder` and `text_duplicate`
- [vectors.c](../../../../runner/src/vectors.c) - loading a corpus: reading and parsing each file, its shape, its id and `covers`, its tier, sorting by id, and every refusal of a corpus
- [vectors.h](../../../../runner/src/vectors.h) - a vector, a corpus, the tiers, and what `covers` may name
- [walk.c](../../../../runner/src/walk.c) - the sorted, recursive listing of `.json` files on Windows and POSIX
- [walk.h](../../../../runner/src/walk.h) - `walk_json_files` and the path list
<!-- end generated -->
