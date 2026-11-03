# Mixworks commercial clearance assessment

Assessment: 2 November 2026. **Not cleared for public binary distribution or sale.**
This is a technical due-diligence record and preparation for professional advice,
not an infringement opinion, a copyright certificate or legal clearance.

Follow-up decision: Jay chose **free to launch; reconsider fees if sales grow**.
See [no-purchase component decisions and replacement scope](NO-PAID-LICENCES.md).
That audit sets the intended technical route; unresolved clearance items below
remain open.

## Owner instructions and working assumptions

Jay confirms that development is by Jay with AI assistance, with no other human
collaborators; there are no permissions beyond personal use, no binaries shared
outside his own machines, and no intended bundled music. Existing UI images and
fonts need checking. A one-time purchase is the preferred planning assumption;
support, updates, price and the selling legal entity remain undecided. The
business is UK-based and overseas sales are contemplated. Do not assume that
the trading names Mixworks or Jaytecit identify a registered legal entity.

The audit was authorised; product removal, purchases, licence acceptance,
registration, outside correspondence and publication have not been performed.
Preserve the working local application and research evidence.

## Result of each requested step

| Step | Work performed | Remaining boundary |
| --- | --- | --- |
| 1. Reference-software provenance | Traced documented DJ.Studio investigation, installed visual content, copied renderer helpers and compiled AUFTAKT research paths. | No exhaustive similarity analysis or legal assessment of historical access, contracts or derivative expression. |
| 2. Dependencies | Identified current native components, staged files, model identities, visual Python packages and concrete notice/relink gaps. | Entitlements, licence choices, full transitive/native-codec clearance and a compliant rebuilt package. |
| 3. Ownership/originality | Reviewed Git authors/history, source notices, AI art prompts, fonts and asset paths; recorded Jay's account. | Applicable AI account terms and human creative contribution, legal owner and any assignment into a company. No originality guarantee. |
| 4. Other rights | Preliminary brand/patent research and current official guidance; identified brand collisions worth examining. | UK register blocked by security challenge; full mark/design searches and patent claim analysis require further work/professional judgement. |
| 5. Commercial offering | Prepared proposed one-time offer, consumer/privacy/tax/security requirements and a private adviser brief. | Owner decisions, reviewed customer terms, an operating support process and technical release acceptance. |

The actionable [register](CLEARANCE-REGISTER.csv) is authoritative for open
actions. [Components](COMPONENTS.md) explains observed licensing; the
[commercial offer](COMMERCIAL-OFFER.md) sets out recommendations rather than
customer promises. [Sources](SOURCES.md) records dated external research.

## Findings that determine the commercial path

### Synesthesia materials are a substantive dependency

`CMakeLists.txt:105-129` references the sibling compatibility prototype and copies
its assets into the executable bundle. The activated candidate contains
`visualiser-runtime/assets/ssf-template.glsl`: 30,500 bytes, SHA-256
`5a8ccd8b189026d17fb7d03a42f7e34e398a2d8cb30309df3fe0fce5610effb8`.
It exactly matches the prototype file. Its first line identifies a Ghidra-defined
string from the analysed executable. This is direct evidence of an extracted
helper in the bundle, not merely inspiration from a feature. The second helper,
`native-media-stage.glsl`, also requires provenance review.

The scene library reads 89 installed scene configurations (87 release scenes,
New and Welcome); the actual scene files are outside this staged bundle. Nine
scene directories contain non-commercial licence notices: alien_cavern,
fire_tunnel, fractilian_foundation, hexoplanet, meta_experiment_3,
ray_marching_experiment_62, sinusoidal_warp, tunnel_beauty_6 and waterly.
Other scenes contain differing attribution, CC0, MIT or unknown-origin hints.
These are file-level observations, not a complete rights classification of each
scene and its scripts, images and incorporated fragments. No licence hint is
not equivalent to permission; a permissive fragment does not clear a whole scene.

