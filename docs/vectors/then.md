# What `then` can assert

A vector asserts decisions and never wording, so two implementations in two languages need not phrase a message
identically. Each key of `then` is compared against the same key of the answer:

| in `then` | holds when |
| --- | --- |
| `null` | the key is absent or null in the answer |
| `{"code": "x"}` | the answer's value is a refusal carrying the code `x` |
| `{"subject": x}` | the answer's value carries the subject `x`; a null `x` holds only when `subject` is answered and null |
| `{"hasFix": b}` | the answer's value answers `hasFix` as `b`, never inferred from a `fix` |
| `{"before": [["a", "b"]]}` | the answer is an array in which `a` precedes `b`, pairwise, because a valid topological order is not unique |
| `{"contains": [x]}` | the answer is an array containing each element |
| `{"equals": x}` | the answer equals `x` exactly |
| `{"absent": true}` | the key is absent or null |
| `{"present": true}` | the key is present and not null |
| anything else | deep equality |

A key absent from `then` asserts nothing, and a `then` with no key at all, or `present: false` or `absent: false`, is refused when the corpus loads.
