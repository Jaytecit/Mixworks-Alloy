# Licensing and beta distribution

The project repository is [Jaytecit/Mixworks-Alloy](https://github.com/Jaytecit/Mixworks-Alloy).
The [Personal Beta Licence](../LICENSE) reserves the original application's
commercial and redistribution rights and permits free personal, non-commercial
testing. This is proprietary source-available software. Beta describes maturity;
it is not itself a licence or a technical copy-protection mechanism. Copyright
arises automatically for original software under [UK guidance](https://www.gov.uk/copyright).
Licence terms establish permissions; they cannot guarantee prevention of copying.

The repository is currently public. GitHub's platform viewing/forking rights
remain applicable, as explained in [GitHub's licensing guidance](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository).
No public installer, binary release or commercial permission is granted by
merely publishing this source. Existing local builds remain local test artifacts.

## Public beta release prerequisites

| Dependency | Observed local terms and build | Before distributing a restrictive beta |
| --- | --- | --- |
| JUCE 8.0.16-security | Pinned commit `ea77444750de45e036e0279bb0291144bf8e3688`; its LICENSE.md offers AGPLv3 or the JUCE agreement. No evidence of the owner's selected proprietary entitlement has been recorded. | Record the owner's acceptance and eligibility for an appropriate [JUCE licence](https://juce.com/legal/juce-8-licence/). Starter may be suitable subject to its current eligibility terms; do not assume eligibility or accept agreements on the owner's behalf. An AGPL distribution cannot impose this project's non-commercial restrictions on the covered combined work. |
| ASIO | The bundled 2025 headers offer GPLv3 or the Steinberg agreement. The existing prototype notice selects GPLv3. | Obtain and record the signed proprietary Steinberg agreement required by [the pinned SDK terms](../licenses/ASIO-SDK.txt), or prepare and verify a build without those headers/backend. Keep the existing local ASIO functionality until a distribution choice is made. |
| SoundTouch 2.4.0 | Pinned commit `d994965fbbcf0f6ceeed0e72516968130c2912f0`; LGPL-2.1, currently linked statically. | Supply the required notices and exact library source and satisfy LGPL replacement/relinking and debugging rights, including necessary application objects/build instructions for static linking; alternatively obtain a suitable separate licence. [Upstream terms](https://www.surina.net/soundtouch/license.html). Copying the licence alone is insufficient. |
| Beat This!, its C++ port, ONNX Runtime, PocketFFT, r8brain and JUCE's bundled dependencies | Separate licences and attribution; current analysis identities are retained in the dependency manifest/notices. | Retain attribution, licence texts and applicable notices in each actual package; audit the exact compiled modules and distributed assets. The personal beta restrictions do not replace these licences. |

**Status: public binary distribution blocked pending the above evidence.** Do not
upload the existing `out/`, `build/` or `.local/` content to GitHub Releases or
publish a download. Local regression/deployment receipts establish technical
checks, not legal distribution clearance. The source licence applies to the
Licensor's independent original material; it does not relicense incorporated
third-party material. No dependencies or third-party agreements were purchased,
accepted or changed as part of setting up this licence.

Keep the public package's licence visible before download/install and include
LICENSE, this distribution status, THIRD-PARTY-NOTICES.md and the relevant licence
texts beside the executable when a compliant package is prepared. Have the final
terms and dependency clearance reviewed before public distribution. Requests for
commercial permission go through the repository owner. No CLA or permission to
relicense unsolicited contributions is assumed.
