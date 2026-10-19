import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

export interface FakeBehaviour {
  /** The `result` object answered for every vector. */
  answer?: Record<string, unknown> | null;
  /** Answered as `witnessed`, which defaults to true. */
  witnessed?: boolean;
  /** Vector ids to leave unanswered. */
  omit?: string[];
  /** Answer in reverse order, so a runner matching by position mismatches everything. */
  reverse?: boolean;
  /** Read the input and answer nothing, ever. */
  hang?: boolean;
  /** Answer this many vectors and then exit, mid-stream. */
  answerOnly?: number;
  /** Exit with this status once finished. */
  exitCode?: number;
  /** Write one line of output that is not an outcome. */
  malformed?: boolean;
  /** Answer this vector id twice, the second time with a different result. */
  answerTwice?: string;
}

/**
 * Writes a throwaway driver and returns the command that runs it, so the runner can be exercised
 * before any engine exists and against behaviour no real driver would volunteer.
 */
export function fakeDriver(behaviour: FakeBehaviour): string {
  const directory = mkdtempSync(join(tmpdir(), "conformance-fake-"));
  const script = join(directory, "driver.mjs");
  writeFileSync(script, source(behaviour), "utf8");
  return `node "${script}"`;
}

function source(behaviour: FakeBehaviour): string {
  return `const behaviour = ${JSON.stringify(behaviour)};
let input = "";
process.stdin.setEncoding("utf8");
process.stdin.on("data", (chunk) => {
  input += chunk;
});
process.stdin.on("end", () => {
  if (behaviour.hang) {
    setInterval(() => {}, 1000);
    return;
  }
  const vectors = input.split("\\n").filter((line) => line.trim().length > 0).map((line) => JSON.parse(line));
  const ordered = behaviour.reverse ? vectors.slice().reverse() : vectors;
  const limit = behaviour.answerOnly === undefined ? ordered.length : behaviour.answerOnly;
  const omit = behaviour.omit ?? [];
  if (behaviour.malformed) {
    process.stdout.write("this is not an outcome\\n");
  }
  for (const vector of ordered.slice(0, limit)) {
    if (omit.includes(vector.id)) {
      continue;
    }
    write(vector.id, behaviour.answer ?? {});
    if (behaviour.answerTwice === vector.id) {
      write(vector.id, { shadowed: true });
    }
  }
  process.exit(behaviour.exitCode ?? 0);
});

function write(id, result) {
  const witnessed = behaviour.witnessed === undefined ? true : behaviour.witnessed;
  process.stdout.write(JSON.stringify({ id, witnessed, result }) + "\\n");
}
`;
}
