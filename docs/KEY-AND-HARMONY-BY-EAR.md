# Key and harmony by ear

These are two separate listening tools. **Key by ear** saves a source key label.
**Harmonise by ear** changes the pitch of one placed segment while retaining its
timing. Neither requires a new automatic key analysis. Original audio is never
rewritten, and listening trials do not modify the arrangement.

## Key by ear

Open **Source beat grid / corrections**, then **Key by ear…** beside the Key field.
Wait for source audio preparation if that button is unavailable. The assistant
uses the original source pitch, independently of any placed segment's transposition.

1. Drag over a musical passage in the waveform, or enter loop start/end seconds.
   Choose bass, melody or vocals rather than a drum-only passage. Previous/Next
   passage moves the loop for another check. Beat detection is not required.
2. Choose **Track**, **Reference** or **Together** and press **Play / Stop**. Set a
   quiet reference level and choose a comfortable Low/Middle/High reference register.
3. Compare **Sound 1 / Sound 2** and choose which feels more settled against the
   music. The guided comparisons cover the twelve home tones. **Try other sounds**
   explores another comparison; **Back / Start again** revisits decisions.
   **Use home sound** keeps the tone being heard and moves directly to chord
   comparison when further home-tone comparisons are unnecessary.
4. Compare two chord sounds for the chosen home tone. This makes the major/minor
   choice audible. **Show musical names** reveals names without making notation
   necessary to operate the tool.
5. Check another passage. **Save source key** saves only the manually chosen key;
   it preserves source BPM/downbeat corrections, segment pitch and arrangement
   timing. Cancel keeps the saved facts unchanged.

An existing key label supplies only a starting home-tone suggestion. A tone fitting
one passage does not prove a song's key. **Unsure** is a valid result; the assistant
clears the unsaved choice and disables Save. Back can restore that choice. The
assistant does not label a listening choice as an automatic confidence score.

## Harmonise by ear

Right-click the segment to adjust and choose **Harmonise by ear…**. Select a
different overlapping segment as the **Reference / stays unchanged**. Repeated
source names are distinguished by lane and timeline start.

1. Select a loop inside their actual overlap. Loop positions use timeline seconds;
   duration is 0.1–30 seconds. **Prepare loop** prepares only that bounded listening
   passage. Both sources must already be available for playback.
2. Use **Reference / Adjusted / Together** to hear either track or their blend at
   the arrangement's timing. **Original / Kept version / Trial** compare pitch
   choices at the same loop position. Original means zero pitch adjustment; Kept
   initially holds the segment's current setting.
3. **Lower / Higher** changes the trial by one musical step without changing tempo.
   The slider covers six steps in either direction. **Fine tuning** adds up to
   fifty cents either way in five-cent increments; one step is 100 cents.
4. **Keep this version** retains a candidate for comparison without applying it.
   Compare with Original and another passage, including vocals. Listening levels
   are matched to the original with bounded gain compensation. A pitch change can
   alter vocal/instrument character even when the blend fits better.
5. **Apply to this segment** prepares its playback/export audio and commits one
   arrangement Undo step. Other occurrences, the reference track, original source
   key, placement, BPM and beat/downbeat mapping stay unchanged. Cancel during
   preparation prevents the commit. Failed or stale preparations keep the mix.

The preview uses independent renderers, including the selected tracks' effects and
the arrangement tempo map. It warms up to two seconds of preceding effect context;
longer effect histories can differ from continuous programme playback. The preview
is a listening aid; the applied pitch setting is rendered through normal playback
and WAV export. Pitched prepared PCM has separate cache identities, including
tempo-ramp geometry and stems. Existing zero-pitch cache identities remain valid.

Session format42 stores the per-segment pitch value. It is separate from source
analysis and key labels; analysis revision11 remains unchanged. Save new sessions
with the new candidate when retaining pitch changes.

## Preview output and ownership

Each tool shows a listening level and output choice. In Arrange, **Master preview**
pauses the arrangement without rewinding it. Headphones can be chosen explicitly.
In Record/Perform, listening requires the configured private route and never falls
back to the audience output. Stop programme playback/capture before changing the
hardware route. Preview level changes do not change arrangement/export gain.

Closing a tool detaches its preview audio and cancels unfinished work. Changing the
loop, pair or pitch invalidates older preparation completions. Apply rejects an
arrangement/source that changed since the comparison opened. A failed source-key
write retains the listening choice for correction or cancellation.

## Verification scope

The supplemental native flag is `--ear-tools-only`; these cases also belong to the
named tempo suite. Run through `verification.run_checks` for isolated stores and
fixture retention. Coverage targets authored frequency/duration oracles, source key
save, pitch/cache identity, normal and ramped tempo, save/reopen/export, route and
callback ownership, native controls, asynchronous completion, Apply and Undo/Redo.
It does not establish physical headphones/ASIO, listening quality, human usability
or full regression qualification. Existing user data and running sessions are not
test fixtures.
