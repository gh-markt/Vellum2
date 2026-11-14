// Linker interposition exercises the worker's documented V8 failure returns.
#include "vellum/script_runtime.hh"
#include <cstdlib>
#include <cstring>
#include <libplatform/libplatform.h>
#include <new>
#include <unistd.h>
#include <v8.h>

namespace {
thread_local bool fail_next_allocation = false;
bool failure(const char *name) {
    const char *mode = std::getenv("VELLUM_TEST_V8_FAILURE");
    return mode != nullptr && std::strcmp(mode, name) == 0;
}
void record(char value) {
    const char *fd = std::getenv("VELLUM_TEST_TRACE_FD");
    if (fd != nullptr) {
        (void)::write(std::atoi(fd), &value, 1);
    }
}
} // namespace

void *operator new(std::size_t size) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        record('A');
        throw std::bad_alloc();
    }
    if (void *result = std::malloc(size ? size : 1)) {
        return result;
    }
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) {
    return ::operator new(size);
}
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}
void operator delete(void *value) noexcept {
    std::free(value);
}
void operator delete[](void *value) noexcept {
    std::free(value);
}
void operator delete(void *value, std::size_t) noexcept {
    std::free(value);
}
void operator delete[](void *value, std::size_t) noexcept {
    std::free(value);
}
void operator delete(void *value, const std::nothrow_t &) noexcept {
    std::free(value);
}
void operator delete[](void *value, const std::nothrow_t &) noexcept {
    std::free(value);
}

extern "C" void __real__ZN2v87Isolate5EnterEv(v8::Isolate *);
extern "C" void __wrap__ZN2v87Isolate5EnterEv(v8::Isolate *isolate) {
    if (failure("enter-allocation")) {
        record('I');
        fail_next_allocation = true;
    }
    try {
        __real__ZN2v87Isolate5EnterEv(isolate);
        fail_next_allocation = false;
    } catch (...) {
        fail_next_allocation = false;
        throw;
    }
}

extern "C" void __real__ZN2v87Isolate7DisposeEv(v8::Isolate *);
extern "C" void __wrap__ZN2v87Isolate7DisposeEv(v8::Isolate *isolate) {
    if (failure("dispose-isolate") || failure("dispose-isolate-invalid")) {
        record('D');
        fail_next_allocation = true;
    }
    try {
        __real__ZN2v87Isolate7DisposeEv(isolate);
        fail_next_allocation = false;
    } catch (...) {
        fail_next_allocation = false;
        throw;
    }
}

extern "C" bool __real__ZN2v82V87DisposeEv();
extern "C" bool __wrap__ZN2v82V87DisposeEv() {
    if (failure("dispose-v8") || failure("dispose-v8-invalid")) {
        record('D');
        fail_next_allocation = true;
    }
    try {
        const bool result = __real__ZN2v82V87DisposeEv();
        fail_next_allocation = false;
        return result;
    } catch (...) {
        fail_next_allocation = false;
        throw;
    }
}

extern "C" v8::ArrayBuffer::Allocator *
__real__ZN2v811ArrayBuffer9Allocator19NewDefaultAllocatorEv();
extern "C" v8::ArrayBuffer::Allocator *
__wrap__ZN2v811ArrayBuffer9Allocator19NewDefaultAllocatorEv() {
    if (failure("allocator-allocation")) {
        record('B');
        fail_next_allocation = true;
    }
    try {
        auto *result = __real__ZN2v811ArrayBuffer9Allocator19NewDefaultAllocatorEv();
        fail_next_allocation = false;
        return result;
    } catch (...) {
        fail_next_allocation = false;
        throw;
    }
}

extern "C" v8::Local<v8::Context>
__real__ZN2v87Context3NewEPNS_7IsolateEPNS_22ExtensionConfigurationENS_10MaybeLocalINS_14ObjectTemplateEEENS5_INS_5ValueEEENS_33DeserializeInternalFieldsCallbackEPNS_14MicrotaskQueueENS_30DeserializeContextDataCallbackENS_29DeserializeAPIWrapperCallbackE(
    v8::Isolate *, v8::ExtensionConfiguration *, v8::MaybeLocal<v8::ObjectTemplate>,
    v8::MaybeLocal<v8::Value>, v8::DeserializeInternalFieldsCallback, v8::MicrotaskQueue *,
    v8::DeserializeContextDataCallback, v8::DeserializeAPIWrapperCallback);
