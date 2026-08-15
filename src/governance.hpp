#pragma once

#include "common.hpp"

#include <set>

namespace cobaltdtl {

enum class GovernanceState { Queued, Executed, Cancelled };

struct GovernanceOperation {
    std::string protocol;
    std::string network;
    std::string target;
    std::string method;
    std::string payloadHash;
    std::string predecessor;
    std::string salt;
    Epoch executeAfter{0};
};

struct GovernanceRecord {
    std::string id;
    GovernanceOperation operation;
    GovernanceState state{GovernanceState::Queued};
    std::string proposer;
    std::set<std::string> approvals;
    Epoch expiresAt{0};
    Epoch executedAt{0};
    std::string cancelledBy;
};

std::string governanceStateName(GovernanceState state);
std::string operationId(const GovernanceOperation& operation);

class GovernanceExecutor {
  public:
    GovernanceExecutor(std::set<std::string> governors, std::size_t quorum, Epoch delay, Epoch grace, std::string guardian);

    GovernanceRecord queue(GovernanceOperation operation, const std::string& proposer, Epoch now);
    GovernanceRecord approve(const std::string& id, const std::string& governor, Epoch now);
    GovernanceRecord execute(const std::string& id, const std::string& governor, Epoch now);
    GovernanceRecord cancel(const std::string& id, const std::string& guardian, Epoch now);
    const GovernanceRecord& record(const std::string& id) const;
    std::vector<GovernanceRecord> records() const;

  private:
    void validateGovernor(const std::string& governor, std::string_view action) const;
    GovernanceRecord& mutableQueued(const std::string& id, Epoch now);

    std::set<std::string> governors_;
    std::size_t quorum_{0};
    Epoch delay_{0};
    Epoch grace_{0};
    std::string guardian_;
    std::map<std::string, GovernanceRecord> records_;
};

}  // namespace cobaltdtl
