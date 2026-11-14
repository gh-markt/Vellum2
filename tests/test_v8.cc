#include "vellum/script_process.hh"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <unicode/uversion.h>
int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
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
        // Conversion is a separate JavaScript operation and may itself throw.
        const auto conversion = vellum::run_script_process(
            argv[1], "({toString() { throw new Error('conversion'); }})");
        if (conversion.success || conversion.text != "JavaScript result conversion failed") {
            return 1;
        }
        const auto embedded_zero = vellum::run_script_process(argv[1], "'a\\u0000\\u03bb'");
        if (!embedded_zero.success || embedded_zero.text != std::string("a\0\xce\xbb", 4)) {
            return 1;
        }
        const auto set_global = vellum::run_script_process(argv[1], "globalThis.leakedGlobal = 7");
        if (!set_global.success || set_global.text != "7") {
            return 1;
        }
        const auto fresh_context = vellum::run_script_process(argv[1], "typeof leakedGlobal");
        if (!fresh_context.success || fresh_context.text != "undefined") {
            return 1;
        }
        UVersionInfo icu_version{};
        u_getVersion(icu_version);
        const auto unicode_version_probe =
            vellum::run_script_process(argv[1], "'\\u{1DF95}'.toUpperCase()");
        if (!unicode_version_probe.success ||
            (icu_version[0] >= 79 && unicode_version_probe.text != "SS")) {
            std::cerr << "V8 does not expose the installed ICU Unicode mapping\n";
            return 1;
        }
        // Unicode 18 introduces a case-folding equivalence across UTF-16 widths.
        // Earlier system ICU versions do not yet assign this code point.
        const auto unicode_regex = vellum::run_script_process(argv[1], R"JS(
            const supplementary = '\u{1DF95}';
            if (supplementary.toUpperCase() === 'SS') {
                const pairs = [
                    ['^\u00df$', supplementary, true],
                    ['^\u1e9e$', supplementary, true],
                    ['^' + supplementary + '$', '\u00df', true],
                    ['^[\u00df]$', supplementary, true],
                    ['^[^\u00df]$', supplementary, false],
                    ['^a\u00dfb$', 'A' + supplementary + 'B', true],
                    ['^\\u{df}$', supplementary, true],
                    ['^(?:\u00df)+$', supplementary + '\u00df\u1e9e' + supplementary, true],
                    ['^\u00df{2}$', supplementary, false],
                    ['^(?:x|\u00df)$', supplementary, true],
                    ['(?<=^\u00df)x$', supplementary + 'x', true],
                ];
                for (const [pattern, subject, expected] of pairs) {
                    for (const flags of ['iu', 'iv']) {
                        if (new RegExp(pattern, flags).test(subject) !== expected)
                            throw new Error('Unicode case-folding mismatch');
                    }
                }
                if (/^\u00df$/i.test(supplementary) ||
                    new RegExp('^' + supplementary + '$', 'i').test('\u00df'))
                    throw new Error('legacy mode matched a supplementary code point');
            }
            'ok';
        )JS");
        if (!unicode_regex.success || unicode_regex.text != "ok") {
            std::cerr << "Unicode regular expression verification failed: " << unicode_regex.text
                      << '\n';
            return 1;
        }
        bool oversized_rejected = false;
        try {
            vellum::run_script_process(argv[1], "'x'.repeat(1048577)");
        } catch (const std::exception &) {
            oversized_rejected = true;
        }
        if (!oversized_rejected) {
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
        if (argc == 3) {
            struct worker_failure {
                const char *mode;
                const char *script;
            };
            for (const auto &failure :
                 {worker_failure{"dispose-isolate", "6 * 7"}, worker_failure{"dispose-v8", "6 * 7"},
                  worker_failure{"run-fatal-allocation",
                                 "JSON.stringify(Array.from({length:200},(_,i)=>({x:i})))"}}) {
                if (::setenv("VELLUM_TEST_V8_FAILURE", failure.mode, 1) != 0) {
                    throw std::runtime_error("cannot configure worker failure test");
                }
                bool reported = false;
                try {
                    (void)vellum::run_script_process(argv[2], failure.script);
                } catch (const std::exception &error) {
                    const std::string_view message(error.what());
                    reported = !message.empty() && message.find("timed out") == message.npos;
                }
                if (::unsetenv("VELLUM_TEST_V8_FAILURE") != 0 || !reported) {
                    throw std::runtime_error("worker failure was not reported to the caller");
                }
                const auto retry = vellum::run_script_process(argv[2], "6 * 7");
                if (!retry.success || retry.text != "42") {
                    throw std::runtime_error("new worker did not recover after failure");
                }
            }
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
