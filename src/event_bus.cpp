#include "loom/event_bus.h"

#include <algorithm>

namespace loom {

EventBus::SubscriptionId EventBus::subscribe(std::string event_name, Handler handler) {
    if (!handler) {
        return 0;
    }
    std::lock_guard lock(mutex_);
    const auto id = next_id_++;
    handlers_[std::move(event_name)].push_back(Subscription{id, std::move(handler)});
    return id;
}

bool EventBus::unsubscribe(const std::string& event_name, SubscriptionId id) {
    std::lock_guard lock(mutex_);
    const auto it = handlers_.find(event_name);
    if (it == handlers_.end()) {
        return false;
    }
    auto& entries = it->second;
    const auto old_size = entries.size();
    std::erase_if(entries, [id](const Subscription& sub) { return sub.id == id; });
    if (entries.empty()) {
        handlers_.erase(it);
    }
    return entries.size() != old_size;
}

void EventBus::emit(const Event& event) const {
    std::vector<Subscription> snapshot;
    {
        std::lock_guard lock(mutex_);
        const auto it = handlers_.find(event.name);
        if (it != handlers_.end()) {
            snapshot = it->second;
        }
    }
    for (const auto& sub : snapshot) {
        if (sub.handler) {
            sub.handler(event);
        }
    }
}

}  // namespace loom
