#include "model.hpp"

namespace cobaltdtl {

std::string ticketStatusName(TicketStatus status) {
    switch (status) {
        case TicketStatus::Queued:
            return "queued";
        case TicketStatus::Deferred:
            return "deferred";
        case TicketStatus::Settled:
            return "settled";
        case TicketStatus::Cancelled:
            return "cancelled";
    }
    return "unknown";
}

std::string liquidationStatusName(LiquidationStatus status) {
    switch (status) {
        case LiquidationStatus::Planned:
            return "planned";
        case LiquidationStatus::Applied:
            return "applied";
        case LiquidationStatus::Rejected:
            return "rejected";
    }
    return "unknown";
}

std::string operationKindName(OperationKind kind) {
    switch (kind) {
        case OperationKind::Deposit:
            return "deposit";
        case OperationKind::Mint:
            return "mint";
        case OperationKind::Redeem:
            return "redeem";
        case OperationKind::Advance:
            return "advance";
        case OperationKind::SetIndex:
            return "index";
        case OperationKind::Rebalance:
            return "rebalance";
        case OperationKind::Settle:
            return "settle";
        case OperationKind::Liquidate:
            return "liquidate";
        case OperationKind::TransferCredit:
            return "transferCredit";
        case OperationKind::SweepFees:
            return "sweepFees";
        case OperationKind::Checkpoint:
            return "checkpoint";
    }
    return "unknown";
}

TicketStatus parseTicketStatus(std::string_view value) {
    const std::string clean = toLower(trim(value));
    if (clean == "queued") {
        return TicketStatus::Queued;
    }
    if (clean == "deferred") {
        return TicketStatus::Deferred;
    }
    if (clean == "settled") {
        return TicketStatus::Settled;
    }
    if (clean == "cancelled") {
        return TicketStatus::Cancelled;
    }
    fail(ErrorCode::Validation, "unknown ticket status: " + clean);
}

OperationKind parseOperationKind(std::string_view value) {
    const std::string clean = toLower(trim(value));
    if (clean == "deposit") {
        return OperationKind::Deposit;
    }
    if (clean == "mint") {
        return OperationKind::Mint;
    }
    if (clean == "redeem" || clean == "requestredemption") {
        return OperationKind::Redeem;
    }
    if (clean == "advance" || clean == "advanceepoch") {
        return OperationKind::Advance;
    }
    if (clean == "index" || clean == "setindex") {
        return OperationKind::SetIndex;
    }
    if (clean == "rebalance") {
        return OperationKind::Rebalance;
    }
    if (clean == "settle" || clean == "settleredemptions") {
        return OperationKind::Settle;
    }
    if (clean == "liquidate") {
        return OperationKind::Liquidate;
    }
    if (clean == "transfercredit" || clean == "transfer") {
        return OperationKind::TransferCredit;
    }
    if (clean == "sweepfees") {
        return OperationKind::SweepFees;
    }
    if (clean == "checkpoint" || clean == "report") {
        return OperationKind::Checkpoint;
    }
    fail(ErrorCode::Validation, "unknown operation type: " + clean);
}

static std::string readString(const JsonValue& object, std::string_view key, const FieldPath& path) {
    return object.at(key, path).asString(path.child(std::string(key)));
}

static std::string readId(const JsonValue& object, std::string_view key, const FieldPath& path) {
    return requireIdentifier(path.child(std::string(key)).str(), readString(object, key, path));
}

static std::string readOptString(const JsonValue& object, std::string_view key, const FieldPath& path, std::string fallback) {
    const JsonValue* value = object.find(key);
    if (value == nullptr || value->isNull()) {
        return fallback;
    }
    return value->asString(path.child(std::string(key)));
}

static Amount readAmount(const JsonValue& object, std::string_view key, const FieldPath& path) {
    return object.at(key, path).asAmount(path.child(std::string(key)));
}

static Amount readOptAmount(const JsonValue& object, std::string_view key, const FieldPath& path, Amount fallback) {
    const JsonValue* value = object.find(key);
    if (value == nullptr || value->isNull()) {
        return fallback;
    }
    return value->asAmount(path.child(std::string(key)));
}

static int readOptInt(const JsonValue& object, std::string_view key, const FieldPath& path, int fallback) {
    const JsonValue* value = object.find(key);
    if (value == nullptr || value->isNull()) {
        return fallback;
    }
    const Amount amount = value->asAmount(path.child(std::string(key)));
    if (amount < std::numeric_limits<int>::min() || amount > std::numeric_limits<int>::max()) {
        fail(ErrorCode::Validation, path.child(std::string(key)).str() + " is outside int range");
    }
    return static_cast<int>(amount);
}

static bool readOptBool(const JsonValue& object, std::string_view key, const FieldPath& path, bool fallback) {
    const JsonValue* value = object.find(key);
    if (value == nullptr || value->isNull()) {
        return fallback;
    }
    return value->asBool(path.child(std::string(key)));
}

std::map<std::string, Amount> parseAmountMap(const JsonValue& value, const FieldPath& path) {
    std::map<std::string, Amount> out;
    for (const auto& item : value.asObject(path)) {
        const std::string id = requireIdentifier(path.child(item.first).str(), item.first);
        const Amount amount = item.second.asAmount(path.child(item.first));
        out.emplace(id, amount);
    }
    return out;
}

AssetDef parseAsset(const JsonValue& value, const FieldPath& path) {
    AssetDef asset;
    asset.id = readId(value, "id", path);
    asset.symbol = readOptString(value, "symbol", path, toUpper(asset.id));
    asset.decimals = readOptInt(value, "decimals", path, 6);
    asset.haircutBps = readOptAmount(value, "haircutBps", path, 0);
    asset.dust = readOptAmount(value, "dust", path, 0);
    if (asset.decimals < 0 || asset.decimals > 18) {
        fail(ErrorCode::Validation, path.child("decimals").str() + " must be between 0 and 18");
    }
    if (asset.haircutBps < 0 || asset.haircutBps > kBps) {
        fail(ErrorCode::Validation, path.child("haircutBps").str() + " must be between 0 and 10000");
    }
    if (asset.dust < 0) {
        fail(ErrorCode::Validation, path.child("dust").str() + " cannot be negative");
    }
    return asset;
}

VaultPolicy parseVaultPolicy(const JsonValue& value, const FieldPath& path) {
    VaultPolicy policy;
    if (const JsonValue* policyValue = value.find("policy")) {
        const FieldPath p = path.child("policy");
        policy.depositCap = readOptAmount(*policyValue, "depositCap", p, policy.depositCap);
        policy.minDeposit = readOptAmount(*policyValue, "minDeposit", p, policy.minDeposit);
        policy.maxTicketCredits = readOptAmount(*policyValue, "maxTicketCredits", p, policy.maxTicketCredits);
        policy.minLiquidity = readOptAmount(*policyValue, "minLiquidity", p, policy.minLiquidity);
        policy.maxBatchPayout = readOptAmount(*policyValue, "maxBatchPayout", p, policy.maxBatchPayout);
        policy.mintFeeBps = readOptAmount(*policyValue, "mintFeeBps", p, policy.mintFeeBps);
        policy.redemptionFeeBps = readOptAmount(*policyValue, "redemptionFeeBps", p, policy.redemptionFeeBps);
        policy.liquidationPenaltyBps =
            readOptAmount(*policyValue, "liquidationPenaltyBps", p, policy.liquidationPenaltyBps);
        policy.reserveHaircutBps = readOptAmount(*policyValue, "reserveHaircutBps", p, policy.reserveHaircutBps);
        policy.redemptionShockBps = readOptAmount(*policyValue, "redemptionShockBps", p, policy.redemptionShockBps);
        policy.operationalBufferBps =
            readOptAmount(*policyValue, "operationalBufferBps", p, policy.operationalBufferBps);
        policy.minimumCapitalCoverageBps =
            readOptAmount(*policyValue, "minimumCapitalCoverageBps", p, policy.minimumCapitalCoverageBps);
        policy.minimumLiquidityCoverageBps =
            readOptAmount(*policyValue, "minimumLiquidityCoverageBps", p, policy.minimumLiquidityCoverageBps);
        policy.maturityEpochs = readOptAmount(*policyValue, "maturityEpochs", p, policy.maturityEpochs);
        policy.redemptionDelay = readOptInt(*policyValue, "redemptionDelay", p, policy.redemptionDelay);
        policy.batchLimit = readOptInt(*policyValue, "batchLimit", p, policy.batchLimit);
        policy.allowDeposits = readOptBool(*policyValue, "allowDeposits", p, policy.allowDeposits);
        policy.allowRedemptions = readOptBool(*policyValue, "allowRedemptions", p, policy.allowRedemptions);
        policy.allowLiquidations = readOptBool(*policyValue, "allowLiquidations", p, policy.allowLiquidations);
    }
    if (policy.minDeposit < 0 || policy.depositCap < 0 || policy.maxTicketCredits < 0 || policy.minLiquidity < 0) {
        fail(ErrorCode::Validation, path.child("policy").str() + " contains a negative limit");
    }
    if (policy.mintFeeBps < 0 || policy.mintFeeBps > kBps) {
        fail(ErrorCode::Validation, path.child("policy.mintFeeBps").str() + " must be between 0 and 10000");
    }
    if (policy.redemptionFeeBps < 0 || policy.redemptionFeeBps > kBps) {
        fail(ErrorCode::Validation, path.child("policy.redemptionFeeBps").str() + " must be between 0 and 10000");
    }
    if (policy.liquidationPenaltyBps < 0 || policy.liquidationPenaltyBps > kBps) {
        fail(ErrorCode::Validation, path.child("policy.liquidationPenaltyBps").str() + " must be between 0 and 10000");
    }
    if (policy.reserveHaircutBps < 0 || policy.reserveHaircutBps > kBps || policy.redemptionShockBps < 0 ||
        policy.operationalBufferBps < 0 || policy.minimumCapitalCoverageBps <= 0 ||
        policy.minimumLiquidityCoverageBps <= 0 || policy.maturityEpochs < 0) {
        fail(ErrorCode::Validation, path.child("policy").str() + " contains an invalid capital parameter");
    }
    if (policy.redemptionDelay < 0) {
        fail(ErrorCode::Validation, path.child("policy.redemptionDelay").str() + " cannot be negative");
    }
    if (policy.batchLimit <= 0) {
        fail(ErrorCode::Validation, path.child("policy.batchLimit").str() + " must be positive");
    }
    return policy;
}

VaultState parseVault(const JsonValue& value, const FieldPath& path) {
    VaultState vault;
    vault.id = readId(value, "id", path);
    vault.assetId = readId(value, "asset", path);
    vault.label = readOptString(value, "label", path, vault.id);
    vault.index = readOptAmount(value, "index", path, kScale);
    vault.realReserve = readOptAmount(value, "reserve", path, 0);
    vault.issuedCredits = readOptAmount(value, "issuedCredits", path, 0);
    vault.pendingCredits = readOptAmount(value, "pendingCredits", path, 0);
    vault.protocolFees = readOptAmount(value, "protocolFees", path, 0);
    vault.insuranceBuffer = readOptAmount(value, "insuranceBuffer", path, 0);
    vault.lastPrice = readOptAmount(value, "lastPrice", path, vault.index);
    vault.policy = parseVaultPolicy(value, path);
    if (vault.index <= 0) {
        fail(ErrorCode::Validation, path.child("index").str() + " must be positive");
    }
    if (vault.realReserve < 0 || vault.issuedCredits < 0 || vault.pendingCredits < 0 || vault.protocolFees < 0 ||
        vault.insuranceBuffer < 0) {
        fail(ErrorCode::Validation, path.str() + " contains a negative accounting field");
    }
    if (vault.pendingCredits > vault.issuedCredits) {
        fail(ErrorCode::Validation, path.child("pendingCredits").str() + " cannot exceed issuedCredits");
    }
    return vault;
}

AccountState parseAccount(const JsonValue& value, const FieldPath& path) {
    AccountState account;
    account.id = readId(value, "id", path);
    if (const JsonValue* reserves = value.find("reserves")) {
        account.reserves = parseAmountMap(*reserves, path.child("reserves"));
    }
    if (const JsonValue* balances = value.find("balances")) {
        account.reserves = parseAmountMap(*balances, path.child("balances"));
    }
    if (const JsonValue* credits = value.find("credits")) {
        account.credits = parseAmountMap(*credits, path.child("credits"));
    }
    account.feesPaid = readOptAmount(value, "feesPaid", path, 0);
    account.reserveOut = readOptAmount(value, "reserveOut", path, 0);
    account.reserveIn = readOptAmount(value, "reserveIn", path, 0);
    account.liquidatedCredits = readOptAmount(value, "liquidatedCredits", path, 0);
    return account;
}

RedemptionTicket parseTicket(const JsonValue& value, const FieldPath& path) {
    RedemptionTicket ticket;
    ticket.id = readId(value, "id", path);
    ticket.vaultId = readId(value, "vault", path);
    ticket.accountId = readId(value, "account", path);
    ticket.credits = readAmount(value, "credits", path);
    ticket.minPayout = readOptAmount(value, "minPayout", path, 0);
    ticket.payout = readOptAmount(value, "payout", path, 0);
    ticket.fee = readOptAmount(value, "fee", path, 0);
    ticket.quotedIndex = readOptAmount(value, "quotedIndex", path, 0);
    ticket.settlementPrice = readOptAmount(value, "settlementPrice", path, 0);
    ticket.requestedEpoch = readOptAmount(value, "requestedEpoch", path, 0);
    ticket.unlockEpoch = readOptAmount(value, "unlockEpoch", path, ticket.requestedEpoch);
    if (const JsonValue* status = value.find("status")) {
        ticket.status = parseTicketStatus(status->asString(path.child("status")));
    }
    if (ticket.credits <= 0) {
        fail(ErrorCode::Validation, path.child("credits").str() + " must be positive");
    }
    return ticket;
}

Operation parseOperation(const JsonValue& value, const FieldPath& path) {
    Operation op;
    op.kind = parseOperationKind(readString(value, "type", path));
    op.id = readOptString(value, "id", path, "");
    if (!op.id.empty()) {
        op.id = requireIdentifier(path.child("id").str(), op.id);
    }
    if (const JsonValue* account = value.find("account")) {
        op.accountId = requireIdentifier(path.child("account").str(), account->asString(path.child("account")));
    }
    if (const JsonValue* toAccount = value.find("to")) {
        op.toAccountId = requireIdentifier(path.child("to").str(), toAccount->asString(path.child("to")));
    }
    if (const JsonValue* vault = value.find("vault")) {
        op.vaultId = requireIdentifier(path.child("vault").str(), vault->asString(path.child("vault")));
    }
    if (const JsonValue* asset = value.find("asset")) {
        op.assetId = requireIdentifier(path.child("asset").str(), asset->asString(path.child("asset")));
    }
    op.amount = readOptAmount(value, "amount", path, 0);
    op.reserve = readOptAmount(value, "reserve", path, op.amount);
    op.credits = readOptAmount(value, "credits", path, 0);
    op.index = readOptAmount(value, "index", path, 0);
    op.minPayout = readOptAmount(value, "minPayout", path, 0);
    op.penaltyBps = readOptAmount(value, "penaltyBps", path, -1);
    op.epochs = readOptInt(value, "epochs", path, 0);
    op.limit = readOptInt(value, "limit", path, 0);
    op.note = readOptString(value, "note", path, "");
    return op;
}

Scenario parseScenario(const JsonValue& root) {
    const FieldPath path("scenario");
    Scenario scenario;
    scenario.name = readOptString(root, "name", path, "cobalt-scenario");
    if (const JsonValue* assets = root.find("assets")) {
        const auto& array = assets->asArray(path.child("assets"));
        for (std::size_t i = 0; i < array.size(); ++i) {
            scenario.assets.push_back(parseAsset(array[i], path.child("assets").child(std::to_string(i))));
        }
    }
    if (const JsonValue* accounts = root.find("accounts")) {
        const auto& array = accounts->asArray(path.child("accounts"));
        for (std::size_t i = 0; i < array.size(); ++i) {
            scenario.accounts.push_back(parseAccount(array[i], path.child("accounts").child(std::to_string(i))));
        }
    }
    if (const JsonValue* vaults = root.find("vaults")) {
        const auto& array = vaults->asArray(path.child("vaults"));
        for (std::size_t i = 0; i < array.size(); ++i) {
            scenario.vaults.push_back(parseVault(array[i], path.child("vaults").child(std::to_string(i))));
        }
    }
    if (const JsonValue* tickets = root.find("tickets")) {
        const auto& array = tickets->asArray(path.child("tickets"));
        for (std::size_t i = 0; i < array.size(); ++i) {
            scenario.tickets.push_back(parseTicket(array[i], path.child("tickets").child(std::to_string(i))));
        }
    }
    if (const JsonValue* operations = root.find("operations")) {
        const auto& array = operations->asArray(path.child("operations"));
        for (std::size_t i = 0; i < array.size(); ++i) {
            scenario.operations.push_back(parseOperation(array[i], path.child("operations").child(std::to_string(i))));
        }
    }
    validateScenarioShape(scenario);
    return scenario;
}

static void requireUnique(std::set<std::string>& ids, const std::string& id, std::string_view label) {
    if (!ids.insert(id).second) {
        fail(ErrorCode::Validation, std::string("duplicate ") + std::string(label) + ": " + id);
    }
}

void validateScenarioShape(const Scenario& scenario) {
    if (scenario.assets.empty()) {
        fail(ErrorCode::Validation, "scenario must define at least one asset");
    }
    if (scenario.vaults.empty()) {
        fail(ErrorCode::Validation, "scenario must define at least one vault");
    }
    std::set<std::string> assetIds;
    std::set<std::string> vaultIds;
    std::set<std::string> accountIds;
    std::set<std::string> ticketIds;
    for (const auto& asset : scenario.assets) {
        requireUnique(assetIds, asset.id, "asset");
    }
    for (const auto& vault : scenario.vaults) {
        requireUnique(vaultIds, vault.id, "vault");
        if (assetIds.find(vault.assetId) == assetIds.end()) {
            fail(ErrorCode::UnknownReference, "vault " + vault.id + " references unknown asset " + vault.assetId);
        }
    }
    for (const auto& account : scenario.accounts) {
        requireUnique(accountIds, account.id, "account");
        for (const auto& reserve : account.reserves) {
            if (assetIds.find(reserve.first) == assetIds.end()) {
                fail(ErrorCode::UnknownReference, "account " + account.id + " references unknown asset " + reserve.first);
            }
        }
        for (const auto& credit : account.credits) {
            if (vaultIds.find(credit.first) == vaultIds.end()) {
                fail(ErrorCode::UnknownReference, "account " + account.id + " references unknown vault " + credit.first);
            }
        }
    }
    for (const auto& ticket : scenario.tickets) {
        requireUnique(ticketIds, ticket.id, "ticket");
        if (vaultIds.find(ticket.vaultId) == vaultIds.end()) {
            fail(ErrorCode::UnknownReference, "ticket " + ticket.id + " references unknown vault " + ticket.vaultId);
        }
        if (accountIds.find(ticket.accountId) == accountIds.end()) {
            fail(ErrorCode::UnknownReference, "ticket " + ticket.id + " references unknown account " + ticket.accountId);
        }
    }
}

std::vector<std::string> collectScenarioIds(const Scenario& scenario) {
    std::vector<std::string> ids;
    for (const auto& asset : scenario.assets) {
        ids.push_back(asset.id);
    }
    for (const auto& vault : scenario.vaults) {
        ids.push_back(vault.id);
    }
    for (const auto& account : scenario.accounts) {
        ids.push_back(account.id);
    }
    for (const auto& ticket : scenario.tickets) {
        ids.push_back(ticket.id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

}  // namespace cobaltdtl

