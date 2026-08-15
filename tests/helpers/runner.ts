import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export type AmountEntry = {
  id: string;
  amount: number;
};

export type AccountReport = {
  id: string;
  reserves: AmountEntry[];
  credits: AmountEntry[];
  feesPaid: number;
  reserveOut: number;
  reserveIn: number;
  liquidatedCredits: number;
};

export type VaultReport = {
  id: string;
  asset: string;
  index: number;
  reserve: number;
  issuedCredits: number;
  pendingCredits: number;
  activeCredits: number;
  price: number;
  protocolFees: number;
  lastPrice: number;
  policy?: {
    depositCap: number;
    minDeposit: number;
    maxTicketCredits: number;
    maxBatchPayout: number;
    batchLimit: number;
  };
};

export type LiquidationReport = {
  id: string;
  vault: string;
  account: string;
  reserveIn: number;
  creditsBurned: number;
  penaltyCredits: number;
  price: number;
  epoch: number;
  status: string;
};

export type TicketReport = {
  id: string;
  vault: string;
  account: string;
  credits: number;
  payout: number;
  fee: number;
  status: string;
};

export type CobaltReport = {
  name: string;
  epoch: number;
  vaults: VaultReport[];
  accounts: AccountReport[];
  tickets: TicketReport[];
  liquidations: LiquidationReport[];
  metrics: Record<string, number>;
  reconciliation: Array<{
    vault: string;
    accountCredits: number;
    queuedCredits: number;
    issuedCredits: number;
    balanced: boolean;
  }>;
  events?: Array<Record<string, string | number>>;
};

export const root = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const binary = join(
  root,
  "out",
  process.platform === "win32" ? "cobaltdtl.exe" : "cobaltdtl",
);

export function ensureBuilt(): void {
  if (existsSync(binary)) {
    return;
  }
  const result = spawnSync(process.execPath, ["scripts/build.mjs"], {
    cwd: root,
    encoding: "utf8",
  });
  if (result.status !== 0) {
    throw new Error(result.stderr || result.stdout || "build failed");
  }
}

export function runCli(args: string[]): string {
  ensureBuilt();
  const result = spawnSync(binary, args, {
    cwd: root,
    encoding: "utf8",
  });
  if (result.status !== 0) {
    throw new Error(`command failed: ${binary} ${args.join(" ")}\n${result.stderr}`);
  }
  return result.stdout;
}

export function runFixture(name: string, options: string[] = []): CobaltReport {
  const fixture = join("tests", "fixtures", name);
  return JSON.parse(runCli(["run", fixture, "--json", ...options])) as CobaltReport;
}

export function validateFixture(name: string): string {
  const fixture = join("tests", "fixtures", name);
  return runCli(["validate", fixture]).trim();
}

export function byId<T extends { id: string }>(collection: T[], id: string): T {
  const found = collection.find((item) => item.id === id);
  if (!found) {
    throw new Error(`missing id ${id}`);
  }
  return found;
}

export function amount(entries: AmountEntry[], id: string): number {
  return byId(entries, id).amount;
}
