import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { runDriverVerbose } from "./driver.js";
import { match } from "./match.js";
import { report } from "./report.js";
import { loadVectors, OPS, VECTORS_PATH, type Op, type Vector } from "./vectors.js";

export const DEFAULT_TIMEOUT_MILLIS = 120000;

/** Runs a driver over a vector set and reports, which is the three modules composed. */
export async function runConformance(
  command: string,
  vectors: Vector[],
  timeoutMillis: number,
): Promise<{ text: string; exitCode: number }> {
  const run = await runDriverVerbose(command, vectors, timeoutMillis);
  const results = vectors.map((vector) => match(vector, run.outcomes.get(vector.id)));
  return report(results, driverNotes(run, timeoutMillis));
}

function driverNotes(run: Awaited<ReturnType<typeof runDriverVerbose>>, timeoutMillis: number): string[] {
  const notes: string[] = [];
  if (run.timedOut) {
    notes.push(`timed out after ${timeoutMillis} ms and was killed, so the answers below are partial`);
  }
  if (!run.timedOut && run.exitCode !== 0) {
    notes.push(`exited ${run.exitCode === null ? "without a status" : String(run.exitCode)}`);
  }
  for (const line of run.malformed) {
    notes.push(`answered a line that is not an outcome: ${line.slice(0, 200)}`);
  }
  if (run.stderr.trim().length > 0) {
    notes.push(`wrote to stderr: ${run.stderr.trim().slice(0, 500)}`);
  }
  return notes;
}

interface Arguments {
  command: "run" | "validate";
  driver?: string;
  only?: Op;
  timeoutMillis: number;
}

function parse(argv: string[]): Arguments {
  const [verb, ...rest] = argv;
  if (verb !== "run" && verb !== "validate") {
    throw new Error("usage: conformance run --driver <command> [--only <op>] | conformance validate");
  }
  const parsed: Arguments = { command: verb, timeoutMillis: DEFAULT_TIMEOUT_MILLIS };
  for (let at = 0; at < rest.length; at += 2) {
    const flag = rest[at];
    const value = rest[at + 1];
    if (value === undefined) {
      throw new Error(`${flag} needs a value`);
    }
    if (flag === "--driver") {
      parsed.driver = value;
    } else if (flag === "--only") {
      if (!(OPS as readonly string[]).includes(value)) {
        throw new Error(`--only takes one of ${OPS.join(", ")}, not ${value}`);
      }
      parsed.only = value as Op;
    } else if (flag === "--timeout") {
      parsed.timeoutMillis = Number(value);
    } else {
      throw new Error(`unknown flag ${flag}`);
    }
  }
  return parsed;
}

async function main(argv: string[]): Promise<number> {
  const parsed = parse(argv);
  const all = loadVectors(VECTORS_PATH);
  const vectors = parsed.only === undefined ? all : all.filter((vector) => vector.op === parsed.only);
  if (parsed.command === "validate") {
    process.stdout.write(`${vectors.length} ${vectors.length === 1 ? "vector is" : "vectors are"} valid against vector.schema.json\n`);
    return vectors.length === 0 ? 1 : 0;
  }
  if (parsed.driver === undefined) {
    throw new Error("run needs --driver <command>");
  }
  const outcome = await runConformance(parsed.driver, vectors, parsed.timeoutMillis);
  process.stdout.write(outcome.text);
  return outcome.exitCode;
}

const invokedDirectly = process.argv[1] !== undefined && resolve(process.argv[1]) === fileURLToPath(import.meta.url);

if (invokedDirectly) {
  main(process.argv.slice(2)).then(
    (code) => process.exit(code),
    (cause: Error) => {
      process.stderr.write(`${cause.message}\n`);
      process.exit(2);
    },
  );
}
