#pragma once

#include "ledger.hpp"

namespace cobaltdtl {

struct QueueTicketView {
    std::string id;
    std::string vaultId;
    std::string accountId;
    TicketStatus status{TicketStatus::Queued};
    Amount credits{0};
    Amount projectedGross{0};
    Amount projectedFee{0};
    Amount projectedNet{0};
    Epoch requestedEpoch{0};
    Epoch unlockEpoch{0};
    Epoch age{0};
    bool ready{false};
};

struct QueueBucket {
    std::string vaultId;
    Epoch unlockEpoch{0};
    int ticketCount{0};
    Amount credits{0};
    Amount projectedGross{0};
    Amount projectedFees{0};
    Amount projectedNet{0};
};

struct QueueAccountSummary {
    std::string accountId;
    int ticketCount{0};
    int readyTickets{0};
    int lockedTickets{0};
    Amount credits{0};
    Amount readyCredits{0};
    Amount lockedCredits{0};
    Amount projectedNet{0};
};

struct QueuePlan {
    std::vector<QueueTicketView> tickets;
    std::vector<QueueBucket> buckets;
    int totalTickets{0};
    int readyTickets{0};
    int lockedTickets{0};
    Amount readyCredits{0};
    Amount lockedCredits{0};
    Amount projectedGross{0};
    Amount projectedFees{0};
    Amount projectedNet{0};
};

class QueuePlanner {
  public:
    explicit QueuePlanner(PricingService pricing = PricingService());

    QueuePlan build(const Ledger& ledger) const;
    QueuePlan buildForVault(const Ledger& ledger, const std::string& vaultId) const;
    std::vector<QueueBucket> maturityBuckets(const Ledger& ledger, const std::string& vaultId) const;
    std::vector<QueueAccountSummary> accountSummaries(const QueuePlan& plan) const;

  private:
    QueueTicketView viewTicket(const Ledger& ledger, const VaultState& vault, const RedemptionTicket& ticket) const;
    QueueBucket bucketFor(const QueueTicketView& ticket) const;
    void mergeBucket(std::vector<QueueBucket>& buckets, const QueueBucket& next) const;
    void mergeAccountSummary(std::vector<QueueAccountSummary>& summaries, const QueueTicketView& ticket) const;
    void appendToPlan(QueuePlan& plan, QueueTicketView view) const;

    PricingService pricing_;
};

}  // namespace cobaltdtl
