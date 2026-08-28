// Compilation Fabric - Guarded lifecycle state machine.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include <string_view>
#include <array>
#include <vector>

namespace compilationfabric {

// The full artifact/attempt lifecycle. Transitions are explicit and validated;
// every transition requires a legal edge from the current state. Terminal states
// have no outgoing edges. Invalidated/corrupt/retired states cannot silently
// regain eligibility because they have no legal edge back to a valid state.
enum class State : uint8_t {
    Submitted, Planning, Planned, Queued, Preparing, Compiling, Optimizing, Linking,
    Validating, Valid, Persisting, Persisted, Deployable, Deploying, Deployed, InUse,
    Superseded, InvalidationPending, Invalidated, Corrupt, Failed, Cancelled, Retired, Terminal
};

std::string_view stateName(State s);
std::optional<State> stateFromName(std::string_view s);
bool isTerminal(State s);
bool isActiveOrValid(State s);

// Legal transition edges. Returns true if the transition is allowed.
bool canTransition(State from, State to);

// Returns the reached state name for a transition, or an error if illegal.
Result<State> transition(State from, State to);

// A thread-safe guarded state holder that validates transitions and never allows
// a stale or invalid state machine to become valid again.
class Lifecycle {
public:
    explicit Lifecycle(State initial = State::Submitted) : state_(initial) {}

    State state() const { std::lock_guard<std::mutex> l(m_); return state_; }
    Result<void> to(State next);
    Result<void> assertCan(State next) const;
    bool isTerminalState() const { return isTerminal(state()); }
    std::string name() const { return std::string(stateName(state())); }

    // A lifecycle that is bound to a generation; rollback of generation is
    // rejected by the orchestrator, not here.
private:
    mutable std::mutex m_;
    State state_;
};

} // namespace compilationfabric
