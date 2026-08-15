#include "capital.hpp"

#include <set>

namespace cobaltdtl {

CapitalLine CapitalEngine::assess(const CapitalInput& input) const {
    validate(input);
    CapitalLine line;
    line.vaultId = input.vaultId;
    line.liability = mulDivCeil(input.issuedCredits, input.index, kScale, "capital liability");
    line.queuedLiability = mulDivCeil(input.pendingCredits, input.index, kScale, "queued liability");
    line.effectiveReserve = subtractBpsFloor(input.reserve, input.reserveHaircutBps, "effective reserve");
    line.stressedOutflows =
        mulDivCeil(line.queuedLiability, checkedAdd(kBps, input.redemptionShockBps, "outflow shock"), kBps, "stressed outflows");
    line.operationalBuffer = applyBpsCeil(line.liability, input.operationalBufferBps, "operational buffer");
    line.requiredCapital = checkedAdd(line.liability, line.stressedOutflows, "required capital");
    line.requiredCapital = checkedAdd(line.requiredCapital, line.operationalBuffer, "required capital buffer");
    line.capitalDeficit = maxAmount(checkedSub(line.requiredCapital, minAmount(line.requiredCapital, line.effectiveReserve), "capital deficit"), 0);
    line.coverageBps = ratioBps(line.effectiveReserve, line.requiredCapital, "capital coverage");
    line.liquidityBps = ratioBps(input.liquidReserve, line.stressedOutflows, "liquidity coverage");
    line.maturityEpochs = input.maturityEpochs;
    return line;
}

PortfolioCapital CapitalEngine::assessPortfolio(std::vector<CapitalInput> inputs, const CapitalPolicy& policy) const {
    validate(policy);
    if (inputs.empty()) {
        fail(ErrorCode::Validation, "capital portfolio requires at least one vault");
    }
    std::sort(inputs.begin(), inputs.end(), [](const CapitalInput& left, const CapitalInput& right) {
        return left.vaultId < right.vaultId;
    });
    std::set<std::string> ids;
    PortfolioCapital portfolio;
    Amount totalLiquidReserve = 0;
    Amount totalStressedOutflows = 0;
    Amount weightedMaturity = 0;
    for (const CapitalInput& input : inputs) {
        if (!ids.insert(input.vaultId).second) {
            fail(ErrorCode::Validation, "duplicate capital vault: " + input.vaultId);
        }
        CapitalLine line = assess(input);
        portfolio.totalLiability = checkedAdd(portfolio.totalLiability, line.liability, "portfolio liability");
        portfolio.totalEffectiveReserve =
            checkedAdd(portfolio.totalEffectiveReserve, line.effectiveReserve, "portfolio effective reserve");
        portfolio.totalRequiredCapital =
            checkedAdd(portfolio.totalRequiredCapital, line.requiredCapital, "portfolio required capital");
        portfolio.totalCapitalDeficit =
            checkedAdd(portfolio.totalCapitalDeficit, line.capitalDeficit, "portfolio capital deficit");
        totalLiquidReserve = checkedAdd(totalLiquidReserve, input.liquidReserve, "portfolio liquid reserve");
        totalStressedOutflows =
            checkedAdd(totalStressedOutflows, line.stressedOutflows, "portfolio stressed outflows");
        weightedMaturity = checkedAdd(
            weightedMaturity,
            checkedMul(line.liability, input.maturityEpochs, "weighted maturity product"),
            "weighted maturity");
        portfolio.vaults.push_back(line);
    }
    if (portfolio.totalLiability <= 0) {
        fail(ErrorCode::Validation, "capital portfolio liability must be positive");
    }
    for (CapitalLine& line : portfolio.vaults) {
        line.concentrationBps = ratioBps(line.liability, portfolio.totalLiability, "vault concentration");
        portfolio.largestConcentrationBps = maxAmount(portfolio.largestConcentrationBps, line.concentrationBps);
        portfolio.hhiBps = checkedAdd(
            portfolio.hhiBps,
            mulDivFloor(line.concentrationBps, line.concentrationBps, kBps, "portfolio HHI"),
            "portfolio HHI sum");
    }
    portfolio.coverageBps = ratioBps(portfolio.totalEffectiveReserve, portfolio.totalRequiredCapital, "portfolio coverage");
    portfolio.liquidityBps = ratioBps(totalLiquidReserve, totalStressedOutflows, "portfolio liquidity");
    portfolio.weightedMaturityEpochs =
        mulDivFloor(weightedMaturity, 1, portfolio.totalLiability, "portfolio weighted maturity");
    portfolio.compliant = portfolio.totalCapitalDeficit == 0 && portfolio.coverageBps >= policy.minimumCoverageBps &&
                          portfolio.liquidityBps >= policy.minimumLiquidityBps && portfolio.hhiBps <= policy.maximumHhiBps;
    return portfolio;
}

void CapitalEngine::validate(const CapitalInput& input) const {
    requireIdentifier("capital vault", input.vaultId);
    if (input.reserve < 0 || input.liquidReserve < 0 || input.issuedCredits < 0 || input.pendingCredits < 0) {
        fail(ErrorCode::Validation, "capital input contains a negative balance");
    }
    if (input.liquidReserve > input.reserve) {
        fail(ErrorCode::Validation, "liquid reserve exceeds total reserve for " + input.vaultId);
    }
    if (input.pendingCredits > input.issuedCredits) {
        fail(ErrorCode::Validation, "pending credits exceed issued credits for " + input.vaultId);
    }
    if (input.index <= 0 || input.maturityEpochs < 0) {
        fail(ErrorCode::Validation, "capital index must be positive and maturity non-negative");
    }
    if (input.reserveHaircutBps < 0 || input.reserveHaircutBps > kBps || input.redemptionShockBps < 0 ||
        input.operationalBufferBps < 0) {
        fail(ErrorCode::Validation, "capital basis-point parameter is outside its domain");
    }
}

void CapitalEngine::validate(const CapitalPolicy& policy) const {
    if (policy.minimumCoverageBps <= 0 || policy.minimumLiquidityBps <= 0 || policy.maximumHhiBps <= 0 ||
        policy.maximumHhiBps > kBps) {
        fail(ErrorCode::Validation, "capital policy thresholds are outside their domain");
    }
}

Amount CapitalEngine::ratioBps(Amount numerator, Amount denominator, std::string_view context) const {
    if (denominator == 0) {
        return numerator == 0 ? 0 : kNoLimit;
    }
    return mulDivFloor(numerator, kBps, denominator, context);
}

}  // namespace cobaltdtl
