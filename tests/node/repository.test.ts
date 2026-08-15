import { createHash } from "node:crypto";
import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import assert from "node:assert/strict";
import test from "node:test";
import { root } from "../helpers/runner.ts";

test("repository ships the approved visual identity", () => {
  const banner = readFileSync(join(root, "assets", "banner.png"));
  assert.equal(
    createHash("sha256").update(banner).digest("hex"),
    "094443e10acf91acf246f3070631660d60f4d14d5b777082b0751b33bdbf6a28",
  );
  assert.match(readFileSync(join(root, "README.md"), "utf8"), /^# CobaltDTL$/m);
});

test("documentation contract contains seven linked guides and rich diagrams", () => {
  const docs = readdirSync(join(root, "docs"))
    .filter((name) => name.endsWith(".md"))
    .sort();
  assert.deepEqual(docs, [
    "01-arquitectura.md",
    "02-modelo-economico.md",
    "03-seguridad-operativa.md",
    "04-cli-json-sdk.md",
    "05-operacion.md",
    "06-gobernanza.md",
    "07-observabilidad.md",
  ]);
  const readme = readFileSync(join(root, "README.md"), "utf8");
  for (const name of docs) assert.match(readme, new RegExp(`docs/${name.replace(".", "\\.")}`));
  const diagrams = [readme, readFileSync(join(root, "SECURITY.md"), "utf8")]
    .concat(docs.map((name) => readFileSync(join(root, "docs", name), "utf8")))
    .join("\n")
    .match(/```mermaid\b/g);
  assert.ok((diagrams?.length ?? 0) >= 18);
});

test("automation validates candidates, production, tags and releases", () => {
  const ci = readFileSync(join(root, ".github", "workflows", "ci.yml"), "utf8");
  const integrity = readFileSync(
    join(root, ".github", "workflows", "release-integrity.yml"),
    "utf8",
  );
  assert.match(ci, /os: \[ubuntu-latest, windows-latest\]/);
  assert.match(ci, /actions\/checkout@v7/);
  assert.match(ci, /actions\/setup-node@v7/);
  assert.match(integrity, /branches: \[main, production\]/);
  assert.match(integrity, /types: \[published\]/);
  assert.match(integrity, /cat-file -t/);
});
