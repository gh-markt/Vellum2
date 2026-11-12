#include "vellum/script_process.hh"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <sys/socket.h>
#include <thread>
int main() {
    const auto handler = [](std::string_view source) -> vellum::script_result {
        if (source.starts_with("malformed")) {
            std::array<std::uint32_t, 5> packet{0x564c4d31, 1, 0, 0, 0};
            std::size_t size = 16;
            if (source == "malformed magic") {
                packet[0] = 0;
            }
            if (source == "malformed version") {
                packet[1] = 2;
            }
            if (source == "malformed length") {
                packet[2] = 1024 * 1024 + 1;
            }
            if (source == "malformed status") {
                packet[3] = 2;
            }
            if (source == "malformed short") {
                size = 4;
            }
            if (source == "malformed long") {
                size = 20;
            }
            ::send(3, packet.data(), size, MSG_NOSIGNAL);
            std::_Exit(0);
        }
        if (source == "hang") {
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
        if (source == "error") {
            return {false, "expected failure"};
        }
        if (source == "crash") {
            std::_Exit(9);
        }
        return {true, std::string(source)};
    };
    return vellum::serve_script_request(handler);
}
