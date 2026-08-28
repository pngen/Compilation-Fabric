# Recovery

On restart the runtime scans persisted metadata, verifies checksums and artifact
digests, rejects corruption/truncation/unsupported versions, reconstructs the
canonical index, preserves valid current generations, discards stale or invalidated
artifacts from eligibility, recovers provenance/reproducibility state, marks
backend/module residency absent, and requires re-load/re-deploy where needed.
Orphan temp files and failed-compile residue are cleaned.
