> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# Native UI playtest — 2026-10-17

Tested the deployed v0.20.0 executable through the Windows computer-use skill, with actual library clicks, a library-to-timeline drag, playback, zoom, snapped/free drags and Undo. No source changes or synthetic test run in this session.

## Tracks and arrangement

- Airsand, TuraniQa — **Better Off Alone (Radio Edit)**: library/clip 124.00 BPM, 1B / B major; `C:/Users/jayte/Music/01. Airsand - Better Off Alone (Radio Edit).mp3`.
- Be Like Butter & Enzo Siffredi — **Ode To Omar (Original Mix)**: 127.00 BPM, 1B / B major; `C:/Users/jayte/Music/09. Ode To Omar (Original Mix).mp3`.

Started with the app's empty timeline. Double-clicked the first track, which established 124 BPM. Dragged the second from the library onto lane 2 near the first track's end. Initial preparation completed; the incoming start displayed **265.105 s**, length **356.003 s**, fade-in **18.217 s**. The outgoing fade and footer's 124-to-127 tempo points appeared automatically. This was a manually positioned overlap, not an exact eight-bar append.

## Observed results

- Played from **04:23.933**, before the incoming track, through **04:52.773**, after the handover. Transport advanced and output meters were active. The tempo display showed **124.00**, then **126.45**, then **127.00**. No red output indication was visible in the sampled screenshots; this is not continuous peak monitoring.
- With Snap on, a 29-pixel drag changed the second clip's start from **265.105** to **266.072 s**, approximately two beats. Its beat markers retained the displayed grid phase.
- With Snap off, an 8-pixel drag changed the start to **266.370 s**, between grid points. Neither drag displayed a preparation/busy stage. This is a responsiveness observation, not an instrumented CPU measurement.
- Two clicks on Undo restored **265.105 s**, the original overlap/fade and Snap on.

## Remaining limits and handoff

**Better Off Alone is labelled `beat 1 ?`; Ode To Omar is labelled `4/4 inferred`.** The first track's downbeat remains uncertain. This run does not establish ground-truth downbeat accuracy, musical phrasing, audible quality, or MixMeister equivalence. Audio output was started on the user's device, but the computer-use tool does not provide an audio listening feed.

The mix remains open and **unsaved**, with the native **Save Mixworks session** dialog open. The suggested filename is `Mixworks UI Test 2026-10-17.mixworks` in Documents, but it was not entered or saved. The helper could inspect the dialog but could not activate it: **`failed to activate captured window`**, including after a fresh window binding and one retry. No overwrite, export, or session-file creation was confirmed. The default filename remains `Session.mixworks`.

The helper's text injection did not enter the attempted artist search; a physical `s` key did, leaving the library filtered by `s` and sorted by key. `set_value` on the JUCE search field returned `Cannot set a value for an element that is not settable`. Ctrl+Z did not visibly undo; the toolbar Undo buttons did. These are unresolved UI/tool interoperability observations, not established app defects.
