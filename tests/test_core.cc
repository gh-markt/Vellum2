#include "vellum/autosave.hh"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
namespace {
int checks = 0;
void require(bool value, const char *message) {
    ++checks;
    if (!value) {
        throw std::runtime_error(message);
    }
}
template <class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "operation should reject input/failure");
}
void validate(const std::string &value) {
    if (!value.starts_with("%PDF-test:") || !value.ends_with("%%EOF")) {
        throw std::runtime_error("invalid test snapshot");
    }
}
std::string payload(const char *content) {
    return std::string("%PDF-test:") + content + "%%EOF";
}
void state_tests() {
    vellum::document_state state;
    require(state.mode() == vellum::document_mode::reader, "initial reader mode");
    require(!state.needs_autosave(), "clean reader");
    state.changed();
    require(!state.needs_autosave(), "dirty reader must never autosave");
    require(state.enable_editor(), "first editor transition");
    require(!state.enable_editor(), "one-way transition is idempotent");
    require(state.needs_autosave(), "reader edits eligible after transition");
    rejects([&state] { state.autosaved(2); });
    state.autosaved(1);
    require(!state.needs_autosave(), "autosave records revision");
    state.changed();
    state.autosaved(2);
    rejects([&state] { state.autosaved(1); });
}
void autosave_tests(const std::filesystem::path &directory) {
    using namespace std::chrono_literals;
    vellum::document_state state;
    int serialized = 0;
    bool fail = false;
    const auto serialize = [&] {
        ++serialized;
        if (fail) {
            throw std::runtime_error("injected serialization failure");
        }
        return payload("filled form in reader");
    };
    const auto now = vellum::autosave_controller::clock::now();
    const auto target = directory / "snapshot.pdf";
    vellum::autosave_controller controller(state, vellum::snapshot_store(target), serialize,
                                           validate, now);
    state.changed();
    require(!controller.trigger_now(now + 120s), "forced trigger must obey Reader mode");
    require(!controller.tick(now + 120s), "timer must obey Reader mode");
    require(serialized == 0 && !std::filesystem::exists(target),
            "reader must not serialize or write");
    state.enable_editor();
    require(!controller.tick(now + 59s), "timer interval");
    require(controller.tick(now + 60s), "timed autosave");
    require(vellum::read_file(target) == payload("filled form in reader"), "snapshot content");
    require(!controller.trigger_now(now + 61s), "unchanged forced trigger skipped");
    require(!controller.tick(now + 120s), "unchanged timer skipped");
    state.changed();
    fail = true;
    rejects([&] { controller.trigger_now(now + 121s); });
    require(state.needs_autosave(), "failure must retain dirty revision");
    fail = false;
    require(controller.trigger_now(now + 122s), "retry succeeds");
    const auto invalid_interval = [&] {
        vellum::autosave_controller bad(state, vellum::snapshot_store(target), serialize, validate,
                                        now, 0s);
    };
    rejects(invalid_interval);
    const auto invalid_serializer = [&] {
        vellum::autosave_controller bad(state, vellum::snapshot_store(target), {}, validate, now);
    };
    rejects(invalid_serializer);
    vellum::document_state write_failure;
    write_failure.enable_editor();
    write_failure.changed();
    vellum::autosave_controller broken(
        write_failure, vellum::snapshot_store(directory / "missing" / "snapshot.pdf"), serialize,
        validate, now);
    rejects([&] { broken.trigger_now(now); });
    require(write_failure.needs_autosave(), "write failure must retain dirty revision");
    vellum::document_state reentrant;
    reentrant.enable_editor();
    reentrant.changed();
    const auto edit_during_snapshot = [&] {
        reentrant.changed();
        return payload("earlier revision");
    };
    vellum::autosave_controller changing(reentrant, vellum::snapshot_store(target),
                                         edit_during_snapshot, validate, now);
    require(changing.trigger_now(now), "reentrant snapshot committed");
    require(reentrant.needs_autosave(), "later edits cannot be cleared by older snapshot");
}
void snapshot_tests(const std::filesystem::path &directory) {
    const auto original = directory / "original.pdf";
    const auto target = directory / "recover.pdf";
    vellum::atomic_write(original, payload("original"));
    const vellum::snapshot_store store(target);
    require(store.path() == target, "snapshot identity is the committed target");
    rejects([&] { store.recover(validate); });
    store.commit(payload("first"), validate);
    store.commit(payload("latest"), validate);
    vellum::atomic_write(directory / "recover.pdf.tmp.orphan", payload("partial"));
    require(store.recover(validate) == payload("latest"), "only committed snapshot recovered");
    require(vellum::read_file(original) == payload("original"), "original never overwritten");
    rejects([&] { store.commit("broken", validate); });
    require(store.recover(validate) == payload("latest"),
            "invalid candidate preserves prior snapshot");
    rejects([&] { store.commit(payload("x"), {}); });
    rejects([&] { store.recover({}); });
    rejects([&] { vellum::snapshot_store invalid(directory / "recover.pdf.tmp.orphan"); });
    vellum::atomic_write(target, "broken");
    rejects([&] { store.recover(validate); });
    require(std::filesystem::exists(directory / "recover.pdf.tmp.orphan"),
            "temporary ignored, not promoted");
    rejects([&] { vellum::read_file(original, 3); });
    vellum::atomic_write(directory / "empty", "");
    require(vellum::read_file(directory / "empty").empty(), "empty file round trip");
    std::string large(20000, 'x');
    vellum::atomic_write(directory / "large", large);
    require(vellum::read_file(directory / "large") == large, "multi-block read/write");
    rejects([&] { vellum::atomic_write(directory, "cannot rename over directory"); });
}
void observer_failure_tests(const std::filesystem::path &directory) {
    const auto target = directory / "résumé-文.pdf";
    vellum::atomic_write(target, payload("old"));
    const auto fail_before = [](vellum::commit_point point) {
        if (point == vellum::commit_point::before_rename) {
            throw std::runtime_error("injected before");
        }
    };
    const auto before = [&] { vellum::atomic_write(target, payload("new"), fail_before); };
    rejects(before);
    require(vellum::read_file(target) == payload("old"),
            "pre-commit exception preserves previous file");
    const auto fail_after = [](vellum::commit_point point) {
        if (point == vellum::commit_point::after_rename) {
            throw std::runtime_error("injected after");
        }
    };
    const auto after = [&] { vellum::atomic_write(target, payload("new"), fail_after); };
    rejects(after);
    require(vellum::snapshot_store(target).recover(validate) == payload("new"),
            "post-commit file remains authoritative");
}
void crash_test(const std::filesystem::path &directory, vellum::commit_point point) {
    const auto target = directory / "crash.pdf";
    vellum::atomic_write(target, payload("previous"));
    const auto child = ::fork();
    if (child < 0) {
        throw std::runtime_error("fork failed");
    }
    if (child == 0) {
        const auto crash = [point](vellum::commit_point reached) {
            if (point == reached) {
                std::_Exit(71);
            }
        };
        vellum::snapshot_store(target).commit(payload("replacement"), validate, crash);
        std::_Exit(72);
    }
    int status = 0;
    require(::waitpid(child, &status, 0) == child, "wait for crash child");
    require(WIFEXITED(status) && WEXITSTATUS(status) == 71,
            "crash injection reached production path");
    const auto expected =
        point == vellum::commit_point::before_rename ? payload("previous") : payload("replacement");
    require(vellum::snapshot_store(target).recover(validate) == expected,
            "atomic old-or-new snapshot");
}
} // namespace
int main() {
    std::string pattern = (std::filesystem::temp_directory_path() / "vellum-tests-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr) {
        return 2;
    }
    const std::filesystem::path directory(pattern);
    try {
        state_tests();
        autosave_tests(directory);
        snapshot_tests(directory);
        observer_failure_tests(directory);
        crash_test(directory, vellum::commit_point::before_rename);
        crash_test(directory, vellum::commit_point::after_rename);
        std::filesystem::remove_all(directory);
        std::cout << checks << " core checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(directory);
        return 1;
    }
}
