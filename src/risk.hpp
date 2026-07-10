#pragma once

#include "pricing.hpp"

namespace cobaltdtl {

struct RiskReport {
    bool depositsOpen{true};
    bool redemptionsOpen{true};
    bool liquidationsOpen{true};
    bool liquidityFloorMet{true};
    Amount reserveAfter{0};
    Amount activeSupplyAfter{0};
    Amount reserveRatioBps{0};
    std::vector<std::string> notes;
};

class RiskEngine {
  public:
    explicit RiskEngine(PricingService pricing = PricingService());

    void checkDeposit(const VaultState& vault, Amount reserveAmount) const;
    void checkRedemptionRequest(const VaultState& vault, Amount credits) const;
    void checkLiquidation(const VaultState& vault, Amount reserveAmount) const;
    void checkBatchLimit(const VaultState& vault, Amount payout) const;
    RiskReport assess(const VaultState& vault) const;

  private:
    PricingService pricing_;
};

}  // namespace cobaltdtl

