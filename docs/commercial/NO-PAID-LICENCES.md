# Launch without purchased third-party licences

Audit: 2 November 2026. **Feasible direction, not a cleared release.**

Jay chose **free to launch; reconsider fees if sales grow**. Retain a proprietary,
one-time-purchase product as the working assumption. Free commercial agreements,
attribution and proportionate open-source compliance are acceptable routes; no
agreement has been accepted on Jay's behalf. This document supersedes undecided
cost choices in the initial audit, not its outstanding rights findings.

## Recommended component decisions

| Component / capability | Decision for the first commercial build | Expected product impact and remaining condition |
| --- | --- | --- |
| JUCE: interface, audio, plug-in hosting and infrastructure | **Keep on free Starter if eligible.** | No framework rewrite. Starter is free up to USD20,000 applicable revenue/funding over the previous 12 months. For individuals/sole traders the basis is Framework-related revenue/funding; companies include entity/affiliate revenue/funding. Confirm legal owner, tier and applicable seats/history; monitor eligibility as the business grows. [JUCE agreement](https://juce.com/legal/juce-8-licence/) |
| ASIO device support | **Keep using the free proprietary agreement.** | No intended loss of professional audio-device support. Steinberg says no standard licence fees or royalties; Jay must sign and obtain the required countersignature and comply with the agreement. Payment is not the obstacle. [Official FAQ](https://forums.steinberg.net/t/asio-sdk-proprietary-licensing-faq/1044024) |
| SoundTouch time stretch / pitch | **Keep; fix LGPL distribution.** | Prefer a replaceable DLL with exact source/build materials, notices and the required modification/debugging rights. Preserve the DSP version, float configuration and settings. No intended change in sound, but compare rendered output and timing before release. A separately purchased licence is optional, not required for commercial use under LGPL. [SoundTouch terms](https://www.surina.net/soundtouch/license.html), [LGPL 2.1](https://opensource.org/license/lgpl-2-1) |
| VST3 hosting | **Keep the pinned MIT SDK.** | No SDK fee identified. Preserve notices and follow trademark rules; do not bundle customers' commercial plug-ins. VST2 is disabled. [Steinberg SDK](https://www.steinberg.net/developers/vstsdk/) |
| Beat This! detector / model, C++ port, ONNX Runtime, PocketFFT, r8brain | **Keep the pinned MIT/BSD components.** | No intended detection or sound-quality loss. Preserve notices, model/export provenance and modifications. Complete transitive notices; the existing training-data caveat remains. See [component evidence](COMPONENTS.md) and [sources](SOURCES.md). |
| Extracted Synesthesia SSF template and unresolved media shader | **Replace before commercial distribution.** | Implement an original visual helper interface and effects from independently documented requirements. Renaming or translating extracted code is not an adequate replacement. Current staged template is an exact match to the extracted 30,500-byte file. |
| Synesthesia's installed scenes, scripts, textures and presets | **Replace with an original or individually cleared visual collection.** | Preserve music-reactive visuals, scene arrangement, controls, transitions and fullscreen output as goals. The same specific Synesthesia scenes cannot be promised. Nine inspected directories contain non-commercial notices; other directories contain mixed/unknown grants. A permissive fragment does not clear a whole scene. [Synesthesia EULA](https://synesthesia.live/resources/synesthesia-eula.pdf) |
| Recovered AUFTAKT research code, constants and model path | **Exclude from the commercial target; retain private evidence.** | No intended effect on ordinary Beat This! detection. Exclusion must cover CLI dispatch, feature/network implementations, filterbank tables, recovered decoder branches and test registration. Removing just the model file is insufficient; it is already absent from the bundle. |
| PyAV / FFmpeg video dependency in visual host | **Remove the dependency from the initial still-image/procedural renderer, after separating imports.** | Current Alloy visual lane explicitly rejects video, so this should preserve its supported user workflow. Merely deleting PyAV breaks imports: renderer → media → video_decoder → av. Future video support needs a qualified codec build and separate rights assessment. |
| Python, NumPy, Pillow, GLFW, PyOpenGL, Duktape/dukpy and other visual libraries | **Keep only required, cleared runtime components.** | Their observed top-level terms offer free commercial routes. Package exact native/transitive notices too. OpenCV and pip are candidates for exclusion from the deliverable, not deletion from the development environment. Remove sounddevice only after separating the PCM analyser from its capture-module import. |
| Optional Demucs stems | **Retain as a conditional capability; resolve model permission first.** | MIT code is established; the exact pinned HTDemucs model card/API retrieved here contain no explicit licence field/grant. This is missing evidence, not proof that payment is required. Obtain a grant covering these weights or select clearly licensed equivalent weights and compare separation quality. If unresolved at launch, exclude the stem installer/model integration from the commercial offer. |
| Torch/CUDA and native stem dependencies | **Qualify the exact runtime; do not assume a paid SDK is necessary.** | Audit redistribution terms and native notices. CPU-only fallback can reduce GPU packaging dependencies but may be much slower, so is not an equivalent replacement by default. Review unused encoder dependencies separately from WAV stem output. |
| Published tonal profiles in key detection | **Resolve data provenance; retain if permitted.** | No evidence here that a paid licence is necessary. If the exact numerical data cannot be cleared, derive replacements from appropriately licensed data and measure key accuracy. Do not substitute untested profiles or assume a library's GPL covers independently published research values. |
| UI font, logo and icons | **Use installed Segoe UI; retain demonstrably original artwork.** | No font binaries are currently bundled. Using a Windows-installed font does not require buying a font redistribution licence; packaging the font itself is a different matter. Review logo provenance/similarity. [Microsoft font FAQ](https://learn.microsoft.com/en-us/typography/fonts/font-faq) |
| Music and demo content | **Ship without third-party music.** | Matches Jay's preference. Public demonstrations need separately cleared recordings/compositions; possession of a purchased track is not evidence of promotional rights. |

## What actually needs removal or replacement

There is no confirmed core component that must be purchased merely to launch
under the chosen approach, **provided JUCE eligibility is established**. Most
licensing work concerns permissions and packaging, not buying substitutes.

The commercial package must exclude the extracted/unresolved visual material and
unlicensed scene collection unless an appropriate grant is secured. Our chosen
route is replacement. Exclude recovered research from that target and review its
remaining provenance implications; this does not erase development history.
Exclude optional model/video functionality if its rights remain unresolved at
release. Preserve the working private preview and all reference evidence.

GPL/AGPL software can be used commercially and sold. Its obligations can conflict
with the intended proprietary terms. We have not chosen to relicense the whole
application; buying a commercial alternative is not the only way to comply.

## Replacement feasibility and verification

1. **SoundTouch packaging — contained change.** Its CMake already supports shared
   builds and a Windows DLL wrapper. Select an appropriate target/ABI rather than
   globally toggling every dependency to shared. Supply the exact library source,
   modifications and build instructions; verify a compatible rebuilt DLL can be
   substituted. Check playback, rendered audio, stretch duration and project
   reopening. Dynamic linking alone does not discharge all LGPL obligations.
2. **Research exclusion — bounded but cross-cutting.** Production neural analysis
   references the Beat This! frontend/postprocessor. Recovered paths are reached
   through research CLI/tests; `RhythmDecoder` also contains recovered branches.
   Trace shared types and registration before exclusion, then verify ordinary
   detection against the retained candidate and inspect final package contents.
3. **Original visual runtime and scene pack — substantial creative work.** Specify
   our own audio signals, controls and scene format; remove dependence on the
   installed Synesthesia directory. Use original shader functions and art or
   individually verified permissive sources. Prior exposure means this must not
   be described as a proven clean-room rewrite. Compare responsiveness, transitions,
   readability, still-image handling, fullscreen output and performance using
   our own scenes. Jay's judgement is needed on visual quality and variety.
4. **Minimal visual distribution — moderate work.** Separate still-image support
   from video imports and externally supplied PCM analysis from device capture.
   Produce a self-contained runtime that starts on a clean Windows test machine
   without Synesthesia, the prototype tree or a development venv. Remove unused
   codecs and development tools from the package. Verify all supported workflows.
5. **Stem and profile evidence — uncertain scope.** Check exact grants before
   changing algorithms. If replacement is necessary, comparative accuracy,
   listening and performance checks determine whether it preserves the product.

These are engineering estimates from source inspection, not completed migrations
or quality guarantees. The visual replacements offer equivalent categories of
capability, not identical copies of another product's artistic output.

## Why a JUCE replacement is not recommended now

Local inspection found JUCE references in 221 C++/header/include files. It is much
more than a window skin: components span drawing, audio buffers/devices, DSP,
VST3, files, state and infrastructure. Replacing it would be a major port with
audio, interface, accessibility and stability risks.

Free alternatives exist for parts of the job: [wxWidgets](https://wxwidgets.org/about/licence/)
permits proprietary binaries through its licence exception, and
[miniaudio](https://miniaud.io/) offers permissive audio I/O. Neither replaces all
JUCE capabilities; ASIO and VST3 hosting would still need integration. These are
contingency candidates, not an approved replacement stack or proof of parity.
Jay's decision to reconsider fees after growth makes a launch-time rewrite
unnecessary, conditional on Starter eligibility.

## Evidence, limitations and owner input

This follow-up inspected CMake, research call sites, visual import paths, WAV
export, stem setup/worker, framework usage and upstream terms. Seven local FFmpeg
libraries each report LGPL v3 or later. The earlier configuration still contains
x264/x265 enable flags: the full binary/dependency provenance is unresolved;
self-reported library licences alone neither clear the wheel nor prove it GPL.
[FFmpeg documentation](https://www.ffmpeg.org/general.html) explains its optional
GPL-library conditions. Avoid this unnecessary launch dependency instead of
assuming a paid codec licence is required.

Private evidence and the retrieved pinned model card/API are retained in
`.local/commercial-audit-20261102/no-paid-licences/`. No new model download,
build, native regression, app restart, deletion, purchase, vendor contact or
licence acceptance was performed. This is not an exhaustive SBOM or patent,
copyright, contract or trademark clearance. Those existing register items remain
open; zero licence-purchase cost does not mean zero business/legal expense.

**Jay's necessary inputs:** identify the legal seller and confirm relevant JUCE
eligibility facts; review/sign the free ASIO agreement; judge the eventual original
visual pack; decide whether unresolved optional stems/video should delay launch.
The agent can prepare code/packaging changes, provenance records, licence bundles
and validation. Only the owner can provide private entitlement facts and execute
agreements; legal opinions and rights-holder grants cannot be manufactured by an
automated audit. No immediate technical specification is needed from Jay.

Recommended order: establish JUCE eligibility; isolate commercial build contents;
implement compliant SoundTouch packaging; create the original visual runtime and
scene pack with minimal dependencies; finish optional-model and exact-package
clearance. A later release candidate needs the project's full release checks.