extern "C" v8::Local<v8::Context>
__wrap__ZN2v87Context3NewEPNS_7IsolateEPNS_22ExtensionConfigurationENS_10MaybeLocalINS_14ObjectTemplateEEENS5_INS_5ValueEEENS_33DeserializeInternalFieldsCallbackEPNS_14MicrotaskQueueENS_30DeserializeContextDataCallbackENS_29DeserializeAPIWrapperCallbackE(
    v8::Isolate *isolate, v8::ExtensionConfiguration *extensions,
    v8::MaybeLocal<v8::ObjectTemplate> object_template, v8::MaybeLocal<v8::Value> object,
    v8::DeserializeInternalFieldsCallback fields, v8::MicrotaskQueue *queue,
    v8::DeserializeContextDataCallback data, v8::DeserializeAPIWrapperCallback api) {
    if (failure("context-allocation")) {
        record('X');
        fail_next_allocation = true;
    }
    try {
        auto result =
            __real__ZN2v87Context3NewEPNS_7IsolateEPNS_22ExtensionConfigurationENS_10MaybeLocalINS_14ObjectTemplateEEENS5_INS_5ValueEEENS_33DeserializeInternalFieldsCallbackEPNS_14MicrotaskQueueENS_30DeserializeContextDataCallbackENS_29DeserializeAPIWrapperCallbackE(
                isolate, extensions, object_template, object, fields, queue, data, api);
        fail_next_allocation = false;
        return result;
    } catch (...) {
        fail_next_allocation = false;
        throw;
    }
}

extern "C" v8::MaybeLocal<v8::Script>
__real__ZN2v86Script7CompileENS_5LocalINS_7ContextEEENS1_INS_6StringEEEPNS_12ScriptOriginE(
    v8::Local<v8::Context>, v8::Local<v8::String>, v8::ScriptOrigin *);
extern "C" v8::MaybeLocal<v8::Script>
__wrap__ZN2v86Script7CompileENS_5LocalINS_7ContextEEENS1_INS_6StringEEEPNS_12ScriptOriginE(
    v8::Local<v8::Context> context, v8::Local<v8::String> source, v8::ScriptOrigin *origin) {
    if (failure("compile-internal-allocation")) {
        record('K');
        fail_next_allocation = true;
    }
    v8::MaybeLocal<v8::Script> result;
    try {
        result =
            __real__ZN2v86Script7CompileENS_5LocalINS_7ContextEEENS1_INS_6StringEEEPNS_12ScriptOriginE(
                context, source, origin);
        fail_next_allocation = false;
    } catch (...) {
        fail_next_allocation = false;
        throw;
    }
    if (failure("compile-allocation") && result.IsEmpty()) {
        record('C');
        fail_next_allocation = true;
    }
    return result;
}

extern "C" v8::MaybeLocal<v8::Value>
__real__ZN2v86Script3RunENS_5LocalINS_7ContextEEE(v8::Script *, v8::Local<v8::Context>);
extern "C" v8::MaybeLocal<v8::Value>
__wrap__ZN2v86Script3RunENS_5LocalINS_7ContextEEE(v8::Script *script,
                                                  v8::Local<v8::Context> context) {
    if (failure("run-fatal-allocation")) {
        record('F');
        fail_next_allocation = true;
    }
    v8::MaybeLocal<v8::Value> result;
    try {
        result = __real__ZN2v86Script3RunENS_5LocalINS_7ContextEEE(script, context);
        fail_next_allocation = false;
    } catch (...) {
        fail_next_allocation = false;
        throw;
    }
    if (failure("run-allocation") && result.IsEmpty()) {
        record('E');
        fail_next_allocation = true;
    }
    return result;
}

extern "C" void
__real__ZN2v86String9Utf8ValueC1EPNS_7IsolateENS_5LocalINS_5ValueEEENS0_12WriteOptionsE(
    v8::String::Utf8Value *, v8::Isolate *, v8::Local<v8::Value>, v8::String::WriteOptions);
