#pragma once

#include "ledger.hpp"

namespace cobaltdtl {

struct RunOptions {
    bool includeEvents{false};
    bool pretty{false};
};

class Engine {
  public:
    Scenario loadScenarioFile(const std::string& path) const;
    Ledger buildLedger(const Scenario& scenario) const;
    Ledger runScenario(const Scenario& scenario) const;
    Ledger runFile(const std::string& path) const;
    void validateFile(const std::string& path) const;
};

std::string readTextFile(const std::string& path);

}  // namespace cobaltdtl

