#include "queue.hpp"

namespace cobaltdtl {

QueuePlanner::QueuePlanner(PricingService pricing) : pricing_(std::move(pricing)) {}

QueuePlan QueuePlanner::build(const Ledger& ledger) const {
    QueuePlan plan;
    for (const RedemptionTicket& ticket : ledger.tickets()) {
        if (ticket.status != TicketStatus::Queued && ticket.status != TicketStatus::Deferred) {
            continue;
        }
        const VaultState& vault = ledger.vault(ticket.vaultId);
        appendToPlan(plan, viewTicket(ledger, vault, ticket));
    }
    return plan;
}

QueuePlan QueuePlanner::buildForVault(const Ledger& ledger, const std::string& vaultId) const {
    QueuePlan plan;
    const VaultState& vault = ledger.vault(vaultId);
    for (const RedemptionTicket& ticket : ledger.tickets()) {
        if (ticket.vaultId != vaultId) {
            continue;
        }
        if (ticket.status != TicketStatus::Queued && ticket.status != TicketStatus::Deferred) {
            continue;
        }
        appendToPlan(plan, viewTicket(ledger, vault, ticket));
    }
    return plan;
}

std::vector<QueueBucket> QueuePlanner::maturityBuckets(const Ledger& ledger, const std::string& vaultId) const {
    std::vector<QueueBucket> buckets;
    const QueuePlan plan = buildForVault(ledger, vaultId);
    for (const QueueTicketView& ticket : plan.tickets) {
        mergeBucket(buckets, bucketFor(ticket));
    }
    std::sort(buckets.begin(), buckets.end(), [](const QueueBucket& left, const QueueBucket& right) {
        if (left.unlockEpoch != right.unlockEpoch) {
            return left.unlockEpoch < right.unlockEpoch;
        }
        return left.vaultId < right.vaultId;
    });
    return buckets;
}

std::vector<QueueAccountSummary> QueuePlanner::accountSummaries(const QueuePlan& plan) const {
    std::vector<QueueAccountSummary> summaries;
    for (const QueueTicketView& ticket : plan.tickets) {
        mergeAccountSummary(summaries, ticket);
    }
    std::sort(summaries.begin(), summaries.end(), [](const QueueAccountSummary& left, const QueueAccountSummary& right) {
        if (left.credits != right.credits) {
            return left.credits > right.credits;
        }
        return left.accountId < right.accountId;
    });
    return summaries;
}

QueueTicketView QueuePlanner::viewTicket(const Ledger& ledger, const VaultState& vault, const RedemptionTicket& ticket) const {
    QueueTicketView view;
    const PriceSnapshot price = pricing_.snapshot(vault);
    const Amount gross = mulDivFloor(ticket.credits, price.price, kScale, "queue projected gross");
    const Amount fee = applyBpsFloor(gross, vault.policy.redemptionFeeBps, "queue projected fee");
    view.id = ticket.id;
    view.vaultId = ticket.vaultId;
    view.accountId = ticket.accountId;
    view.status = ticket.status;
    view.credits = ticket.credits;
    view.projectedGross = gross;
    view.projectedFee = fee;
    view.projectedNet = checkedSub(gross, fee, "queue projected net");
    view.requestedEpoch = ticket.requestedEpoch;
    view.unlockEpoch = ticket.unlockEpoch;
    view.ready = ticket.unlockEpoch <= ledger.epoch();
    if (ledger.epoch() >= ticket.requestedEpoch) {
        view.age = checkedSub(ledger.epoch(), ticket.requestedEpoch, "queue age");
    }
    return view;
}

QueueBucket QueuePlanner::bucketFor(const QueueTicketView& ticket) const {
    QueueBucket bucket;
    bucket.vaultId = ticket.vaultId;
    bucket.unlockEpoch = ticket.unlockEpoch;
    bucket.ticketCount = 1;
    bucket.credits = ticket.credits;
    bucket.projectedGross = ticket.projectedGross;
    bucket.projectedFees = ticket.projectedFee;
    bucket.projectedNet = ticket.projectedNet;
    return bucket;
}

void QueuePlanner::mergeBucket(std::vector<QueueBucket>& buckets, const QueueBucket& next) const {
    for (QueueBucket& bucket : buckets) {
        if (bucket.vaultId == next.vaultId && bucket.unlockEpoch == next.unlockEpoch) {
            bucket.ticketCount += next.ticketCount;
            bucket.credits = checkedAdd(bucket.credits, next.credits, "queue bucket credits");
            bucket.projectedGross = checkedAdd(bucket.projectedGross, next.projectedGross, "queue bucket gross");
            bucket.projectedFees = checkedAdd(bucket.projectedFees, next.projectedFees, "queue bucket fees");
            bucket.projectedNet = checkedAdd(bucket.projectedNet, next.projectedNet, "queue bucket net");
            return;
        }
    }
    buckets.push_back(next);
}

void QueuePlanner::mergeAccountSummary(
    std::vector<QueueAccountSummary>& summaries,
    const QueueTicketView& ticket) const {
    for (QueueAccountSummary& summary : summaries) {
        if (summary.accountId != ticket.accountId) {
            continue;
        }
        summary.ticketCount += 1;
        summary.credits = checkedAdd(summary.credits, ticket.credits, "queue account credits");
        summary.projectedNet = checkedAdd(summary.projectedNet, ticket.projectedNet, "queue account net");
        if (ticket.ready) {
            summary.readyTickets += 1;
            summary.readyCredits = checkedAdd(summary.readyCredits, ticket.credits, "queue account ready credits");
        } else {
            summary.lockedTickets += 1;
            summary.lockedCredits = checkedAdd(summary.lockedCredits, ticket.credits, "queue account locked credits");
        }
        return;
    }
    QueueAccountSummary summary;
    summary.accountId = ticket.accountId;
    summary.ticketCount = 1;
    summary.credits = ticket.credits;
    summary.projectedNet = ticket.projectedNet;
    if (ticket.ready) {
        summary.readyTickets = 1;
        summary.readyCredits = ticket.credits;
    } else {
        summary.lockedTickets = 1;
        summary.lockedCredits = ticket.credits;
    }
    summaries.push_back(std::move(summary));
}

void QueuePlanner::appendToPlan(QueuePlan& plan, QueueTicketView view) const {
    plan.totalTickets += 1;
    if (view.ready) {
        plan.readyTickets += 1;
        plan.readyCredits = checkedAdd(plan.readyCredits, view.credits, "queue ready credits");
    } else {
        plan.lockedTickets += 1;
        plan.lockedCredits = checkedAdd(plan.lockedCredits, view.credits, "queue locked credits");
    }
    plan.projectedGross = checkedAdd(plan.projectedGross, view.projectedGross, "queue projected gross total");
    plan.projectedFees = checkedAdd(plan.projectedFees, view.projectedFee, "queue projected fee total");
    plan.projectedNet = checkedAdd(plan.projectedNet, view.projectedNet, "queue projected net total");
    mergeBucket(plan.buckets, bucketFor(view));
    plan.tickets.push_back(std::move(view));
}

}  // namespace cobaltdtl
