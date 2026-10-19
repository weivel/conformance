import { expect, it } from "vitest";
import { loadVectors, OPS, VECTORS_PATH } from "../vectors.js";

// A family that silently empties leaves a behaviour nothing asks about, and the report still reads
// complete because it only ever shows the rows it has.
const CORPUS = loadVectors(VECTORS_PATH);

const SECTIONS = ["12.1", "12.2", "12.3", "12.4", "12.5", "12.6"];

it("asks about every op", () => {
  expect([...new Set(CORPUS.map((vector) => vector.op))].sort()).toEqual([...OPS].sort());
});

it("covers every section of the design the kit exists to pin", () => {
  expect([...new Set(CORPUS.map((vector) => vector.covers))].sort()).toEqual(SECTIONS);
});
