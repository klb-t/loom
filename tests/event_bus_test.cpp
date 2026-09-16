#include "loom/event_bus.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    loom::EventBus bus;
    int calls = 0;
    std::string last;

    const auto id = bus.subscribe(loom::kMsgCreated, [&](const loom::Event& e) {
        ++calls;
        last = e.payload_json;
        (void)bus.subscribe("noop", [](const loom::Event&) {});
    });
    assert(id != 0);

    bus.emit({loom::kMsgCreated, R"({"id":"m_test"})"});
    assert(calls == 1);
    assert(last == R"({"id":"m_test"})");
    assert(bus.unsubscribe(loom::kMsgCreated, id));
    bus.emit({loom::kMsgCreated, "{}"});
    assert(calls == 1);

    std::cout << "event_bus_test: OK\n";
    return 0;
}
