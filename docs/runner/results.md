# What a result means

The runner prints one table, a row per op that has vectors and a `total` row, a column per status, then each failed
or errored vector with its id, what it `covers` and its reasons, then how many layer vectors await a layer suite.

| status | meaning |
| --- | --- |
| `passed` | the adapter answered, and the answer satisfies the vector |
| `failed` | the adapter answered, and the answer does not |
| `errored` | no adapter answered the vector, or its answer could not be read |
| `awaiting a layer` | a layer vector, to be witnessed by a language's own layer suite |

**Only `passed` is a pass.** `failed`, `errored` and `awaiting a layer` fail the run, and the report keeps each in
its own column so a reader sees what was not run: a suite that counts a vector it did not run as a pass reports a
green it has not earned. An empty run is not a pass either.
