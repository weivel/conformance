import { expect, it } from "vitest";
import { runDriver } from "../driver.js";
import { match } from "../match.js";
import { report } from "../report.js";
import { loadVectors, VECTORS_PATH } from "../vectors.js";
import { fakeDriver } from "./fake-driver.js";

const REFUSAL = {
  detail: "studio-thing is built for studio, and this host hosts claude",
  fix: "add claude to targets, or remove targets to mean every application",
};

it("passes a vector a driver answers correctly and fails one it does not", async () => {
  const vectors = loadVectors(VECTORS_PATH).filter((vector) => vector.id === "supports/refuses-another-application");
  expect(vectors).toHaveLength(1);

  const passing = await runDriver(fakeDriver({ answer: { refusal: REFUSAL } }), vectors, 5000);
  expect(report(vectors.map((vector) => match(vector, passing.get(vector.id)))).exitCode).toBe(0);

  const failing = await runDriver(fakeDriver({ answer: { refusal: null } }), vectors, 5000);
  const failedReport = report(vectors.map((vector) => match(vector, failing.get(vector.id))));
  expect(failedReport.exitCode).toBe(1);
  expect(failedReport.text).toContain("FAILED supports/refuses-another-application");
});
