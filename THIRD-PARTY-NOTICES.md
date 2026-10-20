# Third-party components

The [Mixworks Personal Beta Licence](LICENSE) applies only to the Licensor's original material. Third-party rights remain governed by their own terms. **The current local executable is not cleared for public binary distribution.** See [distribution prerequisites](docs/LICENSING.md).

- **JUCE 8.0.16-security** — Raw Material Software Limited, pinned commit `ea77444750de45e036e0279bb0291144bf8e3688`. The pinned source's LICENSE.md offers AGPLv3 or the [JUCE agreement](https://juce.com/legal/juce-8-licence/). A restrictive beta needs an appropriate proprietary entitlement; none has been verified here. JUCE bundles further codecs/SDKs with their own notices, which must accompany any eventual package as applicable.
- **SoundTouch 2.4.0** — Olli Parviainen and contributors, pinned commit `d994965fbbcf0f6ceeed0e72516968130c2912f0`, LGPL-2.1. The current build links it statically. Public distribution must satisfy the [upstream terms](https://www.surina.net/soundtouch/license.html), including replacement/relinking rights and source/object provision as applicable, or use a separately obtained licence. The complete LGPL text is in `licenses/SoundTouch-LGPL-2.1.txt`.

## Local analysis components

Mixworks performs analysis locally. No music is uploaded for detection.

- **Beat This! final0 model** — Francesco Foscarin, Jan Schlüter and Gerhard Widmer, ISMIR 2024. [Original project](https://github.com/CPJKU/beat_this), MIT licensed code and weights. The pinned ONNX export comes from [Masaki Ono's C++ port](https://github.com/mosynthkey/beat_this_cpp), commit `07ab790a9ec2eda8093d52d249e3ec4f0510ee72`. Its SHA-256 is `c5c1466e08abdb03fdeb50668a06f244b787d564c212490482231a9cfbe9ccbd`. Mixworks incorporates the port's Mel transform and minimal postprocessor with attribution; model inference and grid admission are owned by Mixworks.
- **ONNX Runtime 1.24.4** — Microsoft, MIT licensed. The CPU runtime is bundled beside the executable. The download and executable identities are pinned in `scripts/analysis-dependencies.json`.
- **PocketFFT** — Max-Planck-Society and contributors, BSD 3-Clause. Pinned commit `0fa0ef591e38c2758e3184c6c23e497b9f732ffa`.
- **r8brain-free-src** — Aleksey Vaneev, MIT licensed. Pinned commit `e71c31bf320f84210bb4bdcb57e296c39ce940f9`.

Full license texts and ONNX Runtime's additional notices are in `licenses/`, included in each release. These components supplement the existing JUCE and SoundTouch dependencies. Proprietary DJ.Studio and MixMeister code, models and analysis labels are not distributed or used as detection inputs.

The tonal estimator uses published Sha'ath and Faraldo pitch-class profile data. Its implementation uses JUCE spectral analysis, register normalization and independent stereo evidence; it does not incorporate libKeyFinder or Essentia code or binaries. See the [Sha'ath thesis](https://ibrahimshaath.co.uk/keyfinder/KeyFinder.pdf) and [Faraldo profile documentation and references](https://essentia.upf.edu/reference/std_Key.html).

Windows audio also includes JUCE's bundled Steinberg ASIO SDK headers (2025). Their dual Steinberg ASIO / GPLv3 licence is reproduced in `licenses/ASIO-SDK.txt`. This local prototype uses the GPLv3 option; publishing a proprietary build requires the applicable Steinberg agreement. ASIO is a trademark of Steinberg Media Technologies GmbH. No ASIO driver is distributed.
