#include "report.hpp"

#include "analytics.hpp"
#include "queue.hpp"

namespace cobaltdtl {

namespace {

void writeAmountMap(JsonWriter& writer, const std::map<std::string, Amount>& values, std::string_view keyName) {
    writer.key(keyName);
    writer.beginArray();
    for (const auto& item : values) {
        writer.beginObject();
        writer.key("id");
        writer.value(item.first);
        writer.key("amount");
        writer.value(item.second);
        writer.endObject();
    }
    writer.endArray();
}

void writeAssets(JsonWriter& writer, const Ledger& ledger) {
    writer.key("assets");
    writer.beginArray();
    for (const auto& item : ledger.assets()) {
        const AssetDef& asset = item.second;
        writer.beginObject();
        writer.key("id");
        writer.value(asset.id);
        writer.key("symbol");
        writer.value(asset.symbol);
        writer.key("decimals");
        writer.value(static_cast<Amount>(asset.decimals));
        writer.key("haircutBps");
        writer.value(asset.haircutBps);
        writer.key("dust");
        writer.value(asset.dust);
        writer.key("vaultReserve");
        writer.value(ledger.reserveByAsset(asset.id));
        writer.endObject();
    }
    writer.endArray();
}

void writePolicy(JsonWriter& writer, const VaultPolicy& policy) {
    writer.key("policy");
    writer.beginObject();
    writer.key("depositCap");
    writer.value(policy.depositCap);
    writer.key("minDeposit");
    writer.value(policy.minDeposit);
    writer.key("maxTicketCredits");
    writer.value(policy.maxTicketCredits);
    writer.key("minLiquidity");
    writer.value(policy.minLiquidity);
    writer.key("maxBatchPayout");
    writer.value(policy.maxBatchPayout);
    writer.key("mintFeeBps");
    writer.value(policy.mintFeeBps);
    writer.key("redemptionFeeBps");
    writer.value(policy.redemptionFeeBps);
    writer.key("liquidationPenaltyBps");
    writer.value(policy.liquidationPenaltyBps);
    writer.key("redemptionDelay");
    writer.value(static_cast<Amount>(policy.redemptionDelay));
    writer.key("batchLimit");
    writer.value(static_cast<Amount>(policy.batchLimit));
    writer.key("allowDeposits");
    writer.value(policy.allowDeposits);
    writer.key("allowRedemptions");
    writer.value(policy.allowRedemptions);
    writer.key("allowLiquidations");
    writer.value(policy.allowLiquidations);
    writer.endObject();
}

void writeVaults(JsonWriter& writer, const Ledger& ledger) {
    PricingService pricing;
    RiskEngine risk(pricing);
    writer.key("vaults");
    writer.beginArray();
    for (const auto& item : ledger.vaults()) {
        const VaultState& vault = item.second;
        const PriceSnapshot price = pricing.snapshot(vault);
        const RiskReport riskReport = risk.assess(vault);
        writer.beginObject();
        writer.key("id");
        writer.value(vault.id);
        writer.key("asset");
        writer.value(vault.assetId);
        writer.key("label");
        writer.value(vault.label);
        writer.key("index");
        writer.value(vault.index);
        writer.key("reserve");
        writer.value(vault.realReserve);
        writer.key("issuedCredits");
        writer.value(vault.issuedCredits);
        writer.key("pendingCredits");
        writer.value(vault.pendingCredits);
        writer.key("activeCredits");
        writer.value(price.activeCredits);
        writer.key("price");
        writer.value(price.price);
        writer.key("indexedLiability");
        writer.value(price.indexedLiability);
        writer.key("reserveRatioBps");
        writer.value(price.reserveRatioBps);
        writer.key("protocolFees");
        writer.value(vault.protocolFees);
        writer.key("insuranceBuffer");
        writer.value(vault.insuranceBuffer);
        writer.key("lastPrice");
        writer.value(vault.lastPrice);
        writer.key("risk");
        writer.beginObject();
        writer.key("depositsOpen");
        writer.value(riskReport.depositsOpen);
        writer.key("redemptionsOpen");
        writer.value(riskReport.redemptionsOpen);
        writer.key("liquidationsOpen");
        writer.value(riskReport.liquidationsOpen);
        writer.key("liquidityFloorMet");
        writer.value(riskReport.liquidityFloorMet);
        writer.key("notes");
        writer.beginArray();
        for (const std::string& note : riskReport.notes) {
            writer.value(note);
        }
        writer.endArray();
        writer.endObject();
        writePolicy(writer, vault.policy);
        writer.endObject();
    }
    writer.endArray();
}

void writeAccounts(JsonWriter& writer, const Ledger& ledger) {
    writer.key("accounts");
    writer.beginArray();
    for (const auto& item : ledger.accounts()) {
        const AccountState& account = item.second;
        writer.beginObject();
        writer.key("id");
        writer.value(account.id);
        writeAmountMap(writer, account.reserves, "reserves");
        writeAmountMap(writer, account.credits, "credits");
        writer.key("feesPaid");
        writer.value(account.feesPaid);
        writer.key("reserveOut");
        writer.value(account.reserveOut);
        writer.key("reserveIn");
        writer.value(account.reserveIn);
        writer.key("liquidatedCredits");
        writer.value(account.liquidatedCredits);
        writer.endObject();
    }
    writer.endArray();
}

void writeTickets(JsonWriter& writer, const Ledger& ledger) {
    writer.key("tickets");
    writer.beginArray();
    for (const RedemptionTicket& ticket : ledger.tickets()) {
        writer.beginObject();
        writer.key("id");
        writer.value(ticket.id);
        writer.key("vault");
        writer.value(ticket.vaultId);
        writer.key("account");
        writer.value(ticket.accountId);
        writer.key("credits");
        writer.value(ticket.credits);
        writer.key("minPayout");
        writer.value(ticket.minPayout);
        writer.key("payout");
        writer.value(ticket.payout);
        writer.key("fee");
        writer.value(ticket.fee);
        writer.key("quotedIndex");
        writer.value(ticket.quotedIndex);
        writer.key("settlementPrice");
        writer.value(ticket.settlementPrice);
        writer.key("requestedEpoch");
        writer.value(ticket.requestedEpoch);
        writer.key("unlockEpoch");
        writer.value(ticket.unlockEpoch);
        writer.key("status");
        writer.value(ticketStatusName(ticket.status));
        writer.endObject();
    }
    writer.endArray();
}

void writeLiquidations(JsonWriter& writer, const Ledger& ledger) {
    writer.key("liquidations");
    writer.beginArray();
    for (const LiquidationRecord& record : ledger.liquidations()) {
        writer.beginObject();
        writer.key("id");
        writer.value(record.id);
        writer.key("vault");
        writer.value(record.vaultId);
        writer.key("account");
        writer.value(record.accountId);
        writer.key("reserveIn");
        writer.value(record.reserveIn);
        writer.key("creditsBurned");
        writer.value(record.creditsBurned);
        writer.key("penaltyCredits");
        writer.value(record.penaltyCredits);
        writer.key("price");
        writer.value(record.price);
        writer.key("epoch");
        writer.value(record.epoch);
        writer.key("status");
        writer.value(liquidationStatusName(record.status));
        writer.endObject();
    }
    writer.endArray();
}

void writeMetrics(JsonWriter& writer, const Ledger& ledger) {
    const Metrics& metrics = ledger.metrics();
    writer.key("metrics");
    writer.beginObject();
    writer.key("deposits");
    writer.value(metrics.deposits);
    writer.key("mintedCredits");
    writer.value(metrics.mintedCredits);
    writer.key("redemptionsQueued");
    writer.value(metrics.redemptionsQueued);
    writer.key("redemptionPayout");
    writer.value(metrics.redemptionPayout);
    writer.key("redemptionFees");
    writer.value(metrics.redemptionFees);
    writer.key("liquidations");
    writer.value(metrics.liquidations);
    writer.key("liquidatedCredits");
    writer.value(metrics.liquidatedCredits);
    writer.key("reserveRebalance");
    writer.value(metrics.reserveRebalance);
    writer.key("transfers");
    writer.value(metrics.transfers);
    writer.key("feesSwept");
    writer.value(metrics.feesSwept);
    writer.key("batches");
    writer.value(static_cast<Amount>(metrics.batches));
    writer.key("ticketsSettled");
    writer.value(static_cast<Amount>(metrics.ticketsSettled));
    writer.key("ticketsDeferred");
    writer.value(static_cast<Amount>(metrics.ticketsDeferred));
    writer.endObject();
}

void writeReconciliation(JsonWriter& writer, const Ledger& ledger) {
    writer.key("reconciliation");
    writer.beginArray();
    for (const ReconciliationLine& line : ledger.reconcile()) {
        writer.beginObject();
        writer.key("vault");
        writer.value(line.vaultId);
        writer.key("accountCredits");
        writer.value(line.accountCredits);
        writer.key("queuedCredits");
        writer.value(line.queuedCredits);
        writer.key("issuedCredits");
        writer.value(line.issuedCredits);
        writer.key("reserve");
        writer.value(line.reserve);
        writer.key("balanced");
        writer.value(checkedAdd(line.accountCredits, line.queuedCredits, "reconciliation") == line.issuedCredits);
        writer.endObject();
    }
    writer.endArray();
}

void writeEvents(JsonWriter& writer, const Ledger& ledger) {
    writer.key("events");
    writer.beginArray();
    for (const Event& event : ledger.events()) {
        writer.beginObject();
        writer.key("seq");
        writer.value(static_cast<Amount>(event.seq));
        writer.key("epoch");
        writer.value(event.epoch);
        writer.key("type");
        writer.value(event.type);
        writer.key("account");
        writer.value(event.accountId);
        writer.key("vault");
        writer.value(event.vaultId);
        writer.key("asset");
        writer.value(event.assetId);
        writer.key("amount");
        writer.value(event.amount);
        writer.key("credits");
        writer.value(event.credits);
        writer.key("note");
        writer.value(event.note);
        writer.endObject();
    }
    writer.endArray();
}

void writeChecks(JsonWriter& writer, const CheckReport& checks) {
    writer.key("checks");
    writer.beginObject();
    writer.key("ok");
    writer.value(checks.ok());
    writer.key("info");
    writer.value(static_cast<Amount>(checks.infoCount));
    writer.key("warnings");
    writer.value(static_cast<Amount>(checks.warningCount));
    writer.key("errors");
    writer.value(static_cast<Amount>(checks.errorCount));
    writer.key("issues");
    writer.beginArray();
    for (const CheckIssue& issue : checks.issues) {
        writer.beginObject();
        writer.key("severity");
        writer.value(checkSeverityName(issue.severity));
        writer.key("code");
        writer.value(issue.code);
        writer.key("scope");
        writer.value(issue.scope);
        writer.key("id");
        writer.value(issue.id);
        writer.key("message");
        writer.value(issue.message);
        writer.key("observed");
        writer.value(issue.observed);
        writer.key("expected");
        writer.value(issue.expected);
        writer.endObject();
    }
    writer.endArray();
    writer.endObject();
}

void writeAnalyticsVaults(JsonWriter& writer, const std::vector<VaultHealth>& vaults) {
    writer.key("vaultHealth");
    writer.beginArray();
    for (const VaultHealth& health : vaults) {
        writer.beginObject();
        writer.key("vault");
        writer.value(health.vaultId);
        writer.key("asset");
        writer.value(health.assetId);
        writer.key("status");
        writer.value(health.status);
        writer.key("reserve");
        writer.value(health.reserve);
        writer.key("issuedCredits");
        writer.value(health.issuedCredits);
        writer.key("activeCredits");
        writer.value(health.activeCredits);
        writer.key("pendingCredits");
        writer.value(health.pendingCredits);
        writer.key("price");
        writer.value(health.price);
        writer.key("index");
        writer.value(health.index);
        writer.key("indexedLiability");
        writer.value(health.indexedLiability);
        writer.key("reserveRatioBps");
        writer.value(health.reserveRatioBps);
        writer.key("feeShareBps");
        writer.value(health.feeShareBps);
        writer.key("queueShareBps");
        writer.value(health.queueShareBps);
        writer.key("notes");
        writer.beginArray();
        for (const std::string& note : health.notes) {
            writer.value(note);
        }
        writer.endArray();
        writer.endObject();
    }
    writer.endArray();
}

void writeAnalyticsAccounts(JsonWriter& writer, const std::vector<AccountExposure>& accounts) {
    writer.key("accountExposure");
    writer.beginArray();
    for (const AccountExposure& exposure : accounts) {
        writer.beginObject();
        writer.key("account");
        writer.value(exposure.accountId);
        writer.key("totalReserves");
        writer.value(exposure.totalReserves);
        writer.key("totalCreditBookValue");
        writer.value(exposure.totalCreditBookValue);
        writer.key("feesPaid");
        writer.value(exposure.feesPaid);
        writer.key("reserveOut");
        writer.value(exposure.reserveOut);
        writer.key("reserveIn");
        writer.value(exposure.reserveIn);
        writer.key("liquidatedCredits");
        writer.value(exposure.liquidatedCredits);
        writer.key("reserves");
        writer.beginArray();
        for (const AccountHolding& holding : exposure.reserves) {
            writer.beginObject();
            writer.key("id");
            writer.value(holding.id);
            writer.key("balance");
            writer.value(holding.reserveBalance);
            writer.endObject();
        }
        writer.endArray();
        writer.key("credits");
        writer.beginArray();
        for (const AccountHolding& holding : exposure.credits) {
            writer.beginObject();
            writer.key("id");
            writer.value(holding.id);
            writer.key("balance");
            writer.value(holding.creditBalance);
            writer.key("bookValue");
            writer.value(holding.creditBookValue);
            writer.endObject();
        }
        writer.endArray();
        writer.endObject();
    }
    writer.endArray();
}

void writeAnalyticsAssets(JsonWriter& writer, const std::vector<AssetAggregate>& assets) {
    writer.key("assetAggregates");
    writer.beginArray();
    for (const AssetAggregate& aggregate : assets) {
        writer.beginObject();
        writer.key("asset");
        writer.value(aggregate.assetId);
        writer.key("symbol");
        writer.value(aggregate.symbol);
        writer.key("accountReserves");
        writer.value(aggregate.accountReserves);
        writer.key("vaultReserves");
        writer.value(aggregate.vaultReserves);
        writer.key("protocolFees");
        writer.value(aggregate.protocolFees);
        writer.key("insuranceBuffers");
        writer.value(aggregate.insuranceBuffers);
        writer.key("accountCount");
        writer.value(static_cast<Amount>(aggregate.accountCount));
        writer.key("vaultCount");
        writer.value(static_cast<Amount>(aggregate.vaultCount));
        writer.endObject();
    }
    writer.endArray();
}

void writeAnalyticsBatches(JsonWriter& writer, const std::vector<BatchPreview>& batches) {
    writer.key("batchPreview");
    writer.beginArray();
    for (const BatchPreview& batch : batches) {
        writer.beginObject();
        writer.key("vault");
        writer.value(batch.vaultId);
        writer.key("eligibleTickets");
        writer.value(static_cast<Amount>(batch.eligibleTickets));
        writer.key("lockedTickets");
        writer.value(static_cast<Amount>(batch.lockedTickets));
        writer.key("eligibleCredits");
        writer.value(batch.eligibleCredits);
        writer.key("lockedCredits");
        writer.value(batch.lockedCredits);
        writer.key("estimatedPayout");
        writer.value(batch.estimatedPayout);
        writer.key("estimatedFees");
        writer.value(batch.estimatedFees);
        writer.key("oldestRequestEpoch");
        writer.value(batch.oldestRequestEpoch);
        writer.key("nextUnlockEpoch");
        writer.value(batch.nextUnlockEpoch);
        writer.endObject();
    }
    writer.endArray();
}

void writeQueuePlan(JsonWriter& writer, const Ledger& ledger) {
    QueuePlanner planner;
    const QueuePlan plan = planner.build(ledger);
    const std::vector<QueueAccountSummary> accountSummaries = planner.accountSummaries(plan);
    writer.key("queuePlan");
    writer.beginObject();
    writer.key("totalTickets");
    writer.value(static_cast<Amount>(plan.totalTickets));
    writer.key("readyTickets");
    writer.value(static_cast<Amount>(plan.readyTickets));
    writer.key("lockedTickets");
    writer.value(static_cast<Amount>(plan.lockedTickets));
    writer.key("readyCredits");
    writer.value(plan.readyCredits);
    writer.key("lockedCredits");
    writer.value(plan.lockedCredits);
    writer.key("projectedGross");
    writer.value(plan.projectedGross);
    writer.key("projectedFees");
    writer.value(plan.projectedFees);
    writer.key("projectedNet");
    writer.value(plan.projectedNet);
    writer.key("buckets");
    writer.beginArray();
    for (const QueueBucket& bucket : plan.buckets) {
        writer.beginObject();
        writer.key("vault");
        writer.value(bucket.vaultId);
        writer.key("unlockEpoch");
        writer.value(bucket.unlockEpoch);
        writer.key("ticketCount");
        writer.value(static_cast<Amount>(bucket.ticketCount));
        writer.key("credits");
        writer.value(bucket.credits);
        writer.key("projectedGross");
        writer.value(bucket.projectedGross);
        writer.key("projectedFees");
        writer.value(bucket.projectedFees);
        writer.key("projectedNet");
        writer.value(bucket.projectedNet);
        writer.endObject();
    }
    writer.endArray();
    writer.key("accounts");
    writer.beginArray();
    for (const QueueAccountSummary& account : accountSummaries) {
        writer.beginObject();
        writer.key("account");
        writer.value(account.accountId);
        writer.key("ticketCount");
        writer.value(static_cast<Amount>(account.ticketCount));
        writer.key("readyTickets");
        writer.value(static_cast<Amount>(account.readyTickets));
        writer.key("lockedTickets");
        writer.value(static_cast<Amount>(account.lockedTickets));
        writer.key("credits");
        writer.value(account.credits);
        writer.key("readyCredits");
        writer.value(account.readyCredits);
        writer.key("lockedCredits");
        writer.value(account.lockedCredits);
        writer.key("projectedNet");
        writer.value(account.projectedNet);
        writer.endObject();
    }
    writer.endArray();
    writer.key("tickets");
    writer.beginArray();
    for (const QueueTicketView& ticket : plan.tickets) {
        writer.beginObject();
        writer.key("id");
        writer.value(ticket.id);
        writer.key("vault");
        writer.value(ticket.vaultId);
        writer.key("account");
        writer.value(ticket.accountId);
        writer.key("status");
        writer.value(ticketStatusName(ticket.status));
        writer.key("ready");
        writer.value(ticket.ready);
        writer.key("credits");
        writer.value(ticket.credits);
        writer.key("projectedGross");
        writer.value(ticket.projectedGross);
        writer.key("projectedFee");
        writer.value(ticket.projectedFee);
        writer.key("projectedNet");
        writer.value(ticket.projectedNet);
        writer.key("requestedEpoch");
        writer.value(ticket.requestedEpoch);
        writer.key("unlockEpoch");
        writer.value(ticket.unlockEpoch);
        writer.key("age");
        writer.value(ticket.age);
        writer.endObject();
    }
    writer.endArray();
    writer.endObject();
}

void writeAnalytics(JsonWriter& writer, const Ledger& ledger) {
    AnalyticsEngine analyticsEngine;
    const SystemAnalytics analytics = analyticsEngine.analyze(ledger);
    writer.key("analytics");
    writer.beginObject();
    writeAnalyticsVaults(writer, analytics.vaults);
    writeAnalyticsAccounts(writer, analytics.accounts);
    writeAnalyticsAssets(writer, analytics.assets);
    writeAnalyticsBatches(writer, analytics.batches);
    writeQueuePlan(writer, ledger);
    writeChecks(writer, analytics.checks);
    writer.endObject();
}

}  // namespace

std::string renderReport(const Ledger& ledger, const RunOptions& options) {
    JsonWriter writer(options.pretty);
    writer.beginObject();
    writer.key("name");
    writer.value(ledger.name());
    writer.key("epoch");
    writer.value(ledger.epoch());
    writeAssets(writer, ledger);
    writeVaults(writer, ledger);
    writeAccounts(writer, ledger);
    writeTickets(writer, ledger);
    writeLiquidations(writer, ledger);
    writeMetrics(writer, ledger);
    writeReconciliation(writer, ledger);
    writeAnalytics(writer, ledger);
    if (options.includeEvents) {
        writeEvents(writer, ledger);
    }
    writer.endObject();
    return writer.str();
}

}  // namespace cobaltdtl
