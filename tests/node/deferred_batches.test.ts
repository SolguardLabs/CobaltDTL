import test from "node:test";
import assert from "node:assert/strict";
import { byId, runFixture } from "../helpers/runner.ts";

test("redemption delay and batch limit leave later tickets in queue", () => {
  const state = runFixture("deferred_queue.json");
  const vault = byId(state.vaults, "cvUSD");
  const alphaTicket = byId(state.tickets, "ticket-alpha-1");
  const betaTicket = byId(state.tickets, "ticket-beta-1");

  assert.equal(state.epoch, 3);
  assert.equal(alphaTicket.status, "settled");
  assert.equal(betaTicket.status, "queued");
  assert.equal(vault.pendingCredits, betaTicket.credits);
  assert.equal(state.metrics.batches, 2);
  assert.equal(state.metrics.ticketsSettled, 1);
});

test("queued accounting remains balanced after partial settlement", () => {
  const state = runFixture("deferred_queue.json");
  const line = state.reconciliation.find((item) => item.vault === "cvUSD");
  assert.ok(line);
  assert.equal(line?.balanced, true);
  assert.equal(line?.queuedCredits, 50000);
});

