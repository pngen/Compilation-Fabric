// Compilation Fabric - Observability.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Observability.hpp"

namespace compilationfabric {

Observability::Observability(size_t maxEvents) : maxEvents_(maxEvents) {}

void Observability::count(std::string_view key, int64_t delta) {
    std::lock_guard<std::mutex> l(m_);
    counters_[std::string(key)] += delta;
}
void Observability::set(std::string_view key, int64_t value) {
    std::lock_guard<std::mutex> l(m_);
    counters_[std::string(key)] = value;
}
int64_t Observability::get(std::string_view key) const {
    std::lock_guard<std::mutex> l(m_);
    auto it = counters_.find(std::string(key));
    return it == counters_.end() ? 0 : it->second;
}
void Observability::recordEvent(std::string type, Json data) {
    std::lock_guard<std::mutex> l(m_);
    Event e; e.timeMs = Clock::nowMillis(); e.type = std::move(type); e.data = std::move(data);
    events_.push_back(std::move(e));
    while (events_.size() > maxEvents_) events_.pop_front();
}

Json Observability::snapshot() const {
    std::lock_guard<std::mutex> l(m_);
    Json j = Json::object({});
    Json counters = Json::object({});
    for (auto& [k, v] : counters_) counters.set(k, Json::number(static_cast<double>(v)));
    j.set("counters", std::move(counters));
    std::vector<Json> ev;
    for (auto& e : events_) {
        Json je = Json::object({});
        je.set("time_ms", Json::number(static_cast<double>(e.timeMs)));
        je.set("type", Json::str(e.type));
        je.set("data", e.data);
        ev.push_back(std::move(je));
    }
    j.set("events", Json::array(std::move(ev)));
    return j;
}

Json Observability::stats() const {
    std::lock_guard<std::mutex> l(m_);
    Json j = Json::object({});
    for (auto& [k, v] : counters_) j.set(k, Json::number(static_cast<double>(v)));
    return j;
}

std::vector<Event> Observability::recentEvents(size_t limit) const {
    std::lock_guard<std::mutex> l(m_);
    std::vector<Event> out;
    size_t start = events_.size() > limit ? events_.size() - limit : 0;
    for (size_t i = start; i < events_.size(); ++i) out.push_back(events_[i]);
    return out;
}

void Observability::ExplainBuilder::add(std::string key, Json value) {
    if (fields_.empty()) fields_.clear();
    fields_.insert_or_assign(std::move(key), std::move(value));
}
void Observability::ExplainBuilder::addText(std::string key, std::string value) {
    add(std::move(key), Json::str(std::move(value)));
}
Json Observability::ExplainBuilder::build() {
    std::map<std::string, Json> o = fields_;
    return Json::object(std::move(o));
}
void Observability::ExplainBuilder::clear() { fields_.clear(); }

} // namespace compilationfabric
