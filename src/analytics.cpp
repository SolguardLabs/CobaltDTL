#include "analytics.hpp"

namespace cobaltdtl {

AnalyticsEngine::AnalyticsEngine(PricingService pricing) : pricing_(std::move(pricing)) {}

SystemAnalytics AnalyticsEngine::analyze(const Ledger& ledger) const {
    SystemAnalytics analytics;
    analytics.vaults = vaultHealth(ledger);
    analytics.accounts = accountExposure(ledger);
    analytics.assets = assetAggregates(ledger);
    analytics.batches = batchPreviews(ledger);
    analytics.checks = checker_.run(ledger);
    return analytics;
}

std::vector<VaultHealth> AnalyticsEngine::vaultHealth(const Ledger& ledger) const {
    std::vector<VaultHealth> health;
    health.reserve(ledger.vaults().size());
    for (const auto& item : ledger.vaults()) {
        health.push_back(buildVaultHealth(item.second));
    }
    return health;
}

std::vector<AccountExposure> AnalyticsEngine::accountExposure(const Ledger& ledger) const {
    std::vector<AccountExposure> exposures;
    exposures.reserve(ledger.accounts().size());
    for (const auto& item : ledger.accounts()) {
        exposures.push_back(buildAccountExposure(ledger, item.second));
    }
    return exposures;
}

std::vector<AssetAggregate> AnalyticsEngine::assetAggregates(const Ledger& ledger) const {
    std::vector<AssetAggregate> aggregates;
    aggregates.reserve(ledger.assets().size());
    for (const auto& item : ledger.assets()) {
        aggregates.push_back(buildAssetAggregate(ledger, item.second));
    }
    return aggregates;
}

std::vector<BatchPreview> AnalyticsEngine::batchPreviews(const Ledger& ledger) const {
    std::vector<BatchPreview> previews;
    previews.reserve(ledger.vaults().size());
    for (const auto& item : ledger.vaults()) {
        previews.push_back(buildBatchPreview(ledger, item.second));
    }
    return previews;
}

VaultHealth AnalyticsEngine::buildVaultHealth(const VaultState& vault) const {
    const PriceSnapshot price = pricing_.snapshot(vault);
    VaultHealth health;
    health.vaultId = vault.id;
    health.assetId = vault.assetId;
    health.reserve = vault.realReserve;
    health.issuedCredits = vault.issuedCredits;
    health.activeCredits = price.activeCredits;
    health.pendingCredits = vault.pendingCredits;
    health.price = price.price;
    health.index = vault.index;
    health.indexedLiability = price.indexedLiability;
    health.reserveRatioBps = price.reserveRatioBps;
    if (vault.realReserve > 0) {
        health.feeShareBps = mulDivFloor(vault.protocolFees, kBps, vault.realReserve, "fee share");
    }
    if (vault.issuedCredits > 0) {
        health.queueShareBps = mulDivFloor(vault.pendingCredits, kBps, vault.issuedCredits, "queue share");
    }
    health.status = classifyVault(price, vault);
    health.notes = vaultNotes(price, vault);
    return health;
}

AccountExposure AnalyticsEngine::buildAccountExposure(const Ledger& ledger, const AccountState& account) const {
    AccountExposure exposure;
    exposure.accountId = account.id;
    exposure.feesPaid = account.feesPaid;
    exposure.reserveOut = account.reserveOut;
    exposure.reserveIn = account.reserveIn;
    exposure.liquidatedCredits = account.liquidatedCredits;
    for (const auto& item : account.reserves) {
        AccountHolding holding;
        holding.id = item.first;
        holding.reserveBalance = item.second;
        exposure.totalReserves = checkedAdd(exposure.totalReserves, item.second, "account reserve exposure");
        exposure.reserves.push_back(holding);
    }
    for (const auto& item : account.credits) {
        AccountHolding holding;
        holding.id = item.first;
        holding.creditBalance = item.second;
        const VaultState& vault = ledger.vault(item.first);
        holding.creditBookValue = pricing_.reserveForCredits(vault, item.second);
        exposure.totalCreditBookValue =
            checkedAdd(exposure.totalCreditBookValue, holding.creditBookValue, "account credit exposure");
        exposure.credits.push_back(holding);
    }
    return exposure;
}

AssetAggregate AnalyticsEngine::buildAssetAggregate(const Ledger& ledger, const AssetDef& asset) const {
    AssetAggregate aggregate;
    aggregate.assetId = asset.id;
    aggregate.symbol = asset.symbol;
    for (const auto& item : ledger.accounts()) {
        const auto found = item.second.reserves.find(asset.id);
        if (found != item.second.reserves.end()) {
            aggregate.accountReserves = checkedAdd(aggregate.accountReserves, found->second, "asset account reserve");
            if (found->second != 0) {
                aggregate.accountCount += 1;
            }
        }
    }
    for (const auto& item : ledger.vaults()) {
        const VaultState& vault = item.second;
        if (vault.assetId != asset.id) {
            continue;
        }
        aggregate.vaultReserves = checkedAdd(aggregate.vaultReserves, vault.realReserve, "asset vault reserve");
        aggregate.protocolFees = checkedAdd(aggregate.protocolFees, vault.protocolFees, "asset protocol fees");
        aggregate.insuranceBuffers =
            checkedAdd(aggregate.insuranceBuffers, vault.insuranceBuffer, "asset insurance buffers");
        aggregate.vaultCount += 1;
    }
    return aggregate;
}

BatchPreview AnalyticsEngine::buildBatchPreview(const Ledger& ledger, const VaultState& vault) const {
    BatchPreview preview;
    preview.vaultId = vault.id;
    preview.oldestRequestEpoch = ledger.epoch();
    preview.nextUnlockEpoch = 0;
    bool sawRequest = false;
    bool sawLocked = false;
    const PriceSnapshot price = pricing_.snapshot(vault);
    for (const RedemptionTicket& ticket : ledger.tickets()) {
        if (ticket.vaultId != vault.id) {
            continue;
        }
        if (ticket.status != TicketStatus::Queued && ticket.status != TicketStatus::Deferred) {
            continue;
        }
        if (!sawRequest || ticket.requestedEpoch < preview.oldestRequestEpoch) {
            preview.oldestRequestEpoch = ticket.requestedEpoch;
            sawRequest = true;
        }
        if (ticket.unlockEpoch > ledger.epoch()) {
            preview.lockedTickets += 1;
            preview.lockedCredits = checkedAdd(preview.lockedCredits, ticket.credits, "locked ticket credits");
            if (!sawLocked || ticket.unlockEpoch < preview.nextUnlockEpoch) {
                preview.nextUnlockEpoch = ticket.unlockEpoch;
                sawLocked = true;
            }
            continue;
        }
        preview.eligibleTickets += 1;
        preview.eligibleCredits = checkedAdd(preview.eligibleCredits, ticket.credits, "eligible ticket credits");
        const Amount gross = mulDivFloor(ticket.credits, price.price, kScale, "batch preview payout");
        const Amount fee = applyBpsFloor(gross, vault.policy.redemptionFeeBps, "batch preview fee");
        preview.estimatedPayout = checkedAdd(preview.estimatedPayout, checkedSub(gross, fee, "batch net"), "batch payout");
        preview.estimatedFees = checkedAdd(preview.estimatedFees, fee, "batch fees");
    }
    return preview;
}

std::string AnalyticsEngine::classifyVault(const PriceSnapshot& price, const VaultState& vault) const {
    if (vault.realReserve < 0) {
        return "deficit";
    }
    if (price.indexedLiability == 0 && vault.issuedCredits == 0) {
        return "idle";
    }
    if (price.reserveRatioBps >= 11'000) {
        return "surplus";
    }
    if (price.reserveRatioBps >= 9'500) {
        return "balanced";
    }
    if (price.reserveRatioBps >= 8'000) {
        return "watch";
    }
    return "thin";
}

std::vector<std::string> AnalyticsEngine::vaultNotes(const PriceSnapshot& price, const VaultState& vault) const {
    std::vector<std::string> notes;
    if (!vault.policy.allowDeposits) {
        notes.push_back("deposits_closed");
    }
    if (!vault.policy.allowRedemptions) {
        notes.push_back("redemptions_closed");
    }
    if (!vault.policy.allowLiquidations) {
        notes.push_back("liquidations_closed");
    }
    if (vault.pendingCredits > 0) {
        notes.push_back("redemption_queue");
    }
    if (vault.protocolFees > 0) {
        notes.push_back("fees_accrued");
    }
    if (vault.realReserve < vault.policy.minLiquidity) {
        notes.push_back("liquidity_floor");
    }
    if (price.reserveRatioBps < 9'500 && price.indexedLiability > 0) {
        notes.push_back("reserve_ratio_watch");
    }
    return notes;
}

Amount AnalyticsEngine::accountReserveTotal(const AccountState& account) const {
    RunningTotal total;
    for (const auto& item : account.reserves) {
        total.add(item.second, "account reserve total");
    }
    return total.value;
}

Amount AnalyticsEngine::accountCreditBookValue(const Ledger& ledger, const AccountState& account) const {
    RunningTotal total;
    for (const auto& item : account.credits) {
        total.add(pricing_.reserveForCredits(ledger.vault(item.first), item.second), "account credit book total");
    }
    return total.value;
}

}  // namespace cobaltdtl

