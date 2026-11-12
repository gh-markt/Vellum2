#include "vellum/script_process.hh"
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        const auto result = vellum::run_script_process(argv[1], "6 * 7");
        if (!result.success || result.text != "42") {
            return 1;
        }
        if (vellum::run_script_process(argv[1], "throw new Error('negative')").success) {
            return 1;
        }
        if (vellum::run_script_process(argv[1], "function {").success) {
            return 1;
        }
        try {
            vellum::run_script_process(argv[1], "while (true) {}", std::chrono::milliseconds(500));
            return 1;
        } catch (const std::exception &) {
        }
        const auto isolated =
            vellum::run_script_process(argv[1], "typeof process + ':' + typeof require");
        if (!isolated.success || isolated.text != "undefined:undefined") {
            return 1;
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
