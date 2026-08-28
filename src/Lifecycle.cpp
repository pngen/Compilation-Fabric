// Compilation Fabric - Lifecycle.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Lifecycle.hpp"

namespace compilationfabric {

std::string_view stateName(State s) {
    switch (s) {
        case State::Submitted: return "Submitted";
        case State::Planning: return "Planning";
        case State::Planned: return "Planned";
        case State::Queued: return "Queued";
        case State::Preparing: return "Preparing";
        case State::Compiling: return "Compiling";
        case State::Optimizing: return "Optimizing";
        case State::Linking: return "Linking";
        case State::Validating: return "Validating";
        case State::Valid: return "Valid";
        case State::Persisting: return "Persisting";
        case State::Persisted: return "Persisted";
        case State::Deployable: return "Deployable";
        case State::Deploying: return "Deploying";
        case State::Deployed: return "Deployed";
        case State::InUse: return "InUse";
        case State::Superseded: return "Superseded";
        case State::InvalidationPending: return "InvalidationPending";
        case State::Invalidated: return "Invalidated";
        case State::Corrupt: return "Corrupt";
        case State::Failed: return "Failed";
        case State::Cancelled: return "Cancelled";
        case State::Retired: return "Retired";
        case State::Terminal: return "Terminal";
    }
    return "Unknown";
}
std::optional<State> stateFromName(std::string_view s) {
    const std::pair<const char*, State> arr[] = {
        {"Submitted",State::Submitted},{"Planning",State::Planning},{"Planned",State::Planned},{"Queued",State::Queued},
        {"Preparing",State::Preparing},{"Compiling",State::Compiling},{"Optimizing",State::Optimizing},{"Linking",State::Linking},
        {"Validating",State::Validating},{"Valid",State::Valid},{"Persisting",State::Persisting},{"Persisted",State::Persisted},
        {"Deployable",State::Deployable},{"Deploying",State::Deploying},{"Deployed",State::Deployed},{"InUse",State::InUse},
        {"Superseded",State::Superseded},{"InvalidationPending",State::InvalidationPending},{"Invalidated",State::Invalidated},
        {"Corrupt",State::Corrupt},{"Failed",State::Failed},{"Cancelled",State::Cancelled},{"Retired",State::Retired},
        {"Terminal",State::Terminal}};
    for (auto& p : arr) if (s == p.first) return p.second;
    return std::nullopt;
}
bool isTerminal(State s) {
    switch (s) {
        case State::Invalidated:
        case State::Corrupt:
        case State::Failed:
        case State::Cancelled:
        case State::Retired:
        case State::Terminal:
            return true;
        default: return false;
    }
}
bool isActiveOrValid(State s) {
    switch (s) {
        case State::Valid:
        case State::Persisted:
        case State::Deployable:
        case State::Deployed:
        case State::InUse:
            return true;
        default: return false;
    }
}

bool canTransition(State from, State to) {
    // Terminal states have no outgoing edges.
    if (isTerminal(from)) return false;
    switch (from) {
        case State::Submitted:
            return to == State::Planning || to == State::Failed || to == State::Cancelled;
        case State::Planning:
            return to == State::Planned || to == State::Failed || to == State::Cancelled;
        case State::Planned:
            return to == State::Queued || to == State::Failed || to == State::Cancelled;
        case State::Queued:
            return to == State::Preparing || to == State::Cancelled;
        case State::Preparing:
            return to == State::Compiling || to == State::Failed || to == State::Cancelled;
        case State::Compiling:
            return to == State::Optimizing || to == State::Linking || to == State::Validating ||
                   to == State::Failed || to == State::Cancelled;
        case State::Optimizing:
            return to == State::Linking || to == State::Validating || to == State::Failed || to == State::Cancelled;
        case State::Linking:
            return to == State::Validating || to == State::Failed || to == State::Cancelled;
        case State::Validating:
            return to == State::Valid || to == State::Persisting || to == State::Failed || to == State::Corrupt;
        case State::Valid:
            return to == State::Persisting || to == State::Persisted || to == State::Deployable ||
                   to == State::Deployed || to == State::InUse || to == State::Superseded ||
                   to == State::InvalidationPending;
        case State::Persisting:
            return to == State::Persisted || to == State::Deployable || to == State::Failed ||
                   to == State::InvalidationPending;
        case State::Persisted:
            return to == State::Deployable || to == State::Deployed || to == State::InUse ||
                   to == State::Superseded || to == State::InvalidationPending;
        case State::Deployable:
            return to == State::Deploying || to == State::Deployed || to == State::InUse ||
                   to == State::Superseded || to == State::InvalidationPending;
        case State::Deploying:
            return to == State::Deployed || to == State::InUse || to == State::Failed || to == State::InvalidationPending;
        case State::Deployed:
            return to == State::InUse || to == State::Superseded || to == State::InvalidationPending;
        case State::InUse:
            return to == State::Deployed || to == State::Superseded || to == State::InvalidationPending;
        case State::Superseded:
            return to == State::InvalidationPending || to == State::Retired;
        case State::InvalidationPending:
            return to == State::Invalidated || to == State::Retired;
        case State::Invalidated:
        case State::Corrupt:
        case State::Failed:
        case State::Cancelled:
        case State::Retired:
        case State::Terminal:
        default:
            return false;
    }
}

Result<State> transition(State from, State to) {
    if (!canTransition(from, to))
        return Err<State>(ErrorCode::InvalidStateTransition,
            std::string(stateName(from)) + " -> " + std::string(stateName(to)) + " is not a legal lifecycle transition");
    return Ok(to);
}

Result<void> Lifecycle::to(State next) {
    std::lock_guard<std::mutex> l(m_);
    if (!canTransition(state_, next))
        return ErrVoid(ErrorCode::InvalidStateTransition,
            std::string(stateName(state_)) + " -> " + std::string(stateName(next)) + " is not a legal lifecycle transition");
    state_ = next;
    return OkVoid();
}

Result<void> Lifecycle::assertCan(State next) const {
    std::lock_guard<std::mutex> l(m_);
    if (!canTransition(state_, next))
        return ErrVoid(ErrorCode::InvalidStateTransition,
            std::string(stateName(state_)) + " -> " + std::string(stateName(next)) + " is not a legal lifecycle transition");
    return OkVoid();
}

} // namespace compilationfabric
