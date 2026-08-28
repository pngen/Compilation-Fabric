# Validation Matrix

The repository validates: clean Release and Debug builds with `/W4 /WX` and zero
warnings; all CTest targets; repeated Release and Debug runs; substantial
fixed-seed property testing; a dedicated adversarial suite; a dedicated
concurrency suite; an explicit lock/deadlock audit; a dedicated persistence and
recovery suite; the real CompilationKey; real compatibility decisions; real
planning; real CPU compilation; real NVRTC compilation; real CUDA module load and
launch (where device access is available); real specialization; real single-flight
compile; real invalidation/recompile; real reproducibility evidence; real
persistence/recovery; real corruption rejection; real toolchain and target
discovery; framed TCP; a coordinator; two worker processes; worker kill and restart
with a new WorkerBootId; coordinator epoch rollover; stale-authority replay;
measured benchmarks; runnable examples; the full CLI inventory; install/export
validation; an external find_package consumer; a documentation leak audit; Mermaid
validation; and the README ending.
