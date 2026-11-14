#include "vellum/autosave.hh"
#include <filesystem>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// Explicit-instantiation access permits boundary-state setup without changing the
// production interface or relying on the object's representation.
template <class Tag, typename Tag::type member> struct private_member_access {
    friend typename Tag::type access_member(Tag) { return member; }
};
struct revision_member {
    using type = std::uint64_t vellum::document_state::*;
    friend type access_member(revision_member);
};
template struct private_member_access<revision_member, &vellum::document_state::current_revision>;

#ifdef VELLUM_STATE_EXCEPTION_FAULTS
bool fail_state_exception_construction = false;
extern "C" void __real__ZNSt14overflow_errorC1EPKc(std::overflow_error *, const char *);
extern "C" void __real__ZNSt11logic_errorC1EPKc(std::logic_error *, const char *);
extern "C" void __wrap__ZNSt14overflow_errorC1EPKc(std::overflow_error *error, const char *what) {
    if (fail_state_exception_construction) {
        fail_state_exception_construction = false;
        throw std::bad_alloc();
    }
    __real__ZNSt14overflow_errorC1EPKc(error, what);
}
extern "C" void __wrap__ZNSt11logic_errorC1EPKc(std::logic_error *error, const char *what) {
    if (fail_state_exception_construction) {
        fail_state_exception_construction = false;
        throw std::bad_alloc();
    }
    __real__ZNSt11logic_errorC1EPKc(error, what);
}
#endif
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
void state_revision_boundary_tests() {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    vellum::document_state state;
    state.*access_member(revision_member{}) = maximum - 1;
    require(state.enable_editor(), "boundary state enters editor");
    state.autosaved(maximum - 1);
    require(!state.needs_autosave(), "boundary state starts clean");
    state.changed();
    require(state.revision() == maximum && state.needs_autosave(),
            "last revision is reachable and dirty");
    bool overflow = false;
    try {
        state.changed();
    } catch (const std::overflow_error &) {
        overflow = true;
    }
    require(overflow && state.revision() == maximum && state.needs_autosave(),
            "revision overflow rejects without wrapping or clearing dirty state");
    state.autosaved(maximum);
    require(!state.needs_autosave(), "maximum revision can be committed");
    rejects([&state, maximum] { state.autosaved(maximum - 1); });
    require(state.revision() == maximum && !state.needs_autosave(),
            "stale maximum-boundary commit leaves state unchanged");
#ifdef VELLUM_STATE_EXCEPTION_FAULTS
    for (const bool overflow_error : {false, true}) {
        fail_state_exception_construction = true;
        bool allocation_failed = false;
        try {
            if (overflow_error) {
                state.changed();
            } else {
                state.autosaved(maximum - 1);
            }
        } catch (const std::bad_alloc &) {
            allocation_failed = true;
        }
        fail_state_exception_construction = false;
        require(allocation_failed && state.revision() == maximum && !state.needs_autosave(),
                "exception-construction failure leaves revision and committed state unchanged");
    }
#endif
}
void state_model_tests() {
    std::size_t states_checked = 0;
    struct model {
        bool editor = false;
        std::uint64_t current = 0;
        std::uint64_t committed = 0;
    };
    const auto explore = [&](const auto &self, vellum::document_state state, model expected,
                             int remaining) -> void {
        ++states_checked;
        require(state.revision() == expected.current, "model current revision mismatch");
        require((state.mode() == vellum::document_mode::editor) == expected.editor,
                "model mode mismatch");
        require(state.needs_autosave() ==
                    (expected.editor && expected.current != expected.committed),
                "model dirty state mismatch");
        if (remaining == 0) {
            return;
        }
        {
            auto next = state;
            auto next_model = expected;
            next.changed();
            ++next_model.current;
            self(self, next, next_model, remaining - 1);
        }
        {
            auto next = state;
            auto next_model = expected;
            require(next.enable_editor() == !next_model.editor, "model editor transition mismatch");
            next_model.editor = true;
            self(self, next, next_model, remaining - 1);
        }
        for (const auto revision : {std::uint64_t{0}, expected.current, expected.current + 1}) {
            auto next = state;
            auto next_model = expected;
            const bool valid = revision >= expected.committed && revision <= expected.current;
            bool accepted = false;
            try {
                next.autosaved(revision);
                accepted = true;
            } catch (const std::logic_error &) {
            }
            require(accepted == valid,
                    "model accepted an invalid revision or rejected a valid one");
            if (valid) {
                next_model.committed = revision;
            }
            self(self, next, next_model, remaining - 1);
        }
    };
    explore(explore, vellum::document_state{}, model{}, 6);
    std::cout << "Revision model: " << states_checked << " states checked\n";
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
    const auto invalid_validator = [&] {
        vellum::autosave_controller bad(state, vellum::snapshot_store(target), serialize, {}, now);
    };
    rejects(invalid_validator);
    const auto negative_interval = [&] {
        vellum::autosave_controller bad(state, vellum::snapshot_store(target), serialize, validate,
                                        now, -1s);
    };
    rejects(negative_interval);

    vellum::document_state validation_state;
    validation_state.enable_editor();
    validation_state.changed();
    bool invalid_snapshot = false;
    int validation_serializations = 0;
    const auto serialize_candidate = [&] {
        ++validation_serializations;
        return invalid_snapshot ? std::string("invalid") : payload("validated");
    };
    const auto validation_target = directory / "validation-retry.pdf";
    vellum::autosave_controller validation_controller(validation_state,
                                                      vellum::snapshot_store(validation_target),
                                                      serialize_candidate, validate, now, 10s);
    require(validation_controller.tick(now + 10s), "initial validation snapshot committed");
    validation_state.changed();
    invalid_snapshot = true;
    rejects([&] { validation_controller.tick(now + 20s); });
    require(validation_state.needs_autosave(), "validation failure retains dirty revision");
    require(vellum::read_file(validation_target) == payload("validated"),
            "validation failure preserves the committed snapshot");
    invalid_snapshot = false;
    require(validation_controller.tick(now + 20s), "validation failure does not delay retry");
    require(!validation_state.needs_autosave() && validation_serializations == 3,
            "successful retry commits the pending revision once");
    validation_state.changed();
    require(!validation_controller.tick(now + 29s), "retry resets timer after success");
    require(validation_controller.tick(now + 30s), "timer fires at the new boundary");
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
void storage_boundary_tests(const std::filesystem::path &directory) {
    const auto exact = directory / "boundary.bin";
    const std::string data(8192, 'x');
    vellum::atomic_write(exact, data);
    require(vellum::read_file(exact, data.size()) == data, "exact read limit is accepted");
    rejects([&] { vellum::read_file(exact, data.size() - 1); });
    rejects([&] { vellum::read_file(exact, 0); });
    const auto empty = directory / "zero-limit.bin";
    vellum::atomic_write(empty, "");
    require(vellum::read_file(empty, 0).empty(), "empty file fits zero-byte limit");
    rejects([&] { vellum::read_file(directory / "missing.bin"); });
    rejects([&] { vellum::read_file(directory); });
    rejects([&] { vellum::atomic_write(directory / "absent" / "file", "new"); });
    require(vellum::read_file(exact) == data, "failed writes preserve unrelated files");

    const auto private_base = directory / "private";
    const auto first = vellum::create_private_directory(private_base);
    const auto second = vellum::create_private_directory(private_base);
    require(first != second && first.parent_path() == private_base &&
                second.parent_path() == private_base,
            "recovery sessions receive distinct directories under the requested base");
    struct stat attributes{};
    require(::stat(first.c_str(), &attributes) == 0 && S_ISDIR(attributes.st_mode) &&
                (attributes.st_mode & 0077) == 0,
            "recovery directory is private");
    rejects([&] { vellum::create_private_directory(exact); });
    const auto previous_directory = std::filesystem::current_path();
    std::filesystem::current_path(directory);
    try {
        vellum::atomic_write("relative-snapshot.pdf", "relative data");
        require(vellum::read_file("relative-snapshot.pdf") == "relative data",
                "relative target is committed in the current directory");
    } catch (...) {
        std::filesystem::current_path(previous_directory);
        throw;
    }
    std::filesystem::current_path(previous_directory);
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
        state_model_tests();
        state_revision_boundary_tests();
        autosave_tests(directory);
        snapshot_tests(directory);
        storage_boundary_tests(directory);
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
