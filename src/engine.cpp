#include "engine.hpp"

#include <fstream>

namespace cobaltdtl {

std::string readTextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail(ErrorCode::Parse, "cannot open file: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

Scenario Engine::loadScenarioFile(const std::string& path) const {
    const std::string text = readTextFile(path);
    JsonValue root = parseJson(text, path);
    return parseScenario(root);
}

Ledger Engine::buildLedger(const Scenario& scenario) const {
    return Ledger(scenario);
}

Ledger Engine::runScenario(const Scenario& scenario) const {
    Ledger ledger(scenario);
    for (const Operation& op : scenario.operations) {
        ledger.apply(op);
    }
    return ledger;
}

Ledger Engine::runFile(const std::string& path) const {
    return runScenario(loadScenarioFile(path));
}

void Engine::validateFile(const std::string& path) const {
    const Scenario scenario = loadScenarioFile(path);
    (void)buildLedger(scenario);
}

}  // namespace cobaltdtl

