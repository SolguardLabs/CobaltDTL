#pragma once

#include "risk.hpp"

namespace cobaltdtl {

struct ReconciliationLine {
    std::string vaultId;
    Amount accountCredits{0};
    Amount queuedCredits{0};
    Amount issuedCredits{0};
    Amount reserve{0};
};

class Ledger {
  public:
    Ledger();
    explicit Ledger(const Scenario& scenario);

    const std::string& name() const;
    Epoch epoch() const;
    const std::map<std::string, AssetDef>& assets() const;
    const std::map<std::string, VaultState>& vaults() const;
    const std::map<std::string, AccountState>& accounts() const;
    const std::vector<RedemptionTicket>& tickets() const;
    const std::vector<LiquidationRecord>& liquidations() const;
    const std::vector<Event>& events() const;
    const Metrics& metrics() const;

    const AssetDef& asset(const std::string& id) const;
    const VaultState& vault(const std::string& id) const;
    const AccountState& account(const std::string& id) const;

    void apply(const Operation& op);
    void advance(Epoch epochs);
    void deposit(const std::string& accountId, const std::string& vaultId, Amount amount, std::string note = "");
    void mint(const std::string& accountId, const std::string& vaultId, Amount reserveAmount, std::string note = "");
    void requestRedemption(
        const std::string& accountId,
        const std::string& vaultId,
        Amount credits,
        Amount minPayout,
        std::string ticketId = "",
        std::string note = "");
    int settleRedemptions(const std::string& vaultId, int limit, std::string note = "");
    void setIndex(const std::string& vaultId, Amount index, std::string note = "");
    void rebalance(const std::string& vaultId, Amount reserveDelta, std::string note = "");
    void liquidate(
        const std::string& accountId,
        const std::string& vaultId,
        Amount reserveAmount,
        Amount penaltyBps,
        std::string recordId = "",
        std::string note = "");
    void transferCredit(
        const std::string& fromAccount,
        const std::string& toAccount,
        const std::string& vaultId,
        Amount credits,
        std::string note = "");
    void sweepFees(const std::string& accountId, const std::string& vaultId, Amount amount, std::string note = "");
    void checkpoint(std::string note = "");

    std::vector<ReconciliationLine> reconcile() const;
    Amount accountCreditSum(const std::string& vaultId) const;
    Amount queuedCreditSum(const std::string& vaultId) const;
    Amount reserveByAsset(const std::string& assetId) const;

  private:
    VaultState& mutableVault(const std::string& id);
    AccountState& mutableAccount(const std::string& id);
    Amount& reserveSlot(AccountState& account, const std::string& assetId);
    Amount& creditSlot(AccountState& account, const std::string& vaultId);

    void debitReserve(AccountState& account, const std::string& assetId, Amount amount, std::string_view context);
    void creditReserve(AccountState& account, const std::string& assetId, Amount amount, std::string_view context);
    void debitCredit(AccountState& account, const std::string& vaultId, Amount amount, std::string_view context);
    void creditCredit(AccountState& account, const std::string& vaultId, Amount amount, std::string_view context);

    void issueDeposit(
        OperationKind kind,
        AccountState& account,
        VaultState& vault,
        Amount amount,
        Amount feeBps,
        std::string_view eventType,
        std::string note);
    RedemptionTicket& appendTicket(RedemptionTicket ticket);
    LiquidationRecord& appendLiquidation(LiquidationRecord record);
    void emit(
        std::string type,
        std::string accountId,
        std::string vaultId,
        std::string assetId,
        Amount amount,
        Amount credits,
        std::string note);

    std::string name_{"cobalt"};
    Epoch epoch_{0};
    std::map<std::string, AssetDef> assets_;
    std::map<std::string, VaultState> vaults_;
    std::map<std::string, AccountState> accounts_;
    std::vector<RedemptionTicket> tickets_;
    std::vector<LiquidationRecord> liquidations_;
    std::vector<Event> events_;
    Metrics metrics_;
    PricingService pricing_;
    RiskEngine risk_;
    IdAllocator ticketIds_{"rdm"};
    IdAllocator liquidationIds_{"liq"};
    std::uint64_t nextEventSeq_{0};
};

}  // namespace cobaltdtl

