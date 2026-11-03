# Persistence, recovery and external stems

Inherit RULES, F-DATA/F-STEM. Current-format roundtrips are mandatory; old-session migration/behaviour is not a release requirement. All rules NOT RUN.

### MW-D01 — Current session roundtrip retains all musical owners
- Contract/source: Model projectToJson/parseProject, Engine capture/prepare/commit; format41.
- Cases/procedure: serialize/reopen F-DATA containing every current field: clips/geometry/links/muting, tempo/labels, original effect ownership, node curves, master, plugins/bindings, sections/HC, stems, launch settings, takes/events. Multiple independent placements and split pieces; Undo/Redo then save; Unicode source/session paths.
- Pass/oracle: independent canonical field/identity table and rendered PCM preserved, except explicitly nonsaved transient UI/runtime state. Open stopped, no pending launches; current data never depends on a legacy migration path. Byte-equal JSON not required when ordering/formatting differs.
- Evidence/mode/severity: disk JSON semantic diff, identity graph and before/reopen PCM, native saved handlers; automated; P1.

### MW-D02 — Parser rejects malformed or hostile documents before side effects
- Contract/source: Model::parseProject and separate validation/prepare/commit boundary; imported sessions/plugins/media paths are trust inputs.
- Cases/procedure: invalid encoding/NUL/oversize/deep structures/types, NaN/Inf through boundary construction, duplicate IDs/orphan links/curves, illegal lane/frame/node/take values, traversal/relative/linked/UNC paths as supported, future/unsupported versions and missing plugin descriptions; deterministic bounded invalid corpus, no uncontrolled fuzzing.
- Pass/oracle: explicit error, no media decode/plugin load/write before structural acceptance; active state/files remain unchanged, bounded memory/time under REQ-05 parser fixture budget. Finite fixtures use hand-authored expected validity, not roundtrip parser output alone. Unsupported historical input may reject safely; no requirement to migrate.
- Evidence/mode/severity: invalid corpus manifest, expected decision, side-effect counters and preserved-state hashes; static + automated; P0 where arbitrary overwrite/crash, otherwise P1.

### MW-D03 — Save and replacement are atomic and destination-safe
- Contract/source: Engine::saveProject, validateWriteDestination, workspace/style/profile stores and TemporaryFile writes.
- Cases/procedure: new/saveAs/replace, cancel chooser/overwrite confirmation, existing backup, source path/hardlink alias, read-only/disk-full/permission/lock/symlink, interruption before/after temporary flush and competing owned instances.
- Pass/oracle: success only for durable complete parsed document, backup preserves previous target, cancellation/failure keeps prior destination/current musical state. No imported original media overwritten; no partial document presented as valid. Validate independent read and file identity, not success text.
- Evidence/mode/severity: original/temporary/final/backup hashes, injected write-stage outcomes and re-read; automated + native picker/manual; P0 data loss, otherwise P1.

### MW-D04 — Missing required media can be relinked transactionally
- Contract/source: Engine::prepareProject and Model::relinkProjectSource, current admission/relink UI.
- Cases/procedure: required missing/moved/corrupt source versus unused library missing source, matching replacement/different identity/wrong format, one/multiple missing files, Locate success/cancel/failure, cancellation during hydration/plugin preparation and new project while stale callback pending.
- Pass/oracle: unused library absence doesn't reject arrangement; required error names affected source and deliberate Locate flow verifies identity/dimensions. Failure/cancel leaves current project, plugin state, file and prepared ownership unchanged; successful relink saves/reopens explicit corrected references, never hidden substitution.
- Evidence/mode/severity: source-ID/disk diff, native actual picker trace and final output comparison; automated + manual; P1.

### MW-D05 — Recovery protects live instances and preserves usable snapshots
- Contract/source: RecoveryStore lease/current/previous snapshots and RecoveryUI, docs/RECOVERY-AND-MONITORING.md.
- Cases/procedure: owned crash fixture with dirty mix, corrupt current/valid previous, both invalid, process still holding lease, two instances, write failure, Recover/Open/Discard/cancel then restart; graceful close while recovery worker pending.
- Pass/oracle: running instance recovery never claimed/deleted, fallback only to validated candidate, explicit action/backup semantics, recovered current-format musical state correct. Failed recovery preserves source evidence and current project; no production kill/crash for testing. Background publication/teardown has no stale callback.
- Evidence/mode/severity: per-instance lease/PID identities, current/previous hashes, recovery semantic+PCM diff and UI; automated/owned lifecycle + manual; P1.

### MW-D06 — Caches, preferences and references have independent retention owners
- Contract/source: AnalysisCache/PcmCache/LibraryCatalogue/WorkspaceLayout/DownbeatReferences and AGENTS preservation instructions.
- Cases/procedure: corrupt/partial/current/future cache, cache-location change and rollback, unknown workspace fields/invalid named layout, source corrections and reference observations; test only isolated stores. Inspect cleanup exact-path/link/hardlink rules and simulate running process conflict.
- Pass/oracle: reuse/invalidation by compatibility identity, no automatic broad deletion; workspace changes outside musical Undo, unknown preference data kept by specified merge, reviewed DownbeatReferences append-only and independently preserved. Detector-update reset only after verified detector change with validated owned production targets and latest protection instructions; not performed by ordinary audit or performance update.
- Evidence/mode/severity: explicit store/ownership map, before/after manifest and cache decisions, retention journal; static + isolated tests; P0 protected data loss, otherwise P1.

