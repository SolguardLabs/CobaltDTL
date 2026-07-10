import test from "node:test";
import assert from "node:assert/strict";
import { byId, runFixture } from "../helpers/runner.ts";

test("multi-vault policy fixture reports each reserve independently", () => {
  const state = runFixture("policy_controls.json");
  const usd = byId(state.vaults, "cvUSD");
  const eur = byId(state.vaults, "cvEUR");

  assert.equal(state.metrics.deposits, 550000);
  assert.equal(usd.asset, "usd");
  assert.equal(eur.asset, "eur");
  assert.equal(usd.policy?.depositCap, 900000);
  assert.equal(eur.policy?.depositCap, 800000);
  assert.ok(usd.protocolFees > 0);
  assert.ok(eur.protocolFees > 0);
});

test("all reconciliation lines are balanced after deposits", () => {
  const state = runFixture("policy_controls.json");
  assert.equal(state.reconciliation.length, 2);
  assert.ok(state.reconciliation.every((line) => line.balanced));
});