The published [Synesthesia EULA](https://synesthesia.live/resources/synesthesia-eula.pdf)
restricts copying, redistribution and reverse engineering, and distinguishes
scene ownership from limited use of rendered output. We have not identified a
grant for the present extracted helper or commercial alternative host. Which
terms Jay accepted, and the effect of statutory exceptions, need counsel.

**Recommendation:** plan an original visual library and independently specified
runtime helpers for the commercial candidate. Alternatively, obtain explicit
rights covering the precise content, helpers, alternative host and customer
distribution. A consumer Synesthesia purchase or requiring customers to install
it must not be treated as sufficient clearance. Do not erase the existing
prototype. Previous exposure cannot be undone by calling a rewrite clean-room;
counsel should advise on a defensible replacement process.

### DJ.Studio and MixMeister require an honest provenance account

`docs/research/DJSTUDIO-TRANSITION-AUDIT.md` records analysis of a copied installed
DJ.Studio JavaScript file, alongside normal UI observation. The real-music audit
records local database/label inspection and use of its installed decoder for
comparison. These records establish access, not automatic copying into Alloy or
automatic legality. The published [DJ.Studio terms](https://dj.studio/eula.pdf),
articles 8.4-8.6, contain commercial copying and reverse-engineering restrictions.
Identify the historically applicable agreement and assess mandatory exceptions
before asserting that all investigation was permitted. Comparison labels are
not evidence that the production model was trained on them.

For MixMeister Fusion, existing workflow references and the installed application
were identified. No standalone EULA was found by the targeted local filename
search. This is an unresolved contract-provenance item, not evidence that no
agreement applies. Its vendor DLLs, bitmap skins, metronome WAVs and transition
XML are not named in the current staged package inventory. No exhaustive
code/data similarity or image-similarity search has been performed.

Copyright generally distinguishes function from expression; the
[SAS judgment summary](https://curia.europa.eu/jcms/upload/docs/application/pdf/2012-05/cp120053en.pdf)
is relevant background, not blanket permission for these investigations.
Attribution alone does not cure unauthorised copying. Registered designs,
contract restrictions and patents need separate consideration.

### Detector research must be separated from production clearance

`Source/AuftaktFeatures.cpp`, its feature tables and `AuftaktNetwork.cpp` are
compiled into the main application. `RhythmResearch.cpp` exposes an explicit
`--auftakt-model` path for a recovered 605,824-byte model with a pinned digest.
That recovered model is not in the inspected bundle. Normal `NeuralRhythm.cpp`
uses the separately pinned Beat This! ONNX model. Do not conflate these paths or
describe all compiled research code as provenance-cleared.

**Recommendation:** exclude the recovered-model research path and recovered
tables from a commercial build unless counsel establishes a sound basis to
retain them. Preserve them as private research evidence. Review published tonal
profile data in `Source/Analysis.cpp` separately; publication alone is not a
universal licence, and a claim of an independent implementation needs evidence.

## Ownership and other rights

Git has six local commits attributed to two display names using the same account
email; this agrees with Jay's account but does not prove beneficial ownership.
The first commit contains only README; source and the beta licence arrive in
the subsequent licensing commit. No other human contributor is recorded in the
reviewed history. Git cannot establish who supplied material before a commit.

The AI assistant is not a second human co-owner. OpenAI's
[individual-service terms](https://openai.com/policies/eu-terms-of-use/) allocate
output rights between the parties, subject to law, but do not promise unique
output or settle third-party rights. Business/API terms may instead govern some
work. Keep the applicable account/terms and dated prompts, human design choices,
edits and source checkpoints. Copyright protectability of AI-assisted material
and any transfer to a future company require legal review.

Design concept prompts identify built-in image generation. The launcher SVG is
local source; the exact icon is present in the candidate. The provenance of the
root Martooni images and reference screenshots has not been cleared for
marketing. No font file appears in the staged package. The UI requests the
installed Segoe UI font (`AlloyLookAndFeel.cpp:155`); do not copy Windows font
files into an installer without separate rights.

Brand searches found an existing Mixworks software reseller and iZotope's Alloy
audio product. They are leads, not conclusions about registered rights or
confusion. The UK IPO lookup returned HTTP 403 through the web reader and a
security check in the browser; no register-results search was completed and no
challenge was solved. Request exact, similar and phonetic marks/logo searches
in relevant classes/territories before paying for branding or registration.

An old MixMeister-associated patent, US6448484B1, was located; the secondary
patent index labels it expired. No official legal-status/claim/family review was
completed. Searching competitor names does not establish freedom to operate;
other owners may have relevant claims. See sources and the private adviser brief.

## Evidence and limits

Evidence root: `.local/commercial-audit-20261102/` (excluded from Git). It retains
before-copies of the relevant records, hashes, source review, runtime metadata
and the scene-rights inventory. Existing full source checkpoint
`.local/ua20/candidate-source.zip` remains canonical; no duplicate full runtime,
music, model or executable copy was created by this audit.

The exact activated bundle is `out/releases/0.55.66-708a26477c5d/`, executable
SHA-256 `708a26477c5d31e65f6161cf9f7d17716c241a0fc85e3fa2443e53de262cd409`.
All 35 activation-receipt payload files match. The directory has 36 files when
the receipt itself is counted. All 232 CMake/native build inputs checked (including
the .inc and .cmake files) match the existing candidate source manifest. The
separate Beat This! source licence is not in that source manifest; it matches
the retained/distributed port notice exactly. This does not authenticate upstream
authorship or reproduce the build.

The connected GitHub repository was confirmed public; its Releases endpoint
returned an empty list. This supports, but cannot independently prove, Jay's
statement that no binaries have been shared elsewhere. Existing public source
publication must be included in counsel's assessment. Do not rewrite history or
publish research assets as a supposed remedy.

Only targeted source/metadata inspection was performed. No full similarity
scanner, patent clearance, vulnerability audit, runtime network capture,
complete SBOM, installation qualification, build, native regression, app launch,
listening test, purchase, contact with vendors, or distribution occurred.
Current technical acceptance remains as recorded in the handover; it is not
upgraded by this audit. A register item closes only with its stated evidence,
not because a recommendation has been written.

## Recommended order from here

1. Obtain a focused software/IP solicitor review of access history, Synesthesia
   helpers/content and recovered detector research, using the prepared private
   brief. Jay chooses and instructs the adviser; the agent cannot certify rights.
2. Select the legal owner before entering vendor agreements; obtain qualified
   accounting advice on sole trader versus company and overseas tax exposure.
3. Implement the agreed commercial boundary: original/cleared visuals and helpers,
   isolated research, qualified native and optional runtime components. Preserve
   the current local experience independently.
4. Complete the exact-package notices/source/relink materials and brand clearance.
5. Finalise the offer and reviewed customer terms, then perform an explicitly
   authorised technical release checkpoint on the final compliant package.

No immediate technical specification is needed from Jay. The pending owner
decisions and professional tasks are identified individually in the register.
