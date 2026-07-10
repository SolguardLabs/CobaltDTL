#pragma once

#include "ledger.hpp"

namespace cobaltdtl {

enum class CheckSeverity {
    Info,
    Warning,
    Error
};

std::string checkSeverityName(CheckSeverity severity);

struct CheckIssue {
    CheckSeverity severity{CheckSeverity::Info};
    std::string code;
    std::string scope;
    std::string id;
    std::string message;
    Amount observed{0};
    Amount expected{0};
};

struct CheckReport {
    std::vector<CheckIssue> issues;
    int infoCount{0};
    int warningCount{0};
    int errorCount{0};

    bool ok() const;
    void add(CheckIssue issue);
};

class SanityChecker {
  public:
    CheckReport run(const Ledger& ledger) const;

  private:
    void checkVaultReferences(const Ledger& ledger, CheckReport& report) const;
    void checkAccountReferences(const Ledger& ledger, CheckReport& report) const;
    void checkCreditConservation(const Ledger& ledger, CheckReport& report) const;
    void checkTicketAccounting(const Ledger& ledger, CheckReport& report) const;
    void checkLiquidationRecords(const Ledger& ledger, CheckReport& report) const;
    void checkReserveSurfaces(const Ledger& ledger, CheckReport& report) const;
    void checkPolicyRanges(const Ledger& ledger, CheckReport& report) const;
    void checkEventOrdering(const Ledger& ledger, CheckReport& report) const;

    void addIssue(
        CheckReport& report,
        CheckSeverity severity,
        std::string code,
        std::string scope,
        std::string id,
        std::string message,
        Amount observed = 0,
        Amount expected = 0) const;
};

}  // namespace cobaltdtl

