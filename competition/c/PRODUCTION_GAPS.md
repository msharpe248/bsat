# Next production milestones

Authorized scope: phase/memory diagnosis, measured memory reduction, controlled
search-policy ablations, safe incremental reuse, service embedding, and broader
stateful/certificate assurance. Commit and push each validated milestone.

1. Accounting: complete for the measured scope. Opt-in inclusive CPU, retained
   capacities, congruence temporaries and rebuild overlap identify concrete
   memory targets. See `ACCOUNTING.md` for limits and validation.
2. Memory: first measured reduction complete. Lazy congruence tables save
   64 MiB on two workloads with matching work/proof signatures; see
   `LAZY_CONGRUENCE.md`. Largest-input reconstruction/watch costs remain.
3. Search: all 36 frozen ablation runs audited. Omitting VMTF improves one
   argumentation input about 7x in wall time; reg-n/Stedman remain unresolved.
   Defaults unchanged; see `SEARCH_ABLATIONS.md`.
4. Incremental reuse: opt-in conservative fast path implemented; incompatible
   transformations and interrupted/conditional-UNSAT states rebuild. See
   `INCREMENTAL_REUSE.md` for stateful, allocation and timing evidence.
5. Embedding: opaque ABI-v1 shared library, independent instances, per-instance
   diagnostics, CLI-owned signals and cooperative cancellation implemented.
   See `API_CONTRACT.md` and `EMBEDDING.md`.
6. Assurance: 256-variable exact-oracle histories, sanitizer soak CI, explicit
   conditional-certificate bundles, timeout containment and verification-cost
   reporting implemented. Three industrial UNSAT chains accepted; see
   `ASSURANCE_EXPANSION.md` and `CONDITIONAL_CERTIFICATES.md`.

Linux PMU tools already exist, but actual target measurements still require a
Linux host. No target-server result is implied by local macOS measurements.
