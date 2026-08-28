// Compilation Fabric - Observability: Snapshot, Stats, Event, Explain.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Json.hpp"
#include <map>
#include <mutex>
#include <deque>
#include <string>

namespace compilationfabric {

struct Event {
    int64_t timeMs = 0;
    std::string type;
    Json data = Json::null();
};

class Observability {
public:
    explicit Observability(size_t maxEvents = 4096);

    void count(std::string_view key, int64_t delta = 1);
    void set(std::string_view key, int64_t value);       // overwrite (e.g. generation)
    int64_t get(std::string_view key) const;
    void recordEvent(std::string type, Json data = Json::null());

    // Snapshot: a consistent, bounded view of all tracked metrics + recent events.
    Json snapshot() const;
    // Stats: just the counters (lighter weight).
    Json stats() const;
    std::vector<Event> recentEvents(size_t limit) const;

    // Structured explain accumulator used across the runtime.
    class ExplainBuilder {
    public:
        void add(std::string key, Json value);
        void addText(std::string key, std::string value);
        Json build();
        void clear();
    private:
        std::map<std::string, Json> fields_;
    };

private:
    mutable std::mutex m_;
    std::map<std::string, int64_t> counters_;
    std::deque<Event> events_;
    size_t maxEvents_;
};

} // namespace compilationfabric
