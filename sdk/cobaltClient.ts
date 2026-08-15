import { createHash, randomUUID } from "node:crypto";

export const BPS = 10_000n;
export const SCALE = 1_000_000n;

export type ClientOptions = {
  fetchImpl?: typeof fetch;
  timeoutMs?: number;
  maxResponseBytes?: number;
  allowInsecureLocalhost?: boolean;
};

export type CapitalInput = {
  reserve: bigint;
  liquidReserve: bigint;
  issuedCredits: bigint;
  pendingCredits: bigint;
  index: bigint;
  reserveHaircutBps: bigint;
  redemptionShockBps: bigint;
  operationalBufferBps: bigint;
};

export type CapitalMetrics = {
  liability: bigint;
  queuedLiability: bigint;
  effectiveReserve: bigint;
  stressedOutflows: bigint;
  operationalBuffer: bigint;
  requiredCapital: bigint;
  capitalDeficit: bigint;
  coverageBps: bigint;
  liquidityBps: bigint;
};

export type RedemptionRequest = {
  account: string;
  vault: string;
  credits: bigint;
  minPayout: bigint;
  reference: string;
};

export class CobaltClient {
  readonly #baseURL: URL;
  readonly #fetch: typeof fetch;
  readonly #timeoutMs: number;
  readonly #maxResponseBytes: number;

  constructor(baseURL: string, options: ClientOptions = {}) {
    const parsed = new URL(baseURL);
    const local =
      parsed.hostname === "localhost" ||
      parsed.hostname === "127.0.0.1" ||
      parsed.hostname === "::1";
    if (parsed.protocol !== "https:" && !(local && options.allowInsecureLocalhost)) {
      throw new Error(
        "CobaltClient requires HTTPS outside an explicitly allowed local environment",
      );
    }
    if (parsed.username || parsed.password || parsed.search || parsed.hash) {
      throw new Error("CobaltClient base URL cannot include credentials, query, or fragment");
    }
    parsed.pathname = parsed.pathname.replace(/\/+$/, "") + "/";
    this.#baseURL = parsed;
    this.#fetch = options.fetchImpl ?? fetch;
    this.#timeoutMs = boundedInteger(options.timeoutMs ?? 8_000, 100, 60_000, "timeoutMs");
    this.#maxResponseBytes = boundedInteger(
      options.maxResponseBytes ?? 1_000_000,
      1_024,
      8_000_000,
      "maxResponseBytes",
    );
  }

  snapshot<T = unknown>(signal?: AbortSignal): Promise<T> {
    return this.#request<T>("v1/snapshot", { method: "GET" }, undefined, signal);
  }

  capital<T = unknown>(signal?: AbortSignal): Promise<T> {
    return this.#request<T>("v1/capital", { method: "GET" }, undefined, signal);
  }

  requestRedemption<T = unknown>(
    request: RedemptionRequest,
    idempotencyKey: string = randomUUID(),
  ): Promise<T> {
    const body = {
      account: normalizeToken(request.account, "account"),
      credits: positiveDecimal(request.credits, "credits"),
      min_payout: nonNegativeDecimal(request.minPayout, "minimum payout"),
      reference: normalizeToken(request.reference, "reference"),
      vault: normalizeToken(request.vault, "vault"),
    };
    return this.#request<T>(
      "v1/redemptions",
      { method: "POST", body: canonicalJSON(body) },
      normalizeToken(idempotencyKey, "idempotency key"),
    );
  }

  async #request<T>(
    path: string,
    init: RequestInit,
    idempotencyKey?: string,
    signal?: AbortSignal,
  ): Promise<T> {
    const url = new URL(path, this.#baseURL);
    if (url.origin !== this.#baseURL.origin || !url.pathname.startsWith(this.#baseURL.pathname)) {
      throw new Error("request path escapes the configured CobaltDTL endpoint");
    }
    const controller = new AbortController();
    const timeout = setTimeout(
      () => controller.abort(new Error("request timeout")),
      this.#timeoutMs,
    );
    const relayAbort = () => controller.abort(signal?.reason);
    signal?.addEventListener("abort", relayAbort, { once: true });
    try {
      const headers = new Headers(init.headers);
      headers.set("accept", "application/json");
      headers.set("content-type", "application/json");
      if (idempotencyKey) headers.set("idempotency-key", idempotencyKey);
      const response = await this.#fetch(url, {
        ...init,
        headers,
        cache: "no-store",
        credentials: "omit",
        redirect: "error",
        signal: controller.signal,
      });
      const contentType = response.headers.get("content-type")?.toLowerCase() ?? "";
      if (!contentType.startsWith("application/json")) {
        throw new Error(`unexpected response content type: ${contentType || "missing"}`);
      }
      const declared = Number(response.headers.get("content-length") ?? "0");
      if (Number.isFinite(declared) && declared > this.#maxResponseBytes) {
        throw new Error("response exceeds configured size limit");
      }
      const text = await response.text();
      if (Buffer.byteLength(text, "utf8") > this.#maxResponseBytes) {
        throw new Error("response exceeds configured size limit");
      }
      const body = text === "" ? null : (JSON.parse(text) as unknown);
      if (!response.ok) throw new CobaltHTTPError(response.status, response.statusText, body);
      return body as T;
    } finally {
      clearTimeout(timeout);
      signal?.removeEventListener("abort", relayAbort);
    }
  }
}

