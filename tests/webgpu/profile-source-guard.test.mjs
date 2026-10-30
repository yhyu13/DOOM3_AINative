// Source guard for the Q7b host slice. Kept separate from profile.test.mjs so
// the profile's functional test file stays free of native-only tokens.

import { test } from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { PROCEDURAL_WGSL } from "../../web/profile/scene.mjs";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, "..", "..");
const PROFILE_DIR = path.join(ROOT, "web", "profile");

// Split so this guard file itself cannot satisfy a plain-text search for the
// tokens it forbids in the product sources.
const BANNED = new RegExp(
  ["dx" + "r", "dl" + "ss", "d3d" + "12", "dx" + "il", "ID3D" + "12", "hres" + "ult", "ray" + "Query"].join("|"),
  "i",
);

test("profile sources contain no native-only vocabulary or absolute machine paths", () => {
  const files = fs.readdirSync(PROFILE_DIR).filter((name) => name.endsWith(".mjs") || name.endsWith(".html"));
  assert.ok(files.length >= 9, "expected the profile modules and harness page");
  for (const file of files) {
    const text = fs.readFileSync(path.join(PROFILE_DIR, file), "utf8");
    assert.doesNotMatch(text, BANNED, file);
    assert.doesNotMatch(text, /[A-Za-z]:\\/, file);
  }
});

test("procedural shader is a fullscreen pass and contains no forbidden vocabulary", () => {
  assert.match(PROCEDURAL_WGSL, /@vertex/);
  assert.match(PROCEDURAL_WGSL, /@fragment/);
  assert.match(PROCEDURAL_WGSL, /positions\[vertexIndex\]/);
  assert.doesNotMatch(PROCEDURAL_WGSL, BANNED);
});
