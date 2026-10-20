import type { Outcome } from "./driver.js";
import type { Vector } from "./vectors.js";

export type MatchStatus = "passed" | "failed" | "errored" | "unwitnessed";

export interface MatchResult {
  vector: Vector;
  status: MatchStatus;
  reasons: string[];
}

const ASSERTIONS = ["mentions", "hasFix", "before", "contains", "equals", "absent"] as const;

/**
 * Compares one outcome against one vector's `then`, structurally and never by wording.
 *
 * @implNote Takes an undefined outcome on purpose. A vector the driver never answered is errored,
 * and making that case unrepresentable in the type is exactly how it would become a pass.
 */
export function match(vector: Vector, outcome: Outcome | undefined): MatchResult {
  if (outcome === undefined) {
    return { vector, status: "errored", reasons: ["the driver answered nothing for this vector"] };
  }
  if (!outcome.witnessed) {
    return { vector, status: "unwitnessed", reasons: [unwitnessedReason(outcome)] };
  }
  const result = outcome.result ?? {};
  const reasons: string[] = [];
  for (const [key, expected] of Object.entries(vector.then)) {
    checkKey(key, expected, result[key], reasons);
  }
  return { vector, status: reasons.length === 0 ? "passed" : "failed", reasons };
}

/**
 * @remarks A driver that cannot witness a vector is asked to say why, and the why is the whole value
 * of the unwitnessed state: without it a report says only that something is missing.
 */
function unwitnessedReason(outcome: Outcome): string {
  const reason = outcome.result?.["reason"];
  return typeof reason === "string" && reason.trim().length > 0
    ? reason
    : "the driver reported it cannot witness this vector, and gave no reason";
}

function checkKey(key: string, expected: unknown, actual: unknown, reasons: string[]): void {
  if (expected === null) {
    if (actual !== null && actual !== undefined) {
      reasons.push(`${key}: expected none, the driver answered ${render(actual)}`);
    }
    return;
  }
  if (isAssertionBlock(expected)) {
    for (const [assertion, argument] of Object.entries(expected)) {
      checkAssertion(key, assertion, argument, actual, reasons);
    }
    return;
  }
  if (!deepEqual(expected, actual)) {
    reasons.push(`${key}: expected ${render(expected)}, the driver answered ${render(actual)}`);
  }
}

function checkAssertion(key: string, assertion: string, argument: unknown, actual: unknown, reasons: string[]): void {
  switch (assertion) {
    case "mentions":
      checkMentions(key, argument as string[], actual, reasons);
      return;
    case "hasFix":
      checkHasFix(key, argument === true, actual, reasons);
      return;
    case "before":
      checkBefore(key, argument as string[][], actual, reasons);
      return;
    case "contains":
      checkContains(key, argument as unknown[], actual, reasons);
      return;
    case "equals":
      if (!deepEqual(argument, actual)) {
        reasons.push(`${key}: expected ${render(argument)}, the driver answered ${render(actual)}`);
      }
      return;
    case "absent":
      if (argument === true && actual !== null && actual !== undefined) {
        reasons.push(`${key}: expected none, the driver answered ${render(actual)}`);
      }
      return;
    default:
      throw new Error(`unknown assertion ${assertion} on ${key}`);
  }
}

function checkMentions(key: string, substrings: string[], actual: unknown, reasons: string[]): void {
  const text = textOf(actual);
  if (text === undefined) {
    reasons.push(`${key}: expected text mentioning ${substrings.join(", ")}, the driver answered ${render(actual)}`);
    return;
  }
  for (const substring of substrings) {
    if (!text.includes(substring)) {
      reasons.push(`${key}: does not mention ${JSON.stringify(substring)}, it says ${JSON.stringify(text)}`);
    }
  }
}

function checkHasFix(key: string, required: boolean, actual: unknown, reasons: string[]): void {
  const fix = asRecord(actual)?.["fix"];
  const present = typeof fix === "string" && fix.trim().length > 0;
  if (required && !present) {
    reasons.push(`${key}: carries no fix, and a refusal an author cannot act on is half a refusal`);
  }
  if (!required && present) {
    reasons.push(`${key}: carries a fix where the vector expects none`);
  }
}

function checkBefore(key: string, pairs: string[][], actual: unknown, reasons: string[]): void {
  if (!Array.isArray(actual)) {
    reasons.push(`${key}: expected an ordered array, the driver answered ${render(actual)}`);
    return;
  }
  const order = actual.map((entry) => String(entry));
  for (const pair of pairs) {
    const [earlier, later] = [pair[0] as string, pair[1] as string];
    const earlierAt = order.indexOf(earlier);
    const laterAt = order.indexOf(later);
    if (earlierAt < 0 || laterAt < 0) {
      reasons.push(`${key}: ${earlier} before ${later} cannot be judged, the order is ${render(order)}`);
    } else if (earlierAt > laterAt) {
      reasons.push(`${key}: ${earlier} must come before ${later}, the order is ${render(order)}`);
    }
  }
}

function checkContains(key: string, wanted: unknown[], actual: unknown, reasons: string[]): void {
  if (!Array.isArray(actual)) {
    reasons.push(`${key}: expected an array, the driver answered ${render(actual)}`);
    return;
  }
  for (const element of wanted) {
    if (!actual.some((candidate) => deepEqual(element, candidate))) {
      reasons.push(`${key}: does not contain ${render(element)}, it is ${render(actual)}`);
    }
  }
}

function isAssertionBlock(value: unknown): value is Record<string, unknown> {
  const record = asRecord(value);
  return record !== undefined && Object.keys(record).some((key) => (ASSERTIONS as readonly string[]).includes(key));
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return typeof value === "object" && value !== null && !Array.isArray(value) ? (value as Record<string, unknown>) : undefined;
}

function textOf(value: unknown): string | undefined {
  if (typeof value === "string") {
    return value;
  }
  const record = asRecord(value);
  if (record === undefined) {
    return undefined;
  }
  const parts = ["detail", "message", "fix"].map((field) => record[field]).filter((part): part is string => typeof part === "string");
  return parts.length === 0 ? undefined : parts.join(" ");
}

function deepEqual(expected: unknown, actual: unknown): boolean {
  if (expected === actual) {
    return true;
  }
  if (Array.isArray(expected) && Array.isArray(actual)) {
    return expected.length === actual.length && expected.every((element, index) => deepEqual(element, actual[index]));
  }
  const expectedRecord = asRecord(expected);
  const actualRecord = asRecord(actual);
  if (expectedRecord === undefined || actualRecord === undefined) {
    return false;
  }
  const keys = Object.keys(expectedRecord);
  return keys.length === Object.keys(actualRecord).length && keys.every((key) => deepEqual(expectedRecord[key], actualRecord[key]));
}

function render(value: unknown): string {
  return value === undefined ? "nothing" : JSON.stringify(value);
}
