#pragma once

#include "json.hpp"

#include <set>

namespace cobaltdtl {

enum class TicketStatus {
    Queued,
    Deferred,
    Settled,
    Cancelled
};

enum class LiquidationStatus {
    Planned,
    Applied,
    Rejected
};

enum class OperationKind {
    Deposit,
    Mint,
    Redeem,
    Advance,
    SetIndex,
    Rebalance,
    Settle,
    Liquidate,
    TransferCredit,
    SweepFees,
    Checkpoint
};

std::string ticketStatusName(TicketStatus status);
std::string liquidationStatusName(LiquidationStatus status);
std::string operationKindName(OperationKind kind);
TicketStatus parseTicketStatus(std::string_view value);
OperationKind parseOperationKind(std::string_view value);

struct AssetDef {
    std::string id;
    std::string symbol;
    int decimals{6};
    Amount haircutBps{0};
    Amount dust{0};
};

struct VaultPolicy {
    Amount depositCap{kNoLimit};
    Amount minDeposit{1};
    Amount maxTicketCredits{kNoLimit};
    Amount minLiquidity{0};
    Amount maxBatchPayout{kNoLimit};
    Amount mintFeeBps{0};
    Amount redemptionFeeBps{0};
    Amount liquidationPenaltyBps{500};
    Amount reserveHaircutBps{0};
    Amount redemptionShockBps{2'500};
    Amount operationalBufferBps{50};
    Amount minimumCapitalCoverageBps{10'500};
    Amount minimumLiquidityCoverageBps{10'000};
    Amount maturityEpochs{1};
    int redemptionDelay{1};
    int batchLimit{50};
    bool allowDeposits{true};
    bool allowRedemptions{true};
    bool allowLiquidations{true};
};

struct VaultState {
    std::string id;
    std::string assetId;
    std::string label;
    Amount index{kScale};
    Amount realReserve{0};
    Amount issuedCredits{0};
    Amount pendingCredits{0};
    Amount protocolFees{0};
    Amount insuranceBuffer{0};
    Amount lastPrice{kScale};
    VaultPolicy policy;
};

struct AccountState {
    std::string id;
    std::map<std::string, Amount> reserves;
    std::map<std::string, Amount> credits;
    Amount feesPaid{0};
    Amount reserveOut{0};
    Amount reserveIn{0};
    Amount liquidatedCredits{0};
};

struct RedemptionTicket {
    std::string id;
    std::string vaultId;
    std::string accountId;
    Amount credits{0};
    Amount minPayout{0};
    Amount payout{0};
    Amount fee{0};
    Amount quotedIndex{0};
    Amount settlementPrice{0};
    Epoch requestedEpoch{0};
    Epoch unlockEpoch{0};
    TicketStatus status{TicketStatus::Queued};
};

struct LiquidationRecord {
    std::string id;
    std::string vaultId;
    std::string accountId;
    Amount reserveIn{0};
    Amount creditsBurned{0};
    Amount penaltyCredits{0};
    Amount price{0};
    Epoch epoch{0};
    LiquidationStatus status{LiquidationStatus::Planned};
};

struct Event {
    std::uint64_t seq{0};
    Epoch epoch{0};
    std::string type;
    std::string accountId;
    std::string vaultId;
    std::string assetId;
    Amount amount{0};
    Amount credits{0};
    std::string note;
};

struct Metrics {
    Amount deposits{0};
    Amount mintedCredits{0};
    Amount redemptionsQueued{0};
    Amount redemptionPayout{0};
    Amount redemptionFees{0};
    Amount liquidations{0};
    Amount liquidatedCredits{0};
    Amount reserveRebalance{0};
    Amount transfers{0};
    Amount feesSwept{0};
    int batches{0};
    int ticketsSettled{0};
    int ticketsDeferred{0};
};

struct Operation {
    OperationKind kind{OperationKind::Checkpoint};
    std::string id;
    std::string accountId;
    std::string toAccountId;
    std::string vaultId;
    std::string assetId;
    Amount amount{0};
    Amount reserve{0};
    Amount credits{0};
    Amount index{0};
    Amount minPayout{0};
    Amount penaltyBps{-1};
    int epochs{0};
    int limit{0};
    std::string note;
};

struct Scenario {
    std::string name;
    std::vector<AssetDef> assets;
    std::vector<AccountState> accounts;
    std::vector<VaultState> vaults;
    std::vector<RedemptionTicket> tickets;
    std::vector<Operation> operations;
};

AssetDef parseAsset(const JsonValue& value, const FieldPath& path);
VaultPolicy parseVaultPolicy(const JsonValue& value, const FieldPath& path);
VaultState parseVault(const JsonValue& value, const FieldPath& path);
AccountState parseAccount(const JsonValue& value, const FieldPath& path);
RedemptionTicket parseTicket(const JsonValue& value, const FieldPath& path);
Operation parseOperation(const JsonValue& value, const FieldPath& path);
Scenario parseScenario(const JsonValue& root);

void validateScenarioShape(const Scenario& scenario);
std::map<std::string, Amount> parseAmountMap(const JsonValue& value, const FieldPath& path);
std::vector<std::string> collectScenarioIds(const Scenario& scenario);

}  // namespace cobaltdtl

