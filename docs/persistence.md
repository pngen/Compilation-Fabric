# Persistence

Persistent artifact storage is versioned and checksummed. Each artifact generation
is stored as two files: versioned, checksummed metadata (JSON + canonical key
bytes) and artifact content bytes. Writes are atomic (temp file + replace/rename).
Loads verify the version header, the metadata checksum, and the content digest.
Unknown versions, truncation, corruption, and trailing garbage are rejected
explicitly. Orphan temp files are cleaned on recovery.
