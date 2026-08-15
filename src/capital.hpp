#pragma once

#include "common.hpp"

namespace cobaltdtl {

struct CapitalInput {
    std::string vaultId;
    Amount reserve{0};
    Amount liquidReserve{0};
    Amount issuedCredits{0};
    Amount pendingCredits{0};
    Amount index{kScale};
    Amount reserveHaircutBps{0};
    Amount redemptionShockBps{2'500};
    Amount operationalBufferBps{50};
    Amount maturityEpochs{1};
};

struct CapitalLine {
    std::string vaultId;
    Amount liability{0};
    Amount queuedLiability{0};
    Amount effectiveReserve{0};
    Amount stressedOutflows{0};
    Amount operationalBuffer{0};
    Amount requiredCapital{0};
    Amount capitalDeficit{0};
    Amount coverageBps{0};
    Amount liquidityBps{0};
    Amount concentrationBps{0};
    Amount maturityEpochs{0};
};

struct CapitalPolicy {
    Amount minimumCoverageBps{10'500};
    Amount minimumLiquidityBps{10'000};
    Amount maximumHhiBps{6'000};
};

struct PortfolioCapital {
    std::vector<CapitalLine> vaults;
    Amount totalLiability{0};
    Amount totalEffectiveReserve{0};
    Amount totalRequiredCapital{0};
    Amount totalCapitalDeficit{0};
    Amount coverageBps{0};
    Amount liquidityBps{0};
    Amount largestConcentrationBps{0};
    Amount hhiBps{0};
    Amount weightedMaturityEpochs{0};
    bool compliant{false};
};

class CapitalEngine {
  public:
    CapitalLine assess(const CapitalInput& input) const;
    PortfolioCapital assessPortfolio(std::vector<CapitalInput> inputs, const CapitalPolicy& policy) const;

  private:
    void validate(const CapitalInput& input) const;
    void validate(const CapitalPolicy& policy) const;
    Amount ratioBps(Amount numerator, Amount denominator, std::string_view context) const;
};

}  // namespace cobaltdtl
