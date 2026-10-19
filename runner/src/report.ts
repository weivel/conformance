import type { MatchResult, MatchStatus } from "./match.js";
import { OPS } from "./vectors.js";

const STATUSES: MatchStatus[] = ["passed", "failed", "errored", "unwitnessed"];

/**
 * Turns match results into the table a reader judges an implementation by, plus the process exit
 * code.
 *
 * @implNote Only `passed` is a pass. `unwitnessed` is deliberately not one: a suite that lets a
 * driver declare a vector inapplicable is a suite the implementation grades itself against, which is
 * the objection that put the vectors in their own repository. `notes` carry what the driver did
 * rather than what it answered, a non-zero exit or a line of unparseable output among them, and any
 * note fails the run on its own.
 */
export function report(results: MatchResult[], notes: string[] = []): { text: string; exitCode: number } {
  const lines: string[] = [];
  lines.push(heading());
  for (const op of OPS) {
    const forOp = results.filter((result) => result.vector.op === op);
    if (forOp.length > 0) {
      lines.push(row(op, forOp));
    }
  }
  lines.push(row("total", results));
  const notPassed = results.filter((result) => result.status !== "passed");
  if (notPassed.length > 0) {
    lines.push("");
    for (const result of notPassed) {
      lines.push(`${result.status.toUpperCase()} ${result.vector.id} (covers ${result.vector.covers})`);
      for (const reason of result.reasons) {
        lines.push(`    ${reason}`);
      }
    }
  }
  if (notes.length > 0) {
    lines.push("");
    lines.push("the driver itself:");
    for (const note of notes) {
      lines.push(`    ${note}`);
    }
  }
  const failed = notPassed.length > 0 || notes.length > 0 || results.length === 0;
  if (results.length === 0) {
    lines.push("");
    lines.push("no vectors ran, which is not a pass");
  }
  return { text: `${lines.join("\n")}\n`, exitCode: failed ? 1 : 0 };
}

function heading(): string {
  return ["op".padEnd(12), ...STATUSES.map((status) => status.padStart(12))].join("");
}

function row(label: string, results: MatchResult[]): string {
  const counts = STATUSES.map((status) => String(results.filter((result) => result.status === status).length));
  return [label.padEnd(12), ...counts.map((count) => count.padStart(12))].join("");
}
