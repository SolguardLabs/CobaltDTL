#include "pricing.hpp"

namespace cobaltdtl {

PriceSnapshot PricingService::snapshot(const VaultState& vault) const {
    PriceSnapshot snapshot;
    snapshot.vaultId = vault.id;
    snapshot.reserve = vault.realReserve;
    snapshot.index = vault.index;
    snapshot.issuedCredits = vault.issuedCredits;
    snapshot.pendingCredits = vault.pendingCredits;
    snapshot.activeCredits = checkedSub(vault.issuedCredits, vault.pendingCredits, "active credit supply");
    if (snapshot.activeCredits <= 0) {
        snapshot.price = vault.index;
    } else {
        snapshot.price = mulDivFloor(vault.realReserve, kScale, snapshot.activeCredits, "price per credit");
    }
    snapshot.indexedLiability = indexedLiability(vault);
    if (snapshot.indexedLiability <= 0) {
        snapshot.reserveRatioBps = kBps;
    } else {
        snapshot.reserveRatioBps = mulDivFloor(vault.realReserve, kBps, snapshot.indexedLiability, "reserve ratio");
    }
    return snapshot;
}

Amount PricingService::creditsForReserve(const VaultState& vault, Amount reserveAmount) const {
    if (reserveAmount <= 0) {
        return 0;
    }
    const PriceSnapshot price = snapshot(vault);
    if (price.price <= 0) {
        fail(ErrorCode::Validation, "vault price must be positive");
    }
    return mulDivFloor(reserveAmount, kScale, price.price, "credits for reserve");
}

Amount PricingService::reserveForCredits(const VaultState& vault, Amount credits) const {
    if (credits <= 0) {
        return 0;
    }
    const PriceSnapshot price = snapshot(vault);
    return mulDivFloor(credits, price.price, kScale, "reserve for credits");
}

Amount PricingService::indexedLiability(const VaultState& vault) const {
    if (vault.issuedCredits <= 0) {
        return 0;
    }
    return mulDivFloor(vault.issuedCredits, vault.index, kScale, "indexed liability");
}

Amount PricingService::fairReserveForCredits(const VaultState& vault, Amount credits) const {
    if (credits <= 0 || vault.issuedCredits <= 0) {
        return 0;
    }
    return mulDivFloor(vault.realReserve, credits, vault.issuedCredits, "pro rata reserve");
}

}  // namespace cobaltdtl

