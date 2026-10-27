import { readFileSync } from "node:fs";
import { Ajv } from "ajv";
import { expect, it } from "vitest";
import { SCHEMA_PATH } from "../vectors.js";

const validate = new Ajv({ allErrors: true }).compile(JSON.parse(readFileSync(SCHEMA_PATH, "utf8")));

function vectorCovering(covers: string): unknown {
  return { id: "reach/example", op: "reach", covers, given: {}, then: {} };
}

it("admits every invariant weivel/model states", () => {
  for (let invariant = 1; invariant <= 12; invariant += 1) {
    expect(validate(vectorCovering(`invariant-${invariant}`)), `invariant-${invariant}`).toBe(true);
  }
});

it("refuses an invariant weivel/model does not state", () => {
  expect(validate(vectorCovering("invariant-13"))).toBe(false);
  expect(validate(vectorCovering("invariant-0"))).toBe(false);
});