export class CobaltHTTPError extends Error {
  readonly status: number;
  readonly statusText: string;
  readonly body: unknown;

  constructor(status: number, statusText: string, body: unknown) {
    super(`CobaltDTL request failed with HTTP ${status}${statusText ? ` ${statusText}` : ""}`);
    this.name = "CobaltHTTPError";
    this.status = status;
    this.statusText = statusText;
    this.body = body;
  }
}

export function computeCapitalMetrics(input: CapitalInput): CapitalMetrics {
  for (const [field, value] of Object.entries(input)) {
    if (value < 0n) throw new Error(`${field} must be non-negative`);
  }
  if (input.index <= 0n) throw new Error("index must be positive");
  if (input.liquidReserve > input.reserve) throw new Error("liquid reserve exceeds total reserve");
  if (input.pendingCredits > input.issuedCredits)
    throw new Error("pending credits exceed issued credits");
  if (input.reserveHaircutBps > BPS) throw new Error("reserve haircut exceeds 10000 bps");

  const liability = mulDivCeil(input.issuedCredits, input.index, SCALE);
  const queuedLiability = mulDivCeil(input.pendingCredits, input.index, SCALE);
  const effectiveReserve = mulDivFloor(input.reserve, BPS - input.reserveHaircutBps, BPS);
  const stressedOutflows = mulDivCeil(queuedLiability, BPS + input.redemptionShockBps, BPS);
  const operationalBuffer = mulDivCeil(liability, input.operationalBufferBps, BPS);
  const requiredCapital = liability + stressedOutflows + operationalBuffer;
  return {
    liability,
    queuedLiability,
    effectiveReserve,
    stressedOutflows,
    operationalBuffer,
    requiredCapital,
    capitalDeficit: requiredCapital > effectiveReserve ? requiredCapital - effectiveReserve : 0n,
    coverageBps: ratioBps(effectiveReserve, requiredCapital),
    liquidityBps: ratioBps(input.liquidReserve, stressedOutflows),
  };
}

export function canonicalJSON(value: unknown): string {
  return JSON.stringify(sortValue(value));
}

export function payloadHash(value: unknown): string {
  return createHash("sha256").update(canonicalJSON(value), "utf8").digest("hex");
}

function sortValue(value: unknown): unknown {
  if (typeof value === "bigint") return value.toString(10);
  if (Array.isArray(value)) return value.map(sortValue);
  if (value !== null && typeof value === "object") {
    return Object.fromEntries(
      Object.entries(value as Record<string, unknown>)
        .sort(([left], [right]) => left.localeCompare(right))
        .map(([key, entry]) => [key, sortValue(entry)]),
    );
  }
  if (typeof value === "number" && !Number.isSafeInteger(value)) {
    throw new Error("canonical JSON accepts only safe integer numbers");
  }
  return value;
}

function mulDivFloor(value: bigint, numerator: bigint, denominator: bigint): bigint {
  if (denominator <= 0n) throw new Error("denominator must be positive");
  return (value * numerator) / denominator;
}

function mulDivCeil(value: bigint, numerator: bigint, denominator: bigint): bigint {
  if (denominator <= 0n) throw new Error("denominator must be positive");
  const product = value * numerator;
  return product === 0n ? 0n : (product + denominator - 1n) / denominator;
}

function ratioBps(numerator: bigint, denominator: bigint): bigint {
  if (denominator === 0n) return numerator === 0n ? 0n : (1n << 255n) - 1n;
  return mulDivFloor(numerator, BPS, denominator);
}

function positiveDecimal(value: bigint, field: string): string {
  if (value <= 0n) throw new Error(`${field} must be positive`);
  return value.toString(10);
}

function nonNegativeDecimal(value: bigint, field: string): string {
  if (value < 0n) throw new Error(`${field} must be non-negative`);
  return value.toString(10);
}

function normalizeToken(value: string, field: string): string {
  const normalized = value.trim();
  if (normalized === "" || normalized !== value || /\s/.test(value) || value.length > 128) {
    throw new Error(`${field} must be normalized, non-empty, and at most 128 characters`);
  }
  return value;
}

function boundedInteger(value: number, minimum: number, maximum: number, field: string): number {
  if (!Number.isInteger(value) || value < minimum || value > maximum) {
    throw new Error(`${field} must be an integer within [${minimum}, ${maximum}]`);
  }
  return value;
}
