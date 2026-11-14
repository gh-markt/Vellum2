#include "vellum/script_runtime.hh"
int main() {
    try {
        return vellum::run_v8_worker();
    } catch (...) {
        return 2;
    }
}
