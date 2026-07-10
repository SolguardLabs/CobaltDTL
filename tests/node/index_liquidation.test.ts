import test from "node:test";
import assert from "node:assert/strict";
import { amount, byId, runFixture } from "../helpers/runner.ts";

test("index update and reserve rebalance are reflected in the vault report", () => {
  const state = runFixture("index_liquidation.json");
  const vault = byId(state.vaults, "cvUSD");

  assert.equal(vault.index, 1030000);
  assert.equal(state.metrics.reserveRebalance, 27000);
  assert.ok(vault.reserve > 900000);
  assert.ok(vault.price >= 1000000);
});

test("liquidation burns participant credits and records keeper reserve", () => {
  const state = runFixture("index_liquidation.json");
  const maker = byId(state.accounts, "maker");
  const record = byId(state.liquidations, "liq-maker-1");

  assert.equal(record.status, "applied");
  assert.equal(record.reserveIn, 90000);
  assert.ok(record.creditsBurned > 0);
  assert.equal(amount(maker.reserves, "usd"), 510000);
  assert.equal(maker.liquidatedCredits, record.creditsBurned);
});

