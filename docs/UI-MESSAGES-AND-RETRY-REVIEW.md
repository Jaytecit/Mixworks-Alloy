# Clear messages and focused retry review

App-owned hover help gives a short action or explanation in plain language.
The shared resolver limits it to two sentences, 35 words and 240 characters.
Decimals are kept intact; disabled controls use a specific reason when one is
available, without appending a generic list of possible causes. Plugin editors
keep their own help. File pickers, operating-system and plugin-owned messages
are outside this policy.

Routine success and failure feedback uses the status area. Full failure text is
available through **Tools → Last message details** in a scrollable, copyable view.
The Auto Arrange result also retains its full report there. Confirmations remain
for choices such as Save/Discard/Cancel and edits that move or replace audio.
Those explain the actual consequence and Undo in a few short sentences; they
are not truncated to the hover-help limit.

The source audit reviewed direct help/dialog sites, shared menu-command help,
property-based hints and dynamic reports. Dense instructions were shortened in
the arrangement, editing, automation, timeline, controller, preview, grid,
comparison and export tools. Raw analysis numbers, paths and evidence are now
available explicitly through **Source beat grid / corrections → Analysis
details**, instead of being stacked into the source heading or library help.
The library **Key check** column reads **Unknown**, **Review**, **Detected** or
**Manual**. The ua10 source change labels a valid accepted detector key Detected
regardless of its stability score; Review is reserved for a tentative fallback
or invalid evidence. Detected describes the origin of the result, not human
confirmation or measured accuracy. Manual choices retain their label. Status
colour and sorting follow these categories. The raw evidence remains available
in Analysis details; detector values and automatic harmonic/retry policies are
unchanged. ua13 verifies the build and23 focused classification/sorting checks;
the native table capture was inspected. Musical accuracy remains unmeasured.

## Focused BPM / Key retry

As each track finishes, the progress panel shows its name and result. It states
which fields improved, stayed unchanged, retained manual corrections or still
need review. Read errors and save errors are also reported. The most recent
track result remains available at the top of Source corrections after the
progress panel closes, including after reopening the app.

The **Retry failures** dropdown in Source corrections opens any track that still
needs attention after retry. It includes unresolved BPM/grid or key results,
unreadable files and unsaved results. An exhausted retry is still reviewable:
being ineligible for another automatic attempt does not remove it from this list.
Tracks that have never had a focused retry are not labelled retry failures merely
because their initial analysis is weak.

Select a failure, check the grid or use Key by ear, then save the correction.
Manually corrected fields count as resolved, and the source drops from the list
when all its outstanding issues are resolved. Successfully reading a file clears
its read error; successfully saving clears its save error. Unsaved grid drafts
offer **Save and open**, **Discard and open** or **Stay here** before switching
tracks. Late callbacks cannot switch a newer review window to an old selection.

With no usable selected source, Source corrections opens a failures chooser.
Unreadable sources with known duration can still be reviewed from cached facts;
files without a usable duration must first be found or imported again.

The review list is stored separately in `FocusedRetryReview-v1.json` beside the
analysis store and bound to each source path and audio identity. It does not
change session/analysis versions, retry eligibility, detector calculations,
captured playback clocks or audio preparation. A damaged existing list is kept;
new results remain available for the current run if the list cannot be saved.
The existing detailed retry evidence is also retained in analysis records and
the AnalysisRetries report folder.

No data reset or automatic retiming accompanies this UI change. **Save source
only** saves source facts; **Apply to arrangement** saves and retimes using the
existing grid correction workflow.
