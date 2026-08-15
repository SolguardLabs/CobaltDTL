#pragma once

#include "capital.hpp"
#include "checks.hpp"

namespace cobaltdtl {

struct VaultHealth {
    std::string vaultId;
    std::string assetId;
    Amount reserve{0};
    Amount issuedCredits{0};
    Amount activeCredits{0};
    Amount pendingCredits{0};
    Amount price{0};
    Amount index{0};
    Amount indexedLiability{0};
    Amount reserveRatioBps{0};
    Amount feeShareBps{0};
    Amount queueShareBps{0};
    Amount capitalLiability{0};
    Amount effectiveReserve{0};
    Amount stressedOutflows{0};
    Amount requiredCapital{0};
    Amount capitalDeficit{0};
    Amount capitalCoverageBps{0};
    Amount liquidityCoverageBps{0};
    bool capitalCompliant{false};
    std::string status;
    std::vector<std::string> notes;
};

struct AccountHolding {
    std::string id;
    Amount reserveBalance{0};
    Amount creditBalance{0};
    Amount creditBookValue{0};
};

struct AccountExposure {
    std::string accountId;
    std::vector<AccountHolding> reserves;
    std::vector<AccountHolding> credits;
    Amount totalReserves{0};
    Amount totalCreditBookValue{0};
    Amount feesPaid{0};
    Amount reserveOut{0};
    Amount reserveIn{0};
    Amount liquidatedCredits{0};
};

struct AssetAggregate {
    std::string assetId;
    std::string symbol;
    Amount accountReserves{0};
    Amount vaultReserves{0};
    Amount protocolFees{0};
    Amount insuranceBuffers{0};
    int accountCount{0};
    int vaultCount{0};
};

struct BatchPreview {
    std::string vaultId;
    int eligibleTickets{0};
    int lockedTickets{0};
    Amount eligibleCredits{0};
    Amount lockedCredits{0};
    Amount estimatedPayout{0};
    Amount estimatedFees{0};
    Epoch oldestRequestEpoch{0};
    Epoch nextUnlockEpoch{0};
};

struct SystemAnalytics {
    std::vector<VaultHealth> vaults;
    std::vector<AccountExposure> accounts;
    std::vector<AssetAggregate> assets;
    std::vector<BatchPreview> batches;
    CheckReport checks;
};

class AnalyticsEngine {
  public:
    explicit AnalyticsEngine(PricingService pricing = PricingService());

    SystemAnalytics analyze(const Ledger& ledger) const;
    std::vector<VaultHealth> vaultHealth(const Ledger& ledger) const;
    std::vector<AccountExposure> accountExposure(const Ledger& ledger) const;
    std::vector<AssetAggregate> assetAggregates(const Ledger& ledger) const;
    std::vector<BatchPreview> batchPreviews(const Ledger& ledger) const;

  private:
    VaultHealth buildVaultHealth(const VaultState& vault) const;
    AccountExposure buildAccountExposure(const Ledger& ledger, const AccountState& account) const;
    AssetAggregate buildAssetAggregate(const Ledger& ledger, const AssetDef& asset) const;
    BatchPreview buildBatchPreview(const Ledger& ledger, const VaultState& vault) const;
    std::string classifyVault(const PriceSnapshot& price, const VaultState& vault) const;
    std::vector<std::string> vaultNotes(const PriceSnapshot& price, const VaultState& vault) const;
    Amount accountReserveTotal(const AccountState& account) const;
    Amount accountCreditBookValue(const Ledger& ledger, const AccountState& account) const;

    PricingService pricing_;
    CapitalEngine capital_;
    SanityChecker checker_;
};

}  // namespace cobaltdtl

