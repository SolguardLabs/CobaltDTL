#include "report.hpp"

#include <iostream>

namespace {

void printUsage(std::ostream& out) {
    out << "CobaltDTL synthetic vault simulator\n";
    out << "\n";
    out << "Usage:\n";
    out << "  cobaltdtl validate <scenario.json>\n";
    out << "  cobaltdtl run <scenario.json> [--json] [--events] [--pretty]\n";
}

bool hasFlag(const std::vector<std::string>& args, std::string_view flag) {
    return std::find(args.begin(), args.end(), flag) != args.end();
}

}  // namespace

int main(int argc, char** argv) {
    using namespace cobaltdtl;
    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        if (args.empty() || args[0] == "--help" || args[0] == "-h") {
            printUsage(std::cout);
            return args.empty() ? 1 : 0;
        }
        const std::string command = args[0];
        if (args.size() < 2) {
            printUsage(std::cerr);
            return 1;
        }
        const std::string path = args[1];
        Engine engine;
        if (command == "validate") {
            engine.validateFile(path);
            std::cout << "ok\n";
            return 0;
        }
        if (command == "run") {
            RunOptions options;
            options.includeEvents = hasFlag(args, "--events");
            options.pretty = hasFlag(args, "--pretty");
            const Ledger ledger = engine.runFile(path);
            std::cout << renderReport(ledger, options) << "\n";
            return 0;
        }
        std::cerr << "unknown command: " << command << "\n";
        printUsage(std::cerr);
        return 1;
    } catch (const DtlError& error) {
        std::cerr << errorCodeName(error.code()) << ": " << error.what() << "\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return 2;
    }
}

