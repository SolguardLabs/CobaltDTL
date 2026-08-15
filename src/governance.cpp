#include "governance.hpp"

#include "sha256.hpp"

#include <iomanip>

namespace cobaltdtl {
namespace {

bool isHexDigest(std::string_view value) {
    if (value.size() != 64U) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char character) {
        return std::isxdigit(static_cast<unsigned char>(character)) != 0;
    });
}

void appendCanonical(std::ostringstream& output, std::string_view value) {
    output << value.size() << ':' << value;
}

void validateOperation(const GovernanceOperation& operation) {
    const std::vector<std::pair<std::string_view, std::string>> fields{
        {"protocol", operation.protocol},
        {"network", operation.network},
        {"target", operation.target},
        {"method", operation.method},
        {"salt", operation.salt},
    };
    for (const auto& field : fields) {
        if (field.second.empty() || trim(field.second) != field.second) {
            fail(ErrorCode::Validation, "governance " + std::string(field.first) + " must be non-empty and normalized");
        }
    }
    if (!isHexDigest(operation.payloadHash)) {
        fail(ErrorCode::Validation, "governance payload hash must be a 32-byte hexadecimal digest");
    }
    if (!operation.predecessor.empty() && !isHexDigest(operation.predecessor)) {
        fail(ErrorCode::Validation, "governance predecessor must be a 32-byte hexadecimal operation id");
    }
    if (operation.executeAfter < 0) {
        fail(ErrorCode::Validation, "governance execution epoch cannot be negative");
    }
}

}  // namespace

std::string governanceStateName(GovernanceState state) {
    switch (state) {
        case GovernanceState::Queued:
            return "queued";
        case GovernanceState::Executed:
            return "executed";
        case GovernanceState::Cancelled:
            return "cancelled";
    }
    return "unknown";
}

std::string operationId(const GovernanceOperation& operation) {
    validateOperation(operation);
    std::ostringstream canonical;
    appendCanonical(canonical, operation.protocol);
    appendCanonical(canonical, operation.network);
    appendCanonical(canonical, operation.target);
    appendCanonical(canonical, operation.method);
    appendCanonical(canonical, toLower(operation.payloadHash));
    appendCanonical(canonical, toLower(operation.predecessor));
    appendCanonical(canonical, operation.salt);
    canonical << std::setw(20) << std::setfill('0') << operation.executeAfter;
    return sha256(canonical.str());
}

GovernanceExecutor::GovernanceExecutor(
    std::set<std::string> governors,
    std::size_t quorum,
    Epoch delay,
    Epoch grace,
    std::string guardian)
    : governors_(std::move(governors)), quorum_(quorum), delay_(delay), grace_(grace), guardian_(trim(guardian)) {
    if (governors_.empty() || quorum_ == 0U || quorum_ > governors_.size()) {
        fail(ErrorCode::Validation, "governance quorum is outside the governor set");
    }
    if (delay_ <= 0 || grace_ <= 0 || guardian_.empty()) {
        fail(ErrorCode::Validation, "governance delay, grace, and guardian must be configured");
    }
    for (const std::string& governor : governors_) {
        if (governor.empty() || trim(governor) != governor) {
            fail(ErrorCode::Validation, "governance governor must be non-empty and normalized");
        }
    }
}

GovernanceRecord GovernanceExecutor::queue(GovernanceOperation operation, const std::string& proposer, Epoch now) {
    validateGovernor(proposer, "propose");
    if (now < 0) {
        fail(ErrorCode::Validation, "governance current epoch cannot be negative");
    }
    const Epoch minimum = checkedAdd(now, delay_, "governance minimum execution epoch");
    if (operation.executeAfter < minimum) {
        fail(ErrorCode::Policy, "governance operation does not satisfy the minimum delay");
    }
    GovernanceRecord record;
    record.id = operationId(operation);
    record.operation = std::move(operation);
    record.proposer = proposer;
    record.approvals.insert(proposer);
    record.expiresAt = checkedAdd(record.operation.executeAfter, grace_, "governance expiration");
    if (records_.find(record.id) != records_.end()) {
        fail(ErrorCode::Validation, "governance operation already exists: " + record.id);
    }
    records_.emplace(record.id, record);
    return record;
}

GovernanceRecord GovernanceExecutor::approve(const std::string& id, const std::string& governor, Epoch now) {
    validateGovernor(governor, "approve");
    GovernanceRecord& item = mutableQueued(id, now);
    if (!item.approvals.insert(governor).second) {
        fail(ErrorCode::Policy, "governance approval already recorded for " + governor);
    }
    return item;
}

GovernanceRecord GovernanceExecutor::execute(const std::string& id, const std::string& governor, Epoch now) {
    validateGovernor(governor, "execute");
    GovernanceRecord& item = mutableQueued(id, now);
    if (now < item.operation.executeAfter) {
        fail(ErrorCode::Policy, "governance operation remains timelocked");
    }
    if (item.approvals.size() < quorum_) {
        fail(ErrorCode::Policy, "governance operation has insufficient approvals");
    }
    if (!item.operation.predecessor.empty()) {
        const auto predecessor = records_.find(toLower(item.operation.predecessor));
        if (predecessor == records_.end() || predecessor->second.state != GovernanceState::Executed) {
            fail(ErrorCode::Policy, "governance predecessor is not executed");
        }
    }
    item.state = GovernanceState::Executed;
    item.executedAt = now;
    return item;
}

GovernanceRecord GovernanceExecutor::cancel(const std::string& id, const std::string& guardian, Epoch now) {
    if (trim(guardian) != guardian_) {
        fail(ErrorCode::Policy, "governance cancellation requires the configured guardian");
    }
    GovernanceRecord& item = mutableQueued(id, now);
    item.state = GovernanceState::Cancelled;
    item.cancelledBy = guardian_;
    return item;
}

const GovernanceRecord& GovernanceExecutor::record(const std::string& id) const {
    const auto found = records_.find(toLower(trim(id)));
    if (found == records_.end()) {
        fail(ErrorCode::UnknownReference, "unknown governance operation: " + id);
    }
    return found->second;
}

std::vector<GovernanceRecord> GovernanceExecutor::records() const {
    std::vector<GovernanceRecord> output;
    output.reserve(records_.size());
    for (const auto& item : records_) {
        output.push_back(item.second);
    }
    return output;
}

void GovernanceExecutor::validateGovernor(const std::string& governor, std::string_view action) const {
    if (governors_.find(governor) == governors_.end()) {
        fail(ErrorCode::Policy, "governance " + std::string(action) + " requires an active governor");
    }
}

GovernanceRecord& GovernanceExecutor::mutableQueued(const std::string& id, Epoch now) {
    auto found = records_.find(toLower(trim(id)));
    if (found == records_.end()) {
        fail(ErrorCode::UnknownReference, "unknown governance operation: " + id);
    }
    if (found->second.state != GovernanceState::Queued) {
        fail(ErrorCode::Policy, "governance operation is " + governanceStateName(found->second.state));
    }
    if (now > found->second.expiresAt) {
        fail(ErrorCode::Policy, "governance operation has expired");
    }
    return found->second;
}

}  // namespace cobaltdtl
