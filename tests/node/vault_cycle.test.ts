import test from "node:test";
import assert from "node:assert/strict";
import { amount, byId, runFixture } from "../helpers/runner.ts";

test("base cycle mints credits, queues one ticket and sweeps accrued fees", () => {
  const state = runFixture("base_cycle.json");
  const vault = byId(state.vaults, "cvUSD");
  const alice = byId(state.accounts, "alice");
  const bob = byId(state.accounts, "bob");
  const treasury = byId(state.accounts, "treasury");
  const ticket = byId(state.tickets, "ticket-bob-1");

  assert.equal(state.epoch, 2);
  assert.equal(ticket.status, "settled");
  assert.equal(ticket.credits, 75000);
  assert.ok(ticket.payout > 0);

  assert.equal(amount(alice.credits, "cvUSD"), 449000);
  assert.ok(amount(bob.credits, "cvUSD") > 150000);
  assert.equal(vault.pendingCredits, 0);
  assert.equal(state.metrics.ticketsSettled, 1);
  assert.equal(amount(treasury.reserves, "usd"), state.metrics.feesSwept);
});

test("reconciliation tracks active account credits plus queued tickets", () => {
  const state = runFixture("base_cycle.json");
  const line = state.reconciliation.find((item) => item.vault === "cvUSD");
  assert.ok(line);
  assert.equal(line?.balanced, true);
  assert.equal(line?.issuedCredits, line?.accountCredits);
});

