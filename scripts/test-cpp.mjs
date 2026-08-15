import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const build = spawnSync(process.execPath, ["scripts/build.mjs", "--warnings", "--unit-tests"], {
  cwd: root,
  encoding: "utf8",
  stdio: "inherit",
});
if (build.status !== 0) process.exit(build.status ?? 1);

const binary = join(
  root,
  "out",
  process.platform === "win32" ? "cobaltdtl-tests.exe" : "cobaltdtl-tests",
);
const result = spawnSync(binary, [], { cwd: root, encoding: "utf8", stdio: "inherit" });
process.exit(result.status ?? 1);
