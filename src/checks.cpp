#include "checks.hpp"

#include <set>

namespace cobaltdtl {

std::string checkSeverityName(CheckSeverity severity) {
    switch (severity) {
        case CheckSeverity::Info:
            return "info";
        case CheckSeverity::Warning:
            return "warning";
        case CheckSeverity::Error:
            return "error";
    }
    return "unknown";
}

bool CheckReport::ok() const {
    return errorCount == 0;
}

void CheckReport::add(CheckIssue issue) {
    switch (issue.severity) {
        case CheckSeverity::Info:
            ++infoCount;
            break;
        case CheckSeverity::Warning:
            ++warningCount;
            break;
        case CheckSeverity::Error:
            ++errorCount;
            break;
    }
    issues.push_back(std::move(issue));
}

CheckReport SanityChecker::run(const Ledger& ledger) const {
    CheckReport report;
    checkVaultReferences(ledger, report);
    checkAccountReferences(ledger, report);
    checkCreditConservation(ledger, report);
    checkTicketAccounting(ledger, report);
    checkLiquidationRecords(ledger, report);
    checkReserveSurfaces(ledger, report);
    checkPolicyRanges(ledger, report);
    checkEventOrdering(ledger, report);
    if (report.issues.empty()) {
        addIssue(report, CheckSeverity::Info, "clean", "ledger", ledger.name(), "ledger checks completed");
    }
    return report;
}

void SanityChecker::checkVaultReferences(const Ledger& ledger, CheckReport& report) const {
    for (const auto& item : ledger.vaults()) {
        const VaultState& vault = item.second;
        if (!containsKey(ledger.assets(), vault.assetId)) {
            addIssue(
                report,
                CheckSeverity::Error,
                "vault_asset_missing",
                "vault",
                vault.id,
                "vault references an unknown asset");
        }
        if (vault.id.empty()) {
            addIssue(report, CheckSeverity::Error, "vault_id_empty", "vault", vault.id, "vault id is empty");
        }
        if (vault.assetId.empty()) {
            addIssue(report, CheckSeverity::Error, "vault_asset_empty", "vault", vault.id, "vault asset is empty");
        }
        if (vault.index <= 0) {
            addIssue(
                report,
                CheckSeverity::Error,
                "vault_index_nonpositive",
                "vault",
                vault.id,
                "vault index must be positive",
                vault.index,
                kScale);
        }
        if (vault.pendingCredits > vault.issuedCredits) {
            addIssue(
                report,
                CheckSeverity::Error,
                "pending_exceeds_issued",
                "vault",
                vault.id,
                "pending credits exceed issued credits",
                vault.pendingCredits,
                vault.issuedCredits);
        }
    }
}

void SanityChecker::checkAccountReferences(const Ledger& ledger, CheckReport& report) const {
    for (const auto& item : ledger.accounts()) {
        const AccountState& account = item.second;
        if (account.id.empty()) {
            addIssue(report, CheckSeverity::Error, "account_id_empty", "account", account.id, "account id is empty");
        }
        for (const auto& reserve : account.reserves) {
            if (!containsKey(ledger.assets(), reserve.first)) {
                addIssue(
                    report,
                    CheckSeverity::Error,
                    "account_asset_missing",
                    "account",
                    account.id,
                    "account references an unknown reserve asset");
            }
            if (reserve.second < 0) {
                addIssue(
                    report,
                    CheckSeverity::Error,
                    "account_reserve_negative",
                    "account",
                    account.id,
                    "account reserve is negative",
                    reserve.second,
                    0);
            }
        }
        for (const auto& credit : account.credits) {
            if (!containsKey(ledger.vaults(), credit.first)) {
                addIssue(
                    report,
                    CheckSeverity::Error,
                    "account_vault_missing",
                    "account",
                    account.id,
                    "account references an unknown vault credit");
            }
            if (credit.second < 0) {
                addIssue(
                    report,
                    CheckSeverity::Error,
                    "account_credit_negative",
                    "account",
                    account.id,
                    "account credit balance is negative",
                    credit.second,
                    0);
            }
        }
    }
}

void SanityChecker::checkCreditConservation(const Ledger& ledger, CheckReport& report) const {
    for (const auto& item : ledger.vaults()) {
        const VaultState& vault = item.second;
        const Amount accountCredits = ledger.accountCreditSum(vault.id);
        const Amount queuedCredits = ledger.queuedCreditSum(vault.id);
        const Amount totalCredits = checkedAdd(accountCredits, queuedCredits, "credit conservation check");
        if (totalCredits != vault.issuedCredits) {
            addIssue(
                report,
                CheckSeverity::Error,
                "credit_conservation",
                "vault",
                vault.id,
                "account credits plus queued credits do not match issued credits",
                totalCredits,
                vault.issuedCredits);
        }
        if (queuedCredits != vault.pendingCredits) {
            addIssue(
                report,
                CheckSeverity::Error,
                "queue_conservation",
                "vault",
                vault.id,
                "queued ticket credits do not match vault pending credits",
                queuedCredits,
                vault.pendingCredits);
        }
    }
}

void SanityChecker::checkTicketAccounting(const Ledger& ledger, CheckReport& report) const {
    std::set<std::string> ticketIds;
    for (const RedemptionTicket& ticket : ledger.tickets()) {
        if (!ticketIds.insert(ticket.id).second) {
            addIssue(
                report,
                CheckSeverity::Error,
                "ticket_duplicate",
                "ticket",
                ticket.id,
                "ticket id is duplicated");
        }
        if (!containsKey(ledger.vaults(), ticket.vaultId)) {
            addIssue(
                report,
                CheckSeverity::Error,
                "ticket_vault_missing",
                "ticket",
                ticket.id,
                "ticket references an unknown vault");
        }
        if (!containsKey(ledger.accounts(), ticket.accountId)) {
            addIssue(
                report,
                CheckSeverity::Error,
                "ticket_account_missing",
                "ticket",
                ticket.id,
                "ticket references an unknown account");
        }
        if (ticket.credits <= 0) {
            addIssue(
                report,
                CheckSeverity::Error,
                "ticket_credits_nonpositive",
                "ticket",
                ticket.id,
                "ticket credits must be positive",
                ticket.credits,
                1);
        }
        if (ticket.unlockEpoch < ticket.requestedEpoch) {
            addIssue(
                report,
                CheckSeverity::Error,
                "ticket_epoch_order",
                "ticket",
                ticket.id,
                "ticket unlock epoch is before request epoch",
                ticket.unlockEpoch,
                ticket.requestedEpoch);
        }
        if (ticket.status == TicketStatus::Settled && ticket.payout < 0) {
            addIssue(
                report,
                CheckSeverity::Error,
                "ticket_payout_negative",
                "ticket",
                ticket.id,
                "settled ticket payout is negative",
                ticket.payout,
                0);
        }
        if (ticket.status == TicketStatus::Settled && ticket.settlementPrice <= 0) {
            addIssue(
                report,
                CheckSeverity::Warning,
                "ticket_price_missing",
                "ticket",
                ticket.id,
                "settled ticket does not carry a settlement price");
        }
    }
}

void SanityChecker::checkLiquidationRecords(const Ledger& ledger, CheckReport& report) const {
    std::set<std::string> recordIds;
    for (const LiquidationRecord& record : ledger.liquidations()) {
        if (!recordIds.insert(record.id).second) {
            addIssue(
                report,
                CheckSeverity::Error,
                "liquidation_duplicate",
                "liquidation",
                record.id,
                "liquidation id is duplicated");
        }
        if (!containsKey(ledger.vaults(), record.vaultId)) {
            addIssue(
                report,
                CheckSeverity::Error,
                "liquidation_vault_missing",
                "liquidation",
                record.id,
                "liquidation references an unknown vault");
        }
        if (!containsKey(ledger.accounts(), record.accountId)) {
            addIssue(
                report,
                CheckSeverity::Error,
                "liquidation_account_missing",
                "liquidation",
                record.id,
                "liquidation references an unknown account");
        }
        if (record.reserveIn < 0 || record.creditsBurned < 0 || record.penaltyCredits < 0) {
            addIssue(
                report,
                CheckSeverity::Error,
                "liquidation_negative_amount",
                "liquidation",
                record.id,
                "liquidation contains a negative amount");
        }
        if (record.status == LiquidationStatus::Applied && record.price <= 0) {
            addIssue(
                report,
                CheckSeverity::Error,
                "liquidation_price_nonpositive",
                "liquidation",
                record.id,
                "applied liquidation price must be positive",
                record.price,
                kScale);
        }
    }
}

void SanityChecker::checkReserveSurfaces(const Ledger& ledger, CheckReport& report) const {
    for (const auto& item : ledger.vaults()) {
        const VaultState& vault = item.second;
        if (vault.realReserve < 0) {
            addIssue(
                report,
                CheckSeverity::Error,
                "vault_reserve_negative",
                "vault",
                vault.id,
                "vault reserve is negative",
                vault.realReserve,
                0);
        }
        if (vault.protocolFees < 0) {
            addIssue(
                report,
                CheckSeverity::Error,
                "vault_fees_negative",
                "vault",
                vault.id,
                "vault fees are negative",
                vault.protocolFees,
                0);
        }
        if (vault.protocolFees > vault.realReserve && vault.realReserve >= 0) {
            addIssue(
                report,
                CheckSeverity::Warning,
                "fees_exceed_reserve",
                "vault",
                vault.id,
                "protocol fees exceed visible reserve",
                vault.protocolFees,
                vault.realReserve);
        }
    }
}

void SanityChecker::checkPolicyRanges(const Ledger& ledger, CheckReport& report) const {
    for (const auto& item : ledger.vaults()) {
        const VaultState& vault = item.second;
        const VaultPolicy& policy = vault.policy;
        if (policy.mintFeeBps < 0 || policy.mintFeeBps > kBps) {
            addIssue(
                report,
                CheckSeverity::Error,
                "mint_fee_range",
                "vault",
                vault.id,
                "mint fee bps is outside range",
                policy.mintFeeBps,
                kBps);
        }
        if (policy.redemptionFeeBps < 0 || policy.redemptionFeeBps > kBps) {
            addIssue(
                report,
                CheckSeverity::Error,
                "redemption_fee_range",
                "vault",
                vault.id,
                "redemption fee bps is outside range",
                policy.redemptionFeeBps,
                kBps);
        }
        if (policy.batchLimit <= 0) {
            addIssue(
                report,
                CheckSeverity::Error,
                "batch_limit_range",
                "vault",
                vault.id,
                "batch limit must be positive",
                policy.batchLimit,
                1);
        }
        if (policy.depositCap < vault.realReserve && policy.depositCap != kNoLimit) {
            addIssue(
                report,
                CheckSeverity::Warning,
                "deposit_cap_consumed",
                "vault",
                vault.id,
                "reserve is above configured deposit cap",
                vault.realReserve,
                policy.depositCap);
        }
    }
}

void SanityChecker::checkEventOrdering(const Ledger& ledger, CheckReport& report) const {
    std::uint64_t previousSeq = 0;
    Epoch previousEpoch = 0;
    for (const Event& event : ledger.events()) {
        if (event.seq <= previousSeq) {
            addIssue(
                report,
                CheckSeverity::Error,
                "event_sequence",
                "event",
                std::to_string(event.seq),
                "event sequence is not strictly increasing",
                static_cast<Amount>(event.seq),
                static_cast<Amount>(previousSeq + 1));
        }
        if (event.epoch < previousEpoch) {
            addIssue(
                report,
                CheckSeverity::Error,
                "event_epoch_order",
                "event",
                std::to_string(event.seq),
                "event epoch moved backwards",
                event.epoch,
                previousEpoch);
        }
        previousSeq = event.seq;
        previousEpoch = event.epoch;
    }
}

void SanityChecker::addIssue(
    CheckReport& report,
    CheckSeverity severity,
    std::string code,
    std::string scope,
    std::string id,
    std::string message,
    Amount observed,
    Amount expected) const {
    CheckIssue issue;
    issue.severity = severity;
    issue.code = std::move(code);
    issue.scope = std::move(scope);
    issue.id = std::move(id);
    issue.message = std::move(message);
    issue.observed = observed;
    issue.expected = expected;
    report.add(std::move(issue));
}

}  // namespace cobaltdtl

