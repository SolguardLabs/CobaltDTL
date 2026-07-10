#include "ledger.hpp"

namespace cobaltdtl {

Ledger::Ledger() : risk_(pricing_) {}

Ledger::Ledger(const Scenario& scenario) : name_(scenario.name), risk_(pricing_) {
    for (const auto& asset : scenario.assets) {
        assets_.emplace(asset.id, asset);
    }
    for (const auto& vault : scenario.vaults) {
        vaults_.emplace(vault.id, vault);
    }
    for (const auto& account : scenario.accounts) {
        accounts_.emplace(account.id, account);
    }
    for (const auto& ticket : scenario.tickets) {
        ticketIds_.observe(ticket.id);
        tickets_.push_back(ticket);
    }
    for (const auto& id : collectScenarioIds(scenario)) {
        ticketIds_.observe(id);
        liquidationIds_.observe(id);
    }
    emit("init", "", "", "", 0, 0, "scenario loaded");
}

const std::string& Ledger::name() const {
    return name_;
}

Epoch Ledger::epoch() const {
    return epoch_;
}

const std::map<std::string, AssetDef>& Ledger::assets() const {
    return assets_;
}

const std::map<std::string, VaultState>& Ledger::vaults() const {
    return vaults_;
}

const std::map<std::string, AccountState>& Ledger::accounts() const {
    return accounts_;
}

const std::vector<RedemptionTicket>& Ledger::tickets() const {
    return tickets_;
}

const std::vector<LiquidationRecord>& Ledger::liquidations() const {
    return liquidations_;
}

const std::vector<Event>& Ledger::events() const {
    return events_;
}

const Metrics& Ledger::metrics() const {
    return metrics_;
}

const AssetDef& Ledger::asset(const std::string& id) const {
    return requireConst(assets_, id, "asset");
}

const VaultState& Ledger::vault(const std::string& id) const {
    return requireConst(vaults_, id, "vault");
}

const AccountState& Ledger::account(const std::string& id) const {
    return requireConst(accounts_, id, "account");
}

void Ledger::apply(const Operation& op) {
    switch (op.kind) {
        case OperationKind::Deposit:
            deposit(op.accountId, op.vaultId, op.amount, op.note);
            break;
        case OperationKind::Mint:
            mint(op.accountId, op.vaultId, op.reserve, op.note);
            break;
        case OperationKind::Redeem:
            requestRedemption(op.accountId, op.vaultId, op.credits, op.minPayout, op.id, op.note);
            break;
        case OperationKind::Advance:
            advance(op.epochs > 0 ? op.epochs : 1);
            break;
        case OperationKind::SetIndex:
            setIndex(op.vaultId, op.index, op.note);
            break;
        case OperationKind::Rebalance:
            rebalance(op.vaultId, op.amount, op.note);
            break;
        case OperationKind::Settle:
            (void)settleRedemptions(op.vaultId, op.limit, op.note);
            break;
        case OperationKind::Liquidate:
            liquidate(op.accountId, op.vaultId, op.reserve, op.penaltyBps, op.id, op.note);
            break;
        case OperationKind::TransferCredit:
            transferCredit(op.accountId, op.toAccountId, op.vaultId, op.credits, op.note);
            break;
        case OperationKind::SweepFees:
            sweepFees(op.accountId, op.vaultId, op.amount, op.note);
            break;
        case OperationKind::Checkpoint:
            checkpoint(op.note);
            break;
    }
}

void Ledger::advance(Epoch epochs) {
    if (epochs <= 0) {
        fail(ErrorCode::Validation, "epoch advance must be positive");
    }
    epoch_ = checkedAdd(epoch_, epochs, "epoch advance");
    emit("epoch", "", "", "", epochs, 0, "time advanced");
}

void Ledger::deposit(const std::string& accountId, const std::string& vaultId, Amount amount, std::string note) {
    AccountState& accountState = mutableAccount(accountId);
    VaultState& vaultState = mutableVault(vaultId);
    issueDeposit(OperationKind::Deposit, accountState, vaultState, amount, vaultState.policy.mintFeeBps, "deposit", note);
}

void Ledger::mint(const std::string& accountId, const std::string& vaultId, Amount reserveAmount, std::string note) {
    AccountState& accountState = mutableAccount(accountId);
    VaultState& vaultState = mutableVault(vaultId);
    issueDeposit(OperationKind::Mint, accountState, vaultState, reserveAmount, vaultState.policy.mintFeeBps, "mint", note);
}

void Ledger::requestRedemption(
    const std::string& accountId,
    const std::string& vaultId,
    Amount credits,
    Amount minPayout,
    std::string ticketId,
    std::string note) {
    AccountState& accountState = mutableAccount(accountId);
    VaultState& vaultState = mutableVault(vaultId);
    risk_.checkRedemptionRequest(vaultState, credits);
    debitCredit(accountState, vaultId, credits, "redemption request");
    vaultState.pendingCredits = checkedAdd(vaultState.pendingCredits, credits, "pending credits");
    RedemptionTicket ticket;
    ticket.id = ticketId.empty() ? ticketIds_.nextWith(vaultId) : requireIdentifier("ticket id", ticketId);
    ticket.vaultId = vaultId;
    ticket.accountId = accountId;
    ticket.credits = credits;
    ticket.minPayout = minPayout;
    ticket.quotedIndex = vaultState.index;
    ticket.requestedEpoch = epoch_;
    ticket.unlockEpoch = checkedAdd(epoch_, vaultState.policy.redemptionDelay, "redemption unlock");
    ticket.status = TicketStatus::Queued;
    appendTicket(ticket);
    metrics_.redemptionsQueued = checkedAdd(metrics_.redemptionsQueued, credits, "redemptions queued");
    emit("redemption_requested", accountId, vaultId, vaultState.assetId, 0, credits, std::move(note));
}

int Ledger::settleRedemptions(const std::string& vaultId, int limit, std::string note) {
    VaultState& vaultState = mutableVault(vaultId);
    const int effectiveLimit = limit > 0 ? limit : vaultState.policy.batchLimit;
    int settled = 0;
    int deferred = 0;
    metrics_.batches += 1;
    for (RedemptionTicket& ticket : tickets_) {
        if (settled >= effectiveLimit) {
            break;
        }
        if (ticket.vaultId != vaultId) {
            continue;
        }
        if (ticket.status != TicketStatus::Queued && ticket.status != TicketStatus::Deferred) {
            continue;
        }
        if (ticket.unlockEpoch > epoch_) {
            continue;
        }
        AccountState& accountState = mutableAccount(ticket.accountId);
        const PriceSnapshot price = pricing_.snapshot(vaultState);
        const Amount grossPayout = mulDivFloor(ticket.credits, price.price, kScale, "redemption payout");
        const Amount fee = applyBpsFloor(grossPayout, vaultState.policy.redemptionFeeBps, "redemption fee");
        const Amount netPayout = checkedSub(grossPayout, fee, "redemption net");
        if (ticket.minPayout > 0 && netPayout < ticket.minPayout) {
            ticket.status = TicketStatus::Deferred;
            ++deferred;
            emit("redemption_deferred", ticket.accountId, vaultId, vaultState.assetId, netPayout, ticket.credits, "min payout");
            continue;
        }
        if (grossPayout > vaultState.policy.maxBatchPayout) {
            ticket.status = TicketStatus::Deferred;
            ++deferred;
            emit("redemption_deferred", ticket.accountId, vaultId, vaultState.assetId, grossPayout, ticket.credits, "batch limit");
            continue;
        }
        vaultState.realReserve = checkedSub(vaultState.realReserve, netPayout, "redemption reserve");
        vaultState.protocolFees = checkedAdd(vaultState.protocolFees, fee, "redemption fee reserve");
        vaultState.pendingCredits = checkedSub(vaultState.pendingCredits, ticket.credits, "pending settlement");
        vaultState.issuedCredits = checkedSub(vaultState.issuedCredits, ticket.credits, "issued settlement");
        vaultState.lastPrice = price.price;
        creditReserve(accountState, vaultState.assetId, netPayout, "redemption payout");
        accountState.reserveOut = checkedAdd(accountState.reserveOut, netPayout, "account reserve out");
        accountState.feesPaid = checkedAdd(accountState.feesPaid, fee, "account redemption fee");
        ticket.payout = netPayout;
        ticket.fee = fee;
        ticket.settlementPrice = price.price;
        ticket.status = TicketStatus::Settled;
        metrics_.redemptionPayout = checkedAdd(metrics_.redemptionPayout, netPayout, "redemption metrics");
        metrics_.redemptionFees = checkedAdd(metrics_.redemptionFees, fee, "redemption fee metrics");
        metrics_.ticketsSettled += 1;
        ++settled;
        emit("redemption_settled", ticket.accountId, vaultId, vaultState.assetId, netPayout, ticket.credits, note);
    }
    metrics_.ticketsDeferred += deferred;
    if (settled == 0 && deferred == 0) {
        emit("settlement_empty", "", vaultId, vaultState.assetId, 0, 0, note);
    }
    return settled;
}

void Ledger::setIndex(const std::string& vaultId, Amount index, std::string note) {
    if (index <= 0) {
        fail(ErrorCode::Validation, "index must be positive");
    }
    VaultState& vaultState = mutableVault(vaultId);
    const Amount previous = vaultState.index;
    vaultState.index = index;
    emit("index_update", "", vaultId, vaultState.assetId, index, previous, std::move(note));
}

void Ledger::rebalance(const std::string& vaultId, Amount reserveDelta, std::string note) {
    VaultState& vaultState = mutableVault(vaultId);
    vaultState.realReserve = checkedAdd(vaultState.realReserve, reserveDelta, "reserve rebalance");
    metrics_.reserveRebalance = checkedAdd(metrics_.reserveRebalance, reserveDelta, "rebalance metrics");
    emit("rebalance", "", vaultId, vaultState.assetId, reserveDelta, 0, std::move(note));
}

void Ledger::liquidate(
    const std::string& accountId,
    const std::string& vaultId,
    Amount reserveAmount,
    Amount penaltyBps,
    std::string recordId,
    std::string note) {
    AccountState& accountState = mutableAccount(accountId);
    VaultState& vaultState = mutableVault(vaultId);
    risk_.checkLiquidation(vaultState, reserveAmount);
    if (penaltyBps < 0) {
        penaltyBps = vaultState.policy.liquidationPenaltyBps;
    }
    if (penaltyBps > kBps) {
        fail(ErrorCode::Validation, "liquidation penalty too high");
    }
    const PriceSnapshot price = pricing_.snapshot(vaultState);
    Amount credits = mulDivFloor(reserveAmount, kScale, price.price, "liquidation credits");
    const Amount penaltyCredits = applyBpsFloor(credits, penaltyBps, "liquidation penalty");
    credits = checkedAdd(credits, penaltyCredits, "liquidation credit total");
    const Amount availableCredits = accountState.credits[vaultId];
    if (availableCredits <= 0) {
        fail(ErrorCode::InsufficientBalance, "account has no credits to liquidate: " + accountId);
    }
    const Amount burnCredits = minAmount(availableCredits, credits);
    debitReserve(accountState, vaultState.assetId, reserveAmount, "liquidation reserve");
    debitCredit(accountState, vaultId, burnCredits, "liquidation credit burn");
    vaultState.realReserve = checkedAdd(vaultState.realReserve, reserveAmount, "liquidation reserve");
    vaultState.issuedCredits = checkedSub(vaultState.issuedCredits, burnCredits, "liquidation issued credits");
    accountState.reserveIn = checkedAdd(accountState.reserveIn, reserveAmount, "account reserve in");
    accountState.liquidatedCredits = checkedAdd(accountState.liquidatedCredits, burnCredits, "account liquidated credits");
    LiquidationRecord record;
    record.id = recordId.empty() ? liquidationIds_.nextWith(vaultId) : requireIdentifier("liquidation id", recordId);
    record.vaultId = vaultId;
    record.accountId = accountId;
    record.reserveIn = reserveAmount;
    record.creditsBurned = burnCredits;
    record.penaltyCredits = penaltyCredits;
    record.price = price.price;
    record.epoch = epoch_;
    record.status = LiquidationStatus::Applied;
    appendLiquidation(record);
    metrics_.liquidations = checkedAdd(metrics_.liquidations, reserveAmount, "liquidation metrics");
    metrics_.liquidatedCredits = checkedAdd(metrics_.liquidatedCredits, burnCredits, "liquidation credit metrics");
    emit("liquidation", accountId, vaultId, vaultState.assetId, reserveAmount, burnCredits, std::move(note));
}

void Ledger::transferCredit(
    const std::string& fromAccount,
    const std::string& toAccount,
    const std::string& vaultId,
    Amount credits,
    std::string note) {
    if (credits <= 0) {
        fail(ErrorCode::Validation, "transfer credits must be positive");
    }
    AccountState& from = mutableAccount(fromAccount);
    AccountState& to = mutableAccount(toAccount);
    const VaultState& vaultState = vault(vaultId);
    debitCredit(from, vaultId, credits, "credit transfer");
    creditCredit(to, vaultId, credits, "credit transfer");
    metrics_.transfers = checkedAdd(metrics_.transfers, credits, "transfer metrics");
    emit("credit_transfer", fromAccount, vaultId, vaultState.assetId, 0, credits, std::move(note));
}

void Ledger::sweepFees(const std::string& accountId, const std::string& vaultId, Amount amount, std::string note) {
    AccountState& accountState = mutableAccount(accountId);
    VaultState& vaultState = mutableVault(vaultId);
    Amount sweep = amount > 0 ? amount : vaultState.protocolFees;
    if (sweep <= 0) {
        emit("fee_sweep_empty", accountId, vaultId, vaultState.assetId, 0, 0, std::move(note));
        return;
    }
    if (sweep > vaultState.protocolFees) {
        fail(ErrorCode::InsufficientBalance, "fee sweep exceeds accrued fees");
    }
    vaultState.protocolFees = checkedSub(vaultState.protocolFees, sweep, "fee sweep");
    vaultState.realReserve = checkedSub(vaultState.realReserve, sweep, "fee sweep reserve");
    creditReserve(accountState, vaultState.assetId, sweep, "fee sweep payout");
    metrics_.feesSwept = checkedAdd(metrics_.feesSwept, sweep, "fee sweep metrics");
    emit("fee_sweep", accountId, vaultId, vaultState.assetId, sweep, 0, std::move(note));
}

void Ledger::checkpoint(std::string note) {
    emit("checkpoint", "", "", "", 0, 0, std::move(note));
}

std::vector<ReconciliationLine> Ledger::reconcile() const {
    std::vector<ReconciliationLine> lines;
    for (const auto& item : vaults_) {
        ReconciliationLine line;
        line.vaultId = item.first;
        line.accountCredits = accountCreditSum(item.first);
        line.queuedCredits = queuedCreditSum(item.first);
        line.issuedCredits = item.second.issuedCredits;
        line.reserve = item.second.realReserve;
        lines.push_back(line);
    }
    return lines;
}

Amount Ledger::accountCreditSum(const std::string& vaultId) const {
    RunningTotal total;
    for (const auto& item : accounts_) {
        const auto it = item.second.credits.find(vaultId);
        if (it != item.second.credits.end()) {
            total.add(it->second, "account credit sum");
        }
    }
    return total.value;
}

Amount Ledger::queuedCreditSum(const std::string& vaultId) const {
    RunningTotal total;
    for (const auto& ticket : tickets_) {
        if (ticket.vaultId == vaultId && (ticket.status == TicketStatus::Queued || ticket.status == TicketStatus::Deferred)) {
            total.add(ticket.credits, "queued credit sum");
        }
    }
    return total.value;
}

Amount Ledger::reserveByAsset(const std::string& assetId) const {
    RunningTotal total;
    for (const auto& item : vaults_) {
        if (item.second.assetId == assetId) {
            total.add(item.second.realReserve, "asset reserve");
        }
    }
    return total.value;
}

VaultState& Ledger::mutableVault(const std::string& id) {
    return requireMutable(vaults_, id, "vault");
}

AccountState& Ledger::mutableAccount(const std::string& id) {
    return requireMutable(accounts_, id, "account");
}

Amount& Ledger::reserveSlot(AccountState& accountState, const std::string& assetId) {
    (void)asset(assetId);
    return accountState.reserves[assetId];
}

Amount& Ledger::creditSlot(AccountState& accountState, const std::string& vaultId) {
    (void)vault(vaultId);
    return accountState.credits[vaultId];
}

void Ledger::debitReserve(AccountState& accountState, const std::string& assetId, Amount amount, std::string_view context) {
    if (amount < 0) {
        fail(ErrorCode::Validation, "cannot debit negative reserve");
    }
    Amount& slot = reserveSlot(accountState, assetId);
    if (slot < amount) {
        fail(ErrorCode::InsufficientBalance, "insufficient reserve balance for " + accountState.id);
    }
    slot = checkedSub(slot, amount, context);
}

void Ledger::creditReserve(AccountState& accountState, const std::string& assetId, Amount amount, std::string_view context) {
    if (amount < 0) {
        fail(ErrorCode::Validation, "cannot credit negative reserve");
    }
    Amount& slot = reserveSlot(accountState, assetId);
    slot = checkedAdd(slot, amount, context);
}

void Ledger::debitCredit(AccountState& accountState, const std::string& vaultId, Amount amount, std::string_view context) {
    if (amount < 0) {
        fail(ErrorCode::Validation, "cannot debit negative credit");
    }
    Amount& slot = creditSlot(accountState, vaultId);
    if (slot < amount) {
        fail(ErrorCode::InsufficientBalance, "insufficient vault credits for " + accountState.id);
    }
    slot = checkedSub(slot, amount, context);
}

void Ledger::creditCredit(AccountState& accountState, const std::string& vaultId, Amount amount, std::string_view context) {
    if (amount < 0) {
        fail(ErrorCode::Validation, "cannot credit negative credit");
    }
    Amount& slot = creditSlot(accountState, vaultId);
    slot = checkedAdd(slot, amount, context);
}

void Ledger::issueDeposit(
    OperationKind kind,
    AccountState& accountState,
    VaultState& vaultState,
    Amount amount,
    Amount feeBps,
    std::string_view eventType,
    std::string note) {
    if (amount <= 0) {
        fail(ErrorCode::Validation, "deposit amount must be positive");
    }
    risk_.checkDeposit(vaultState, amount);
    const Amount fee = applyBpsFloor(amount, feeBps, "deposit fee");
    const Amount netReserve = checkedSub(amount, fee, "deposit net");
    if (netReserve <= 0) {
        fail(ErrorCode::Validation, "deposit net amount must be positive");
    }
    const Amount credits = pricing_.creditsForReserve(vaultState, netReserve);
    if (credits <= 0) {
        fail(ErrorCode::Validation, "deposit produces zero credits");
    }
    debitReserve(accountState, vaultState.assetId, amount, "deposit reserve");
    vaultState.realReserve = checkedAdd(vaultState.realReserve, amount, "vault reserve");
    vaultState.protocolFees = checkedAdd(vaultState.protocolFees, fee, "deposit protocol fee");
    vaultState.issuedCredits = checkedAdd(vaultState.issuedCredits, credits, "issued credits");
    vaultState.lastPrice = pricing_.snapshot(vaultState).price;
    creditCredit(accountState, vaultState.id, credits, "minted credits");
    accountState.feesPaid = checkedAdd(accountState.feesPaid, fee, "account fees paid");
    metrics_.deposits = checkedAdd(metrics_.deposits, amount, "deposit metrics");
    metrics_.mintedCredits = checkedAdd(metrics_.mintedCredits, credits, "mint metrics");
    const std::string effectiveNote = note.empty() ? operationKindName(kind) : note;
    emit(std::string(eventType), accountState.id, vaultState.id, vaultState.assetId, amount, credits, effectiveNote);
}

RedemptionTicket& Ledger::appendTicket(RedemptionTicket ticket) {
    ticketIds_.observe(ticket.id);
    tickets_.push_back(std::move(ticket));
    return tickets_.back();
}

LiquidationRecord& Ledger::appendLiquidation(LiquidationRecord record) {
    liquidationIds_.observe(record.id);
    liquidations_.push_back(std::move(record));
    return liquidations_.back();
}

void Ledger::emit(
    std::string type,
    std::string accountId,
    std::string vaultId,
    std::string assetId,
    Amount amount,
    Amount credits,
    std::string note) {
    Event event;
    event.seq = ++nextEventSeq_;
    event.epoch = epoch_;
    event.type = std::move(type);
    event.accountId = std::move(accountId);
    event.vaultId = std::move(vaultId);
    event.assetId = std::move(assetId);
    event.amount = amount;
    event.credits = credits;
    event.note = std::move(note);
    events_.push_back(std::move(event));
}

}  // namespace cobaltdtl

