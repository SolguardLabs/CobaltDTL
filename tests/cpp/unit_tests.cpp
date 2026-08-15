#include "capital.hpp"
#include "governance.hpp"
#include "sha256.hpp"

#include <cassert>
#include <iostream>

using namespace cobaltdtl;

void testSha256() {
    assert(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

void testCapitalPortfolio() {
    CapitalEngine engine;
    std::vector<CapitalInput> inputs{
        {"vault-eur", 520'000, 120'000, 400'000, 40'000, kScale, 200, 2'500, 50, 3},
        {"vault-usd", 800'000, 200'000, 600'000, 50'000, kScale, 100, 2'000, 50, 7},
    };
    const PortfolioCapital portfolio = engine.assessPortfolio(inputs, {10'500, 10'000, 5'500});
    assert(portfolio.totalLiability == 1'000'000);
    assert(portfolio.totalEffectiveReserve == 1'301'600);
    assert(portfolio.totalRequiredCapital == 1'115'000);
    assert(portfolio.totalCapitalDeficit == 0);
    assert(portfolio.hhiBps == 5'200);
    assert(portfolio.largestConcentrationBps == 6'000);
    assert(portfolio.weightedMaturityEpochs == 5);
    assert(portfolio.compliant);
}

void testCapitalDeficit() {
    CapitalEngine engine;
    const CapitalLine line =
        engine.assess({"vault-usd", 900, 50, 1'000, 100, kScale, 1'000, 5'000, 100, 2});
    assert(line.effectiveReserve == 810);
    assert(line.stressedOutflows == 150);
    assert(line.operationalBuffer == 10);
    assert(line.requiredCapital == 1'160);
    assert(line.capitalDeficit == 350);
    assert(line.coverageBps == 6'982);
    assert(line.liquidityBps == 3'333);
}

GovernanceOperation operation(Epoch executeAfter) {
    GovernanceOperation value;
    value.protocol = "CobaltDTL";
    value.network = "vaults-1";
    value.target = "risk-registry";
    value.method = "setCapitalPolicy";
    value.payloadHash = sha256("coverage=10500;liquidity=10000");
    value.salt = "change-2026-08";
    value.executeAfter = executeAfter;
    return value;
}

void testGovernanceLifecycle() {
    GovernanceExecutor executor({"gov-a", "gov-b", "gov-c"}, 2, 20, 40, "guardian");
    GovernanceRecord queued = executor.queue(operation(120), "gov-a", 100);
    assert(queued.approvals.size() == 1U);
    bool timelockRejected = false;
    try {
        (void)executor.execute(queued.id, "gov-c", 119);
    } catch (const DtlError&) {
        timelockRejected = true;
    }
    assert(timelockRejected);
    executor.approve(queued.id, "gov-b", 110);
    const GovernanceRecord executed = executor.execute(queued.id, "gov-c", 120);
    assert(executed.state == GovernanceState::Executed);
    assert(executed.executedAt == 120);
}

void testGovernanceCancellation() {
    GovernanceExecutor executor({"gov-a"}, 1, 10, 20, "guardian");
    GovernanceOperation value = operation(20);
    value.salt = "cancel-change";
    const GovernanceRecord queued = executor.queue(value, "gov-a", 10);
    const GovernanceRecord cancelled = executor.cancel(queued.id, "guardian", 11);
    assert(cancelled.state == GovernanceState::Cancelled);
    bool executionRejected = false;
    try {
        (void)executor.execute(queued.id, "gov-a", 20);
    } catch (const DtlError&) {
        executionRejected = true;
    }
    assert(executionRejected);
}

int main() {
    testSha256();
    testCapitalPortfolio();
    testCapitalDeficit();
    testGovernanceLifecycle();
    testGovernanceCancellation();
    std::cout << "C++ unit tests passed: 5\n";
    return 0;
}
