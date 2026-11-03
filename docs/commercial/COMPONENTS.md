# Component and package findings

Assessment: 2 November 2026. These are observed terms and outstanding actions,
not a complete SPDX SBOM or a declaration that every component is cleared.
Pinned identities come from the local build, dependency manifest and notices;
current upstream guidance is linked in [sources](SOURCES.md).

| Component | Observed use/terms | Required commercial-release action |
| --- | --- | --- |
| JUCE 8.0.16-security | Pin `ea77444750de45e036e0279bb0291144bf8e3688`; AGPLv3/commercial dual terms. Local MP3-length patch retained. | Owner must establish the applicable entitlement, eligibility and seats, including development history and patched framework. Include applicable bundled-component notices. Proprietary sale is possible under a qualifying commercial route. |
| Steinberg ASIO | 2025 bundled headers: proprietary Steinberg/GPLv3 alternative; `JUCE_ASIO=1`. Existing prototype notice selects GPLv3. | Obtain the applicable signed/countersigned proprietary agreement and satisfy conditions, or build/verify an alternative without this SDK/backend. Official guidance describes the proprietary route as royalty-free; this is not permission already granted to Jay. |
| SoundTouch 2.4.0 | Pin `d994965fbbcf0f6ceeed0e72516968130c2912f0`; LGPL-2.1; statically linked in current candidate. | Choose a compliant source/relink route, suitable replaceable shared-library packaging, or a separate commercial licence. Verify replacement/debugging rights and exact corresponding source/build materials. Current bundle has licence text but no relink kit; copying a licence is insufficient. |
| Beat This! final0 model | ONNX SHA-256 `c5c1466e08abdb03fdeb50668a06f244b787d564c212490482231a9cfbe9ccbd`; upstream expressly identifies code and published weights as MIT. | Retain model provenance, author notices and exact export chain. Upstream cautions about copyrighted/limited-licence training files; MIT model terms do not adjudicate that residual issue. |
| Beat This! C++ port | Pin `07ab790a9ec2eda8093d52d249e3ec4f0510ee72`; MIT. Two ported source implementations exist under `Source/ThirdParty/BeatThis/`. | Preserve licence/attribution and identify modifications/export provenance. This is authorised third-party reuse in principle, not uniquely authored Mixworks code. |
| ONNX Runtime 1.24.4 | MIT; DLL SHA-256 `b95efb2113b603bbbf3f191061c5516a871ed546893c820e4f3b7b6c358dbf2a`. | Preserve Microsoft and transitive notices; assess exact package advisories during release/security qualification. |
| PocketFFT / r8brain | BSD-3-Clause / MIT, exact pins in `scripts/analysis-dependencies.json`. | Preserve relevant notices in the actual package. |
| VST3 SDK in pinned JUCE | Local SDK `LICENSE.txt` is MIT, copyright Steinberg 2025. Hosting enabled; VST2 disabled. | Include exact SDK notice and review VST name/logo guidelines separately. Do not import historical GPL/proprietary VST3 assumptions into this pinned SDK. User plug-ins remain separately licensed. |
| Other JUCE internals | JUCE lists FLAC, Ogg Vorbis, JPEG, PNG/zlib, HarfBuzz, SheenBidi and other optional modules/codecs. | Determine compiled configuration and collect required exact notices. A generic JUCE entitlement does not replace these third-party obligations. Do not assume every SDK in the source tree is compiled. |
| AUFTAKT research | Reconstructed frontend/tables and evaluator compiled; optional recovered-model CLI. Recovered model absent from bundle. | Separate from production candidate unless provenance/rights review supports retention. No new recovered-model execution in this audit. |
| Tonal profile data | Published Sha'ath/Faraldo profiles used in `Source/Analysis.cpp`; repository says no libKeyFinder/Essentia code/binaries incorporated. | Verify exact data origins/rights and preserve attribution. Do not treat a paper or documentation link as a blanket licence to a software library. |
| Synesthesia helper and scene content | Extracted helper is in bundle; original installed scenes are loaded externally. Mixed/unknown per-file content terms. | See the main assessment: explicit permission or an independently cleared replacement path is required before commercial clearance. |
| Local renderer Python packages | 11 installed distributions, including pip; 10 declared requirements. Metadata inventory retained locally. | Decide what is actually distributed; freeze minimal runtime and preserve native/transitive licences, exact sources and replacement rights where required. Merely using an external local venv is not a shippable runtime plan. |
| PyAV / FFmpeg / OpenCV | PyAV metadata reports BSD-3-Clause, but FFmpeg libavutil reports LGPL version 3 or later. Its configuration includes x264/x265 flags. OpenCV wheel also carries third-party terms. | Resolve native-library/codec build terms, source and patent questions before distributing. The wrapper licence or one library's self-report does not clear all enabled codecs. No video export capability/permission is implied. |
| Demucs / HTDemucs weights | Optional local setup pins Demucs 4.1.0 commit and model revision. Upstream code MIT; separate weight redistribution provenance remains unresolved in current notes. | Verify the exact pinned model grant and Torch/CUDA/Python/libsndfile/lameenc and transitive terms. No stem runtime/model is in this native bundle. A download-on-demand installer still needs review. |
| Fonts / logo / artwork | UI selects installed Segoe UI; no font file in bundle. Launcher icon is included; concept prompts identify AI generation. | Preserve original art provenance; assess logo similarity and applicable generation terms. Do not redistribute system font files by assumption. No music intended for inclusion. |

## JUCE decision preparation

The current [JUCE EULA](https://juce.com/legal/juce-8-licence/) lists Starter up to
USD20,000 and Indie up to USD300,000 annual revenue/funding. Its revenue basis
differs for individuals and legal entities; company/affiliate income may count
beyond this product. Jay's legal owner, actual eligibility and seat requirements
must be established before choosing. Do not assume a paid tier is necessary,
that a personal-use description is a licence tier, or that a later purchase
automatically resolves earlier use. No agreement was accepted during the audit.

## Exact staged-package gaps

The 36-file inventory includes the receipt, executable, ONNX model/runtime,
licence texts, selected docs, icon and renderer source/helpers. There is no
installer, complete visual Python runtime, SoundTouch relink/source kit, recovered
AUFTAKT model, bundled scene collection, music file or font binary in that list.
The installed scenes and local venv are external runtime dependencies. There is
no complete native/Python/codec notice set for a self-contained distribution.
Preserve this staged preview; prepare a separately qualified commercial package.

The exact metadata inventory is `.local/commercial-audit-20261102/visual-runtime-packages.json`:
av16.1.0, cffi2.1.1, dukpy0.5.0, glfw2.10.2, numpy2.3.5,
opencv-python-headless4.13.0.92, pillow12.3.0, pip25.0.1, pycparser3.11,
PyOpenGL3.1.10 and sounddevice0.5.6. These names/metadata are not a complete native
library SBOM. Python itself, Duktape, GLFW, PortAudio, libffi and bundled numerical,
image and codec libraries require inclusion where actually redistributed.

## No-purchase launch decision

See [the follow-up component decisions](NO-PAID-LICENCES.md): retain eligible
JUCE Starter and free proprietary ASIO, use LGPL-compliant SoundTouch packaging,
replace extracted visual content, exclude recovered research, and separate unused
video dependencies. No proprietary framework replacement is planned at launch.
The follow-up retrieved the exact HTDemucs model card/API: neither contains an
explicit licence grant. All seven inspected FFmpeg DLLs report LGPL3+, extending
the earlier libavutil-only evidence; native provenance/codec rights remain open.
