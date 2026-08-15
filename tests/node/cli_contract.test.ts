import test from "node:test";
import assert from "node:assert/strict";
import { runFixture, validateFixture } from "../helpers/runner.ts";

test("fixtures validate through the public CLI", () => {
  assert.equal(validateFixture("base_cycle.json"), "ok");
  assert.equal(validateFixture("index_liquidation.json"), "ok");
  assert.equal(validateFixture("deferred_queue.json"), "ok");
  assert.equal(validateFixture("policy_controls.json"), "ok");
});

test("event output is opt-in and deterministic", () => {
  const withoutEvents = runFixture("base_cycle.json");
  const withEvents = runFixture("base_cycle.json", ["--events"]);

  assert.equal(withoutEvents.events, undefined);
  assert.ok(withEvents.events);
  assert.equal(withEvents.events?.[0].type, "init");
  assert.equal(withEvents.events?.at(-1)?.type, "fee_sweep");
});
