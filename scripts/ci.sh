#!/usr/bin/env bash
set -euo pipefail

node scripts/build.mjs --warnings
node --test --experimental-strip-types "tests/node/*.test.ts"