extern "C" void
__wrap__ZN2v86String9Utf8ValueC1EPNS_7IsolateENS_5LocalINS_5ValueEEENS0_12WriteOptionsE(
    v8::String::Utf8Value *value, v8::Isolate *isolate, v8::Local<v8::Value> source,
    v8::String::WriteOptions options) {
    if (failure("conversion-internal-allocation")) {
        record('V');
        fail_next_allocation = true;
    }
    try {
        __real__ZN2v86String9Utf8ValueC1EPNS_7IsolateENS_5LocalINS_5ValueEEENS0_12WriteOptionsE(
            value, isolate, source, options);
        fail_next_allocation = false;
    } catch (...) {
        fail_next_allocation = false;
        throw;
    }
    if (failure("result-allocation") || failure("conversion-allocation")) {
        record('R');
        fail_next_allocation = true;
    }
}

extern "C" std::unique_ptr<v8::Platform>
__real__ZN2v88platform18NewDefaultPlatformEiNS0_15IdleTaskSupportENS0_21InProcessStackDumpingESt10unique_ptrINS_17TracingControllerESt14default_deleteIS4_EENS0_12PriorityModeE(
    int, v8::platform::IdleTaskSupport, v8::platform::InProcessStackDumping,
    std::unique_ptr<v8::TracingController>, v8::platform::PriorityMode);
extern "C" std::unique_ptr<v8::Platform>
__wrap__ZN2v88platform18NewDefaultPlatformEiNS0_15IdleTaskSupportENS0_21InProcessStackDumpingESt10unique_ptrINS_17TracingControllerESt14default_deleteIS4_EENS0_12PriorityModeE(
    int threads, v8::platform::IdleTaskSupport idle, v8::platform::InProcessStackDumping dumping,
    std::unique_ptr<v8::TracingController> tracing, v8::platform::PriorityMode priority) {
    if (failure("platform-allocation")) {
        record('A');
        throw std::bad_alloc();
    }
    return __real__ZN2v88platform18NewDefaultPlatformEiNS0_15IdleTaskSupportENS0_21InProcessStackDumpingESt10unique_ptrINS_17TracingControllerESt14default_deleteIS4_EENS0_12PriorityModeE(
        threads, idle, dumping, std::move(tracing), priority);
}

extern "C" bool __real__ZN2v82V810InitializeEi(int);
extern "C" bool __wrap__ZN2v82V810InitializeEi(int configuration) {
    if (failure("initialize")) {
        record('I');
        return false;
    }
    return __real__ZN2v82V810InitializeEi(configuration);
}

extern "C" void __real__ZN2v82V818InitializePlatformEPNS_8PlatformE(v8::Platform *);
extern "C" void __wrap__ZN2v82V818InitializePlatformEPNS_8PlatformE(v8::Platform *platform) {
    if (failure("platform-init-allocation")) {
        record('T');
        fail_next_allocation = true;
    }
    try {
        __real__ZN2v82V818InitializePlatformEPNS_8PlatformE(platform);
        fail_next_allocation = false;
    } catch (...) {
        fail_next_allocation = false;
        throw;
    }
}

extern "C" v8::Isolate *
__real__ZN2v87Isolate3NewERKNS0_12CreateParamsE(const v8::Isolate::CreateParams &);
extern "C" v8::Isolate *
__wrap__ZN2v87Isolate3NewERKNS0_12CreateParamsE(const v8::Isolate::CreateParams &parameters) {
    if (failure("isolate")) {
        record('N');
        return nullptr;
    }
    if (failure("isolate-allocation")) {
        record('J');
        fail_next_allocation = true;
    }
    try {
        auto *result = __real__ZN2v87Isolate3NewERKNS0_12CreateParamsE(parameters);
        fail_next_allocation = false;
        return result;
    } catch (...) {
        fail_next_allocation = false;
        throw;
    }
}

extern "C" v8::MaybeLocal<v8::String>
__real__ZN2v86String11NewFromUtf8EPNS_7IsolateEPKcNS_13NewStringTypeEi(v8::Isolate *, const char *,
                                                                       v8::NewStringType, int);
extern "C" v8::MaybeLocal<v8::String>
__wrap__ZN2v86String11NewFromUtf8EPNS_7IsolateEPKcNS_13NewStringTypeEi(v8::Isolate *isolate,
                                                                       const char *data,
                                                                       v8::NewStringType type,
                                                                       int length) {
    if (failure("source") || failure("source-allocation")) {
        record('S');
        fail_next_allocation = failure("source-allocation");
        return {};
    }
    return __real__ZN2v86String11NewFromUtf8EPNS_7IsolateEPKcNS_13NewStringTypeEi(isolate, data,
                                                                                  type, length);
}
