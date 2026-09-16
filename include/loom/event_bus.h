#pragma once

#include "loom/status.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace loom {

inline constexpr const char* kMsgCreated = "message:created";
inline constexpr const char* kMsgUpdated = "message:updated";
inline constexpr const char* kNodeCreated = "node:created";
inline constexpr const char* kEdgeCreated = "edge:created";
inline constexpr const char* kConvCreated = "conv:created";
inline constexpr const char* kConvSwitched = "conv:switched";
inline constexpr const char* kGraphChanged = "graph:changed";
inline constexpr const char* kImportDone = "import:done";
inline constexpr const char* kSemanticProgress = "semantic:progress";

struct Event {
    std::string name;
    std::string payload_json;
};

class EventBus {
public:
    using Handler = std::function<void(const Event&)>;
    using SubscriptionId = std::uint64_t;

    [[nodiscard]] SubscriptionId subscribe(std::string event_name, Handler handler);
    [[nodiscard]] bool unsubscribe(const std::string& event_name, SubscriptionId id);
    void emit(const Event& event) const;

private:
    struct Subscription {
        SubscriptionId id{};
        Handler handler{};
    };

    mutable std::mutex mutex_;
    mutable SubscriptionId next_id_{1};
    std::unordered_map<std::string, std::vector<Subscription>> handlers_;
};

}  // namespace loom
