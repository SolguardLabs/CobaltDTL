#pragma once

#include "model.hpp"

namespace cobaltdtl {

struct PriceSnapshot {
    std::string vaultId;
    Amount reserve{0};
    Amount index{0};
    Amount issuedCredits{0};
    Amount pendingCredits{0};
    Amount activeCredits{0};
    Amount price{0};
    Amount indexedLiability{0};
    Amount reserveRatioBps{0};
};

class PricingService {
  public:
    PriceSnapshot snapshot(const VaultState& vault) const;
    Amount creditsForReserve(const VaultState& vault, Amount reserveAmount) const;
    Amount reserveForCredits(const VaultState& vault, Amount credits) const;
    Amount indexedLiability(const VaultState& vault) const;
    Amount fairReserveForCredits(const VaultState& vault, Amount credits) const;
};

}  // namespace cobaltdtl