### MW-D07 — Simultaneous saves and shutdown cannot publish stale state
- Contract/source: Workstation job pools, generation guards, store InterProcessLocks and Engine prepare/commit split.
- Cases/procedure: deterministic interleavings save+edit, open+cancel+new, analysis+manual save, workspace/profile concurrent owned instances, shutdown during export/import/recovery/stem/take seek.
- Pass/oracle: accepted state is named/current generation; competing writes reject or serialize explicitly, no deadlock/abandoned owned child or late mutation. All callbacks consumed/cancelled before object destruction; valid subsequent action succeeds. Measure join/cancel time against approved REQ-05 limit rather than waiting indefinitely and calling it PASS.
- Evidence/mode/severity: dependency/lock graph, deterministic scheduling log and final files/process/thread state; static + automated; P1.

### MW-S01 — Stem runtime identity is checked and installation is explicit
- Contract/source: StemJobs/runtime config, scripts/setup-stems.py/stem_worker.py, docs/STEM-SEPARATION.md and LICENSING.md.
- Cases/procedure: installed valid runtime versus missing/changed worker/model/version/config; offline separation, CUDA available/unavailable/OOM fallback to CPU, CPU explicit. Inspect installer pins, backups and dependency/weight provenance without running setup in discovery.
- Pass/oracle: job never auto-downloads or sends music to network; invalid identity fails before inference, explicit setup registers exact model/worker/qualified packages and preserves other data. Backend fallback accurately reported; resolved runtime lock/config recorded for reproducibility.
- Evidence/mode/severity: model/worker/package/config hashes, child command/network observation and harness decisions; static + automated/runtime when installed; P1.

### MW-S02 — Stem worker outputs retain source-time sample dimensions
- Contract/source: worker manifest and declared mono/stereo8–192kHz,≤10min,≤512MB decoded limits; deterministic seed1729/7.8s/25% overlap.
- Cases/procedure: normal/quiet/mono/antiphase/silence and invalid oversized/rate/channel/duration input; original decoded DC and offsets, GPU/CPU where qualified; compare all four drums/bass/other/vocals WAVs independently.
- Pass/oracle: four finite float files, exact source decoded rate/channels/frames and sample-zero offset0; summed cross-correlation alignment ≤1 sample, relative recombination RMS≤15% or residual≤1e-7 near silence as predeclared integration guard. No hidden peak normalization/trim/residual injection. Refusal of synthetic outliers documented, not a claim of separation quality.
- Evidence/mode/severity: independent decoder/scoring output, original/stem hashes and manifest, runtime identity; automated; P1.

### MW-S03 — Stem cancellation, publication and reuse cannot attach a partial set
- Contract/source: StemJob one-job ownership, UUID staging and same-volume publication; cache content/decoded/runtime/settings key.
- Cases/procedure: Cancel at decode/inference/validation/publish, close/reopen progress window, source changes during job, low disk/child crash/competing publication, missing/corrupt one of four assets; rename identical source, runtime absent but valid existing set; app quit reaps owned child.
- Pass/oracle: only complete independently validated set becomes Ready/attachable; original mix stays usable on failure, cleanup only that exited invocation's UUID folder; unchanged valid assets reuse without inference. Incomplete crashed folder never Ready; completed cache never evicted as audit cleanup.
- Evidence/mode/severity: child lifecycle/publication stages, manifests/hashes and cache/attachment decisions, retry; automated/integration; P1.

### MW-S04 — Timeline stems share parent clock and intended control scope
- Contract/source: StemMixing/Model, stem rows/Editing owner selector; volume/pan automation only, parent chain processes sum.
- Cases/procedure: four known tagged test stems, level/pan/mute/solo all branches, exclusive same-lane stem solo with unchanged other lanes, independent simultaneous lane selections and continuous effects; parent mute/lane solo/original switch; split/slip/reverse/loop/tempo/trim/move and nodes; save/reopen/export.
- Pass/oracle: independent stem-sum/source-index/gain oracle within2e-6 float; source clocks never reanalysed per stem; solo excludes other timeline sections and original bypass restores intended parent audio. Only declared controls independently process stems, one Undo, neutral new versus preserved existing settings.
- Evidence/mode/severity: route/clock and PCM matrix, current JSON roundtrip and native row/owner actions; automated + manual/listening; P1.

### MW-S05 — External stem references validate without silently replacing audio
- Contract/source: session stem asset identities, cache hydration on new placement, current-format parsing/prepare boundary.
- Cases/procedure: move/delete/change/hash-mismatch assets, source decoded identity changes, saved explicit set differs from cache, fresh add with matching cache and no installed runtime; cancel/failed open then valid source retry.
- Pass/oracle: saved project uses explicit validated set; missing/changed set rejects opening transaction with current mix preserved, original-mode switch explicit. New placements may hydrate matching validated cache only; saved assets are not silently rewritten from newer cache. No hidden external stem folder dependency omitted from package/session guidance.
- Evidence/mode/severity: original/cache/saved manifest and source identity table, actual open/add decisions/PCM; automated/native; P1.

### MW-S06 — Stem quality claims require listening and rights context
- Contract/source: direction/qualification research, stated recombination guard is not leakage/SDR guarantee.
- Cases/procedure: independently licensed real-music partitions, listen drums/bass/other/vocals solo and recombined for leakage, cymbal transients, bass phasing, quiet/mono; obtain true multitrack references only when available/authorized.
- Pass/oracle: no gross unreported artifacts under agreed acceptance rubric REQ-03; recorded listener judgement and limitations. No reference SDR reported without true independently aligned stems, and no redistribution right inferred for separated user music. Threshold/rubric unresolved → BLOCKED.
- Evidence/mode/severity: source rights/reference identity, listening notes/device/level and independently computed metrics where valid; listening/manual; P1 if advertised quality false, otherwise P2.
