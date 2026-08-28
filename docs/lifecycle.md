# Lifecycle

Every compilation attempt and artifact follows an explicit, guarded lifecycle.
Transitions are validated; no legal edge returns from a terminal state, so an
invalidated, corrupt, or retired artifact cannot silently regain eligibility.

```text
Submitted -> Planning -> Planned -> Queued -> Preparing -> Compiling
  -> Optimizing -> Linking -> Validating -> Valid -> Persisting -> Persisted
  -> Deployable -> Deploying -> Deployed -> InUse
Valid/Deployable/Persisted/InUse -> Superseded -> InvalidationPending -> Invalidated/Retired
Failures/errors go to Failed or Corrupt; Cancelled is explicit.
```

## Rules

- No duplicate authoritative publish.
- No stale attempt overwrites a newer generation.
- An invalidated/corrupt/retired artifact cannot silently regain eligibility.
- Terminal states have no outgoing edges.
