import assert from "node:assert/strict";
import test from "node:test";
import {
  CobaltClient,
  CobaltHTTPError,
  canonicalJSON,
  computeCapitalMetrics,
  payloadHash,
} from "../../sdk/cobaltClient.ts";

test("capital preview matches conservative integer rounding", () => {
  const metrics = computeCapitalMetrics({
    reserve: 900n,
    liquidReserve: 50n,
    issuedCredits: 1_000n,
    pendingCredits: 100n,
    index: 1_000_000n,
    reserveHaircutBps: 1_000n,
    redemptionShockBps: 5_000n,
    operationalBufferBps: 100n,
  });
  assert.deepEqual(metrics, {
    liability: 1_000n,
    queuedLiability: 100n,
    effectiveReserve: 810n,
    stressedOutflows: 150n,
    operationalBuffer: 10n,
    requiredCapital: 1_160n,
    capitalDeficit: 350n,
    coverageBps: 6_982n,
    liquidityBps: 3_333n,
  });
});

test("canonical payloads use stable ordering and decimal amounts", () => {
  const value = { vault: "cvUSD", amount: 25_000n, account: "operations-eu" };
  const encoded = canonicalJSON(value);
  assert.equal(encoded, '{"account":"operations-eu","amount":"25000","vault":"cvUSD"}');
  assert.equal(payloadHash(value).length, 64);
});

test("client enforces endpoint transport policy", () => {
  assert.throws(() => new CobaltClient("http://vaults.example"), /requires HTTPS/);
  assert.throws(
    () => new CobaltClient("https://user:secret@vaults.example"),
    /cannot include credentials/,
  );
  assert.doesNotThrow(
    () => new CobaltClient("http://127.0.0.1:8080", { allowInsecureLocalhost: true }),
  );
});

test("redemption requests carry canonical body and idempotency", async () => {
  let observed: { url: string; init: RequestInit } | undefined;
  const client = new CobaltClient("https://vaults.example/api", {
    fetchImpl: async (input, init = {}) => {
      observed = { url: String(input), init };
      return new Response(JSON.stringify({ accepted: true }), {
        headers: { "content-type": "application/json" },
      });
    },
  });
  const response = await client.requestRedemption<{ accepted: boolean }>(
    {
      account: "alpha",
      vault: "cvUSD",
      credits: 50_000n,
      minPayout: 49_000n,
      reference: "redeem-001",
    },
    "idem-redeem-001",
  );
  assert.equal(response.accepted, true);
  assert.equal(observed?.url, "https://vaults.example/api/v1/redemptions");
  assert.equal(new Headers(observed?.init.headers).get("idempotency-key"), "idem-redeem-001");
  assert.equal(observed?.init.redirect, "error");
  assert.equal(observed?.init.credentials, "omit");
  assert.match(String(observed?.init.body), /"credits":"50000"/);
});

test("client rejects invalid response envelopes", async () => {
  const textClient = new CobaltClient("https://vaults.example", {
    fetchImpl: async () => new Response("ok", { headers: { "content-type": "text/plain" } }),
  });
  await assert.rejects(() => textClient.snapshot(), /unexpected response content type/);

  const largeClient = new CobaltClient("https://vaults.example", {
    maxResponseBytes: 1_024,
    fetchImpl: async () =>
      new Response(JSON.stringify({ value: "x".repeat(2_000) }), {
        headers: { "content-type": "application/json" },
      }),
  });
  await assert.rejects(() => largeClient.capital(), /size limit/);
});

test("client exposes structured HTTP failures", async () => {
  const client = new CobaltClient("https://vaults.example", {
    fetchImpl: async () =>
      new Response(JSON.stringify({ code: "policy" }), {
        status: 422,
        statusText: "Unprocessable Content",
        headers: { "content-type": "application/json" },
      }),
  });
  await assert.rejects(
    () => client.snapshot(),
    (error: unknown) => error instanceof CobaltHTTPError && error.status === 422,
  );
});

test("capital input validation fails closed", () => {
  assert.throws(
    () =>
      computeCapitalMetrics({
        reserve: 1n,
        liquidReserve: 2n,
        issuedCredits: 1n,
        pendingCredits: 0n,
        index: 1_000_000n,
        reserveHaircutBps: 0n,
        redemptionShockBps: 0n,
        operationalBufferBps: 0n,
      }),
    /liquid reserve/,
  );
});
