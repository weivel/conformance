import { readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { Ajv, type ErrorObject } from "ajv";

export const OPS = ["supports", "boot", "order", "resolve", "revoke", "validate"] as const;

export type Op = (typeof OPS)[number];

export interface Vector {
  id: string;
  op: Op;
  covers: string;
  given: Record<string, unknown>;
  then: Record<string, unknown>;
}

export const SCHEMA_PATH = fileURLToPath(new URL("../../vector.schema.json", import.meta.url));

export const VECTORS_PATH = fileURLToPath(new URL("../../vectors", import.meta.url));

/**
 * Reads every vector under a directory, rejecting a corpus the runner cannot be trusted to report
 * on: one that fails its own schema, declares an id twice, or names a family in its id that is not
 * the op it runs. Returns them ordered by id so a report does not depend on the file system's
 * ordering.
 */
export function loadVectors(directory: string): Vector[] {
  const validate = schemaValidator();
  const seen = new Map<string, string>();
  const vectors: Vector[] = [];
  for (const file of jsonFilesUnder(directory)) {
    const vector = parse(file);
    if (!validate(vector)) {
      throw new Error(`${file} is not a valid vector: ${ajvErrors(validate.errors)}`);
    }
    const valid = vector as Vector;
    const previous = seen.get(valid.id);
    if (previous !== undefined) {
      throw new Error(`vector id ${valid.id} is declared twice, in ${previous} and in ${file}`);
    }
    if (familyOf(valid.id) !== valid.op) {
      throw new Error(`vector ${valid.id} in ${file} runs op ${valid.op}, so its id must start ${valid.op}/`);
    }
    seen.set(valid.id, file);
    vectors.push(valid);
  }
  return vectors.sort((left, right) => left.id.localeCompare(right.id));
}

function schemaValidator() {
  const ajv = new Ajv({ allErrors: true });
  return ajv.compile(JSON.parse(readFileSync(SCHEMA_PATH, "utf8")));
}

function ajvErrors(errors: ErrorObject[] | null | undefined): string {
  return (errors ?? []).map((error) => `${error.instancePath || "/"} ${error.message}`).join("; ");
}

function parse(file: string): unknown {
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch (cause) {
    throw new Error(`${file} is not readable JSON: ${(cause as Error).message}`);
  }
}

function familyOf(id: string): string {
  return id.slice(0, id.indexOf("/"));
}

function jsonFilesUnder(directory: string): string[] {
  const found: string[] = [];
  for (const entry of readdirSync(directory).sort()) {
    const path = join(directory, entry);
    if (statSync(path).isDirectory()) {
      found.push(...jsonFilesUnder(path));
    } else if (entry.endsWith(".json")) {
      found.push(path);
    }
  }
  return found;
}
