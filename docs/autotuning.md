# Autotuning

Autotuning is a **bounded** compilation-variant mechanism. Candidate artifacts are
generated from controlled variations (optimization level, block size, launch
parameters, unroll factor, compile-time specialization), run through deterministic
validation and measured benchmark evaluation, and the winning variant is chosen by
an explicit policy. The runtime records the candidate identities, exact flags,
measured validation result, measured performance, the selected winner, and the
rejected candidates with the reason for selection. It does not claim global
optimality; the result is the best among evaluated candidates.
