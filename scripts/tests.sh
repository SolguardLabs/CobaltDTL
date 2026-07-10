#!/usr/bin/env bash
set -euo pipefail

node scripts/build.mjs
node --test --experimental-strip-types "tests/node/*.test.ts"

