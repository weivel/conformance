## What this is

weivel's conformance kit: the plugin core's behaviour written down as vectors, data rather than a test suite inside
one implementation, and the C runner, `weivel-conformance`, that sits the core and the weivel standard against them.
It also holds the drift checks every weivel repository's drift job runs, and every timing budget weivel holds.

## What this is not

It is not an engine and implements no part of weivel: it builds the core and the standard from pinned checkouts and
asks them questions. It does not run the vectors only a language layer can witness; each language implementation's
own suite does, and the kit's checks make sure none is skipped.

## Where to read on

Start at [docs/README.md](docs/README.md), the kit's architecture, and descend: the vectors and their tiers, the
runner and the contract an adapter keeps, the drift checks, the benchmark budgets, and one doc per source folder.
