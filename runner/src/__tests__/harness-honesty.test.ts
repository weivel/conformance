import { fileURLToPath } from "node:url";
import { expect, it } from "vitest";
import { runConformance } from "../cli.js";
import { runDriver } from "../driver.js";
import { match } from "../match.js";
import { report } from "../report.js";
import { loadVectors, type Vector } from "../vectors.js";
import { fakeDriver } from "./fake-driver.js";

// Six ways a conformance suite reports a pass it has not earned. None is about an engine; all are
// about the harness, and a harness that lies is worse than no harness because its output is believed.

function fixtureDir(name: string): string {
  return fileURLToPath(new URL(`fixtures/${name}`, import.meta.url));
}

function vector(id: string): Vector {
  return {
    id,
    op: "supports",
    covers: "12.1",
    given: { host: { applications: ["claude"], api: 2 } },
    then: { refusal: null },
  };
}

const THREE = [vector("supports/one"), vector("supports/two"), vector("supports/three")];
const ONE = vector("supports/only");

it("fails the run on a vector whose op is unknown", () => {
  expect(() => loadVectors(fixtureDir("unknown-op"))).toThrow(/unknown op 'reticulate'/);
});

it("fails the run on two vectors with the same id", () => {
  expect(() => loadVectors(fixtureDir("duplicate-ids"))).toThrow(/duplicate vector id/);
});

it("errors every unanswered vector when the driver dies mid-stream", async () => {
  const outcomes = await runDriver(fakeDriver({ answerOnly: 1, exitCode: 1 }), THREE, 5000);
  const results = THREE.map((candidate) => match(candidate, outcomes.get(candidate.id)));
  expect(results.filter((result) => result.status === "errored")).toHaveLength(2);
  expect(report(results).exitCode).toBe(1);
});

it("matches answers by id, not by position", async () => {
  const outcomes = await runDriver(fakeDriver({ reverse: true }), THREE, 5000);
  expect(THREE.every((candidate) => outcomes.get(candidate.id)?.id === candidate.id)).toBe(true);
});

it("kills a driver that hangs and fails the run", async () => {
  const results = await runConformance(fakeDriver({ hang: true }), THREE, 250);
  expect(results.exitCode).toBe(1);
  expect(results.text).toMatch(/timed out after 250 ms/);
});

it("reports an unwitnessed vector as its own row, never as a pass", () => {
  const results = [match(ONE, { id: ONE.id, witnessed: false, result: {} })];
  expect(results[0]?.status).toBe("unwitnessed");
  expect(report(results).exitCode).toBe(1);
});

it("reports a line that is not an outcome rather than ignoring it", async () => {
  const results = await runConformance(fakeDriver({ malformed: true }), THREE, 5000);
  expect(results.text).toMatch(/answered a line that is not an outcome/);
  expect(results.exitCode).toBe(1);
});
