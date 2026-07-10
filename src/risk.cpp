#include "risk.hpp"

namespace cobaltdtl {

RiskEngine::RiskEngine(PricingService pricing) : pricing_(std::move(pricing)) {}

void RiskEngine::checkDeposit(const VaultState& vault, Amount reserveAmount) const {
    if (!vault.policy.allowDeposits) {
        fail(ErrorCode::Policy, "deposits are closed for vault " + vault.id);
    }
    if (reserveAmount < vault.policy.minDeposit) {
        fail(ErrorCode::Policy, "deposit below minimum for vault " + vault.id);
    }
    const Amount nextReserve = checkedAdd(vault.realReserve, reserveAmount, "deposit cap");
    if (nextReserve > vault.policy.depositCap) {
        fail(ErrorCode::Policy, "deposit cap exceeded for vault " + vault.id);
    }
}

void RiskEngine::checkRedemptionRequest(const VaultState& vault, Amount credits) const {
    if (!vault.policy.allowRedemptions) {
        fail(ErrorCode::Policy, "redemptions are closed for vault " + vault.id);
    }
    if (credits <= 0) {
        fail(ErrorCode::Validation, "redemption credits must be positive");
    }
    if (credits > vault.policy.maxTicketCredits) {
        fail(ErrorCode::Policy, "redemption ticket exceeds vault ticket limit " + vault.id);
    }
}

void RiskEngine::checkLiquidation(const VaultState& vault, Amount reserveAmount) const {
    if (!vault.policy.allowLiquidations) {
        fail(ErrorCode::Policy, "liquidations are closed for vault " + vault.id);
    }
    if (reserveAmount <= 0) {
        fail(ErrorCode::Validation, "liquidation reserve must be positive");
    }
}

void RiskEngine::checkBatchLimit(const VaultState& vault, Amount payout) const {
    if (payout > vault.policy.maxBatchPayout) {
        fail(ErrorCode::Policy, "batch payout limit exceeded for vault " + vault.id);
    }
}

RiskReport RiskEngine::assess(const VaultState& vault) const {
    RiskReport report;
    const PriceSnapshot price = pricing_.snapshot(vault);
    report.depositsOpen = vault.policy.allowDeposits;
    report.redemptionsOpen = vault.policy.allowRedemptions;
    report.liquidationsOpen = vault.policy.allowLiquidations;
    report.reserveAfter = vault.realReserve;
    report.activeSupplyAfter = price.activeCredits;
    report.reserveRatioBps = price.reserveRatioBps;
    report.liquidityFloorMet = vault.realReserve >= vault.policy.minLiquidity;
    if (!report.depositsOpen) {
        report.notes.push_back("deposits_closed");
    }
    if (!report.redemptionsOpen) {
        report.notes.push_back("redemptions_closed");
    }
    if (!report.liquidationsOpen) {
        report.notes.push_back("liquidations_closed");
    }
    if (!report.liquidityFloorMet) {
        report.notes.push_back("liquidity_floor");
    }
    if (price.reserveRatioBps < 9'500) {
        report.notes.push_back("reserve_ratio_watch");
    }
    return report;
}

}  // namespace cobaltdtl

