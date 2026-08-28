// Compilation Fabric - Versioned persistent artifact storage.
//
// Persistence uses a two-file scheme per artifact generation:
//   <id>_<gen>.meta  - versioned, checksummed metadata (JSON + key canonical bytes)
//   <id>_<gen>.bin   - artifact content bytes
//
// Writes are atomic (temp file + replace/rename). Loads verify the version
// header, the metadata checksum, and the content digest. Unknown versions,
// truncation, corruption and trailing garbage are all rejected explicitly.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Descriptor.hpp"
#include "CompilationFabric/Key.hpp"
#include "CompilationFabric/Json.hpp"
#include <vector>
#include <filesystem>

namespace compilationfabric {

struct PersistedRecord {
    ArtifactId id;
    ArtifactGeneration generation = 0;
    CompilationKey key;
    ArtifactDescriptor descriptor;
    std::vector<uint8_t> contentBytes;
    bool invalidated = false;
    bool superseded = false;
    bool corrupted = false;
    std::string error;
};

class PersistenceStore {
public:
    PersistenceStore();
    explicit PersistenceStore(std::filesystem::path root);

    // Creates the root directory if absent (isolated per-run scratch).
    Result<void> open();
    const std::filesystem::path& root() const { return root_; }

    // Atomic, checksummed write of both metadata and content for one generation.
    Result<void> store(const PersistedRecord& record);
    // Read + fully validate one generation. Corrupt/truncated/unknown version all
    // return an error with a precise reason.
    Result<PersistedRecord> load(const ArtifactId& id, ArtifactGeneration gen) const;
    Result<PersistedRecord> loadLatest(const ArtifactId& id) const;

    // List all metadata records present on disk (regardless of validity).
    std::vector<std::pair<ArtifactId, ArtifactGeneration>> list() const;

    // Remove a generation's files (used by purge/retire).
    Result<void> remove(const ArtifactId& id, ArtifactGeneration gen);

    // Scans the whole store, verifies every record, cleans orphan temp files, and
    // returns an ordered view (valid vs corrupted vs invalidated).
    struct RecoveryResult {
        std::vector<PersistedRecord> valid;
        std::vector<PersistedRecord> corrupted;
        std::vector<PersistedRecord> invalidated;
        std::vector<std::filesystem::path> orphanTempRemoved;
    };
    Result<RecoveryResult> recover();

    // Returns true if this record file's persistence version is known.
    static constexpr uint32_t kVersion = 1;

private:
    std::filesystem::path root_;
};

} // namespace compilationfabric
