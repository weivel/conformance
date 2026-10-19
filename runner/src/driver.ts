import { spawn } from "node:child_process";
import type { Vector } from "./vectors.js";

export interface Outcome {
  id: string;
  witnessed: boolean;
  result: Record<string, unknown>;
}

export interface DriverRun {
  outcomes: Map<string, Outcome>;
  exitCode: number | null;
  timedOut: boolean;
  stderr: string;
  malformed: string[];
}

/**
 * Feeds every vector to a driver as NDJSON on stdin and collects one outcome per line of stdout,
 * matched later by id and never by position.
 *
 * @implNote Resolves rather than rejecting when the driver dies, exits non-zero or runs out of time:
 * a vector the driver never answered has to reach the report as an absence, because the only other
 * thing an absence can become is a pass. The timeout is what stops a driver that hangs from turning
 * a red suite into a build that never ends, which this tree has already paid for once.
 */
export async function runDriver(command: string, vectors: Vector[], timeoutMillis: number): Promise<Map<string, Outcome>> {
  return (await runDriverVerbose(command, vectors, timeoutMillis)).outcomes;
}

/** The same run, with everything the report wants to say about the driver itself. */
export function runDriverVerbose(command: string, vectors: Vector[], timeoutMillis: number): Promise<DriverRun> {
  return new Promise<DriverRun>((resolve) => {
    const child = spawn(command, {
      shell: true,
      stdio: ["pipe", "pipe", "pipe"],
      detached: process.platform !== "win32",
    });
    const outcomes = new Map<string, Outcome>();
    const malformed: string[] = [];
    let pending = "";
    let stderr = "";
    let timedOut = false;

    const timer = setTimeout(() => {
      timedOut = true;
      killTree(child.pid);
    }, timeoutMillis);

    const settle = (exitCode: number | null) => {
      clearTimeout(timer);
      takeLine(pending, outcomes, malformed);
      resolve({ outcomes, exitCode, timedOut, stderr, malformed });
    };

    child.stdout.setEncoding("utf8");
    child.stdout.on("data", (chunk: string) => {
      pending += chunk;
      let newline = pending.indexOf("\n");
      while (newline >= 0) {
        takeLine(pending.slice(0, newline), outcomes, malformed);
        pending = pending.slice(newline + 1);
        newline = pending.indexOf("\n");
      }
    });
    child.stderr.setEncoding("utf8");
    child.stderr.on("data", (chunk: string) => {
      stderr += chunk;
    });
    child.on("error", (cause) => {
      stderr += `${cause.message}\n`;
      settle(null);
    });
    child.on("close", settle);

    for (const vector of vectors) {
      child.stdin.write(`${JSON.stringify(vector)}\n`);
    }
    child.stdin.end();
  });
}

/**
 * @implNote A driver command is run through a shell, so the child is the shell and the engine is its
 * child. Killing the shell alone leaves the engine running: on Windows it is orphaned outright,
 * since there is no process group to signal.
 */
function killTree(pid: number | undefined): void {
  if (pid === undefined) {
    return;
  }
  if (process.platform === "win32") {
    spawn("taskkill", ["/pid", String(pid), "/T", "/F"], { stdio: "ignore" });
    return;
  }
  process.kill(-pid, "SIGKILL");
}

function takeLine(line: string, outcomes: Map<string, Outcome>, malformed: string[]): void {
  const text = line.trim();
  if (text.length === 0) {
    return;
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(text);
  } catch {
    malformed.push(text);
    return;
  }
  if (!isOutcome(parsed)) {
    malformed.push(text);
    return;
  }
  outcomes.set(parsed.id, parsed);
}

function isOutcome(value: unknown): value is Outcome {
  if (typeof value !== "object" || value === null) {
    return false;
  }
  const candidate = value as Partial<Outcome>;
  return typeof candidate.id === "string" && typeof candidate.witnessed === "boolean";
}
