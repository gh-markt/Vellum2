#include "vellum/script_runtime.hh"
#include "vellum/script_process.hh"
#include <libplatform/libplatform.h>
#include <memory>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <v8.h>
namespace {
vellum::script_result execute(v8::Isolate *isolate, std::string_view source) {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handles(isolate);
    const auto context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    v8::TryCatch caught(isolate);
    v8::Local<v8::String> text;
    if (!v8::String::NewFromUtf8(isolate, source.data(), v8::NewStringType::kNormal,
                                 static_cast<int>(source.size()))
             .ToLocal(&text)) {
        return {false, "invalid script source"};
    }
    v8::Local<v8::Script> script;
    v8::Local<v8::Value> value;
    if (!v8::Script::Compile(context, text).ToLocal(&script) ||
        !script->Run(context).ToLocal(&value)) {
        return {false, "JavaScript compilation or execution failed"};
    }
    v8::String::Utf8Value result(isolate, value);
    if (*result == nullptr) {
        return {false, "JavaScript result conversion failed"};
    }
    return {true, std::string(*result, result.length())};
}
} // namespace
int vellum::run_v8_worker() {
    const rlimit no_core{0, 0};
    if (::setrlimit(RLIMIT_CORE, &no_core) != 0 || ::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        return 2;
    }
    v8::V8::InitializeICUDefaultLocation(nullptr);
    auto platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    if (!v8::V8::Initialize()) {
        return 2;
    }
    auto allocator = std::unique_ptr<v8::ArrayBuffer::Allocator>(
        v8::ArrayBuffer::Allocator::NewDefaultAllocator());
    v8::Isolate::CreateParams parameters;
    parameters.array_buffer_allocator = allocator.get();
    parameters.constraints.set_max_old_generation_size_in_bytes(128 * 1024 * 1024);
    auto *isolate = v8::Isolate::New(parameters);
    if (isolate == nullptr) {
        return 2;
    }
    const auto handler = [isolate](std::string_view source) { return execute(isolate, source); };
    const int status = vellum::serve_script_request(handler);
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    return status;
}
