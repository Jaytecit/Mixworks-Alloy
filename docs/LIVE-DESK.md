> Read/Touch/Latch/Arm discoverability and the role of Editing need reassessment under W02–W04 in [the roadmap](../FINISHING-PLAN.md); existing capture controls are not accepted newcomer usability.

# Live desk and MIDI profiles

Workflow reference; exact current candidate and verification limits are in [HANDOVER](../HANDOVER.md). [Earlier desk notes (archived)](MAINTENANCE-20261103.md) preserve original device captures and version history. Current modes and original-track ownership supersede earlier arming/per-piece descriptions.

## Programme controls and ownership

Controller provides transport, four mixer lanes, paired EQ/cutoff routing, levels/pans, mute/solo and HC1–HC9. Lane-selector labels name each pot's programme target; private selection never retargets programme MIDI. Selected original-track controls can be shared across split pieces; a mixer-lane control affects that lane's audio/instruments, and master affects programme output. Read the owner label rather than assuming every knob acts on the selected waveform.

**Follow timeline** switches the on-screen pots, levels and pans to the track at
the playhead on their assigned lane. The controls display that track's automation,
including transition-preset nodes, and show its name; adjustments use the same
named owner. In an overlap the selected playing track takes priority, otherwise
the most recently starting track is followed. Pausing holds the displayed values;
seeking updates them. With no track at the cursor, the label explicitly returns
to Mixer lane. Master stays master-owned. Turn Follow timeline off for the normal
lane-only desk. The mode is local to this Controller instance, not saved in the mix.

A drag or open value editor holds its displayed owner until it finishes. Removing
that owner disables the held control rather than redirecting it. Merely watching
or switching follow mode does not write automation. Existing Read/Touch/Latch and
performance-capture rules still govern an actual adjustment. MIDI mappings,
mute/solo pads and the explicitly named mixer-lane menu resets keep their lane
targets; Follow timeline changes on-screen parameter controls only.

The cutoff is centre-off: left is low-pass, right high-pass. Precise entry accepts Off/LP/HP and frequencies. Lane-header/desk cutoff remains lane-owned, so a finished selected segment cannot capture another segment's live lane cutoff. Transition Filter presets and original-track Filter automation have separate named owners.

The small right desk pads use Spare/Spare/Undo/Redo then Play/Stop/Record/Snap. Historical physical FLkey messages were measured; no factory CC assignments should be guessed or installed during maintenance. Keys can drive an enabled live VST instrument. Hosting is in-process; active reported-latency inserts are blocked pending compensation.

The on-screen **Pitch wheel** and **Mod wheel** sit to the left of the pots.
The selector below each wheel chooses a mixer lane and its cutoff, pan, bass,
mid or treble. Pitch starts on lane 1 cutoff and returns to neutral after a
mouse drag. Mod starts on lane 2 cutoff and holds its last setting. Both write
through the same mixer controls used by automation. The wheel selectors are
local to the current Controller and the existing MIDI profile is independent
of those selectors.

## Arrange and Record Perform

Arrange supports free editing, save/playback and offline WAV without recording a pass. Library artwork preview uses master gain/mute and pauses the arrangement without moving its cursor.

Record/Perform Play starts/resumes an independent editable take. Supported GUI/MIDI/direct VST parameter movements, Return, mute/solo, tempo, hotcue/source jumps, launches and structural commits are ordered events. Pause records held source time with silence/tails; Stop finalizes. Takes / preview supports audition, seek reconstruction, event editing/removal, discard and take WAV export while retaining the original arrangement.

Everyday take capture makes all lanes/master eligible; detailed Read/Touch/Latch and exclusions are advanced options. Manual changes hold by default, with Return to automation explicit. Those curve controls and an editable performance take have distinct scopes; see [the implemented contract](ARRANGE-RECORD-PERFORM-PLAN.md).

Optional private library/track/grid/transition preparation has independent transport/level/DSP. Configure a separate device or nonoverlapping pair while stopped; audience1–2 cannot overlap it. Missing headphones never fall back to master. Private changes reach programme only through explicit Apply/Launch. Headphones are optional for recording.

Optional live master WAV captures after master processing before monitor mute, excluding private audio. Writer/FIFO failures retain a labelled partial file while programme/take continue. Editable note recording and opaque/nondeterministic plugin reconstruction are not complete; master WAV preserves the audible pass. Long RF64 capacity remains unqualified.

## Named MIDI profiles

Audio / MIDI shows detected/open/recently receiving state and the active in-memory mapping title. Unnamed or Default / no mappings is explicit. Saved-file bytes alone do not establish active routing.

Learn/Cancel is shared by settings/desk; completed Learn updates startup recovery. Named Save/Save as/Open, portable JSON Import/Export and reversible Delete are implemented. A profile validates before replacing routing; invalid/conflicting/oversized files or failed activation preserve working assignments. Opening cancels Learn and drops stale queued controls. Named files change only through explicit save; imported originals are not autosave destinations.

Delete archives the named copy and detaches its identity while retaining active assignments/startup recovery. Clear separately empties active/startup mappings while preserving named copies. Writes retain backups and report partial save outcomes. Profile operations stay outside musical Undo.

Reconnect polling closes disappeared enabled ports and reopens remembered ones, with exact-name fallback where Windows identifiers change. Secondary/DAW inputs stay opt-in; manual disable stays disabled within the run. Disconnect clears held/queued input and locally releases instrument notes. No hardware programming occurs automatically.

## Timing and unresolved device issues

Pause @ and playing HC jumps use Immediate/Next beat/Next bar(default)/Next phrase timing, with8/16/32bar phrase options. Target hotcues move that source clock while programme/other tracks continue; stale/changed targets cancel. [Hotcues](HOTCUES.md) and [launch timing](LAUNCH-TIMING.md) own details. Full HC MIDI/physical pad acceptance remains open.

Audio / MIDI exposes only driver-supported rates/buffers and records callback work/silent lock skips/available xruns. The UI lock-scope repair is delivered; physical crackle-free playback, original FLkey outage/stale startup profile and ASIO4ALL connection remain unresolved. Do not restore old mappings or change drivers on the owner's behalf. [Request register](REQUEST-STATUS-20261024.md) separates fixes from hardware/listening acceptance.
