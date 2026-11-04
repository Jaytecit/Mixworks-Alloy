#include "Engine.h"
#include "PromoControl.h"
#include "RhythmDecoder.h"
#include "Recovery.h"
#include "StemJobs.h"
#include <cstdlib>
#include <cmath>
#include <juce_cryptography/juce_cryptography.h>

namespace mixworks
{
void runMilkDropTests(const std::function<void(bool,const juce::String&)>&,const juce::File&,const juce::File&);
void runOriginalPlaybackTests(const std::function<void(bool,const juce::String&)>&,const juce::File&);
void runPerformanceTakeTests(const std::function<void(bool,const juce::String&)>&,const juce::File&);
void runPerformanceTakeParserTests(const std::function<void(bool,const juce::String&)>&);
void runTargetedAnalysisTests(const std::function<void(bool,const juce::String&)>&,const juce::File&);
void runImportScanTests(const std::function<void(bool,const juce::String&)>&, const juce::File&, const juce::StringArray&);
void runSectionFlagTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runHotcueTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runSourceSectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runLongNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
void runLongSessionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
void runLongOpenTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
void runSourceStreamingTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
void runLatencyTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
void runExportTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
void runMasteringTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runGridCorrectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runGridAlignmentTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runEarToolTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runVisualArrangementTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runOriginalVisualTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runUiClarityTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAnalysisRefreshTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runFocusedRealProbe(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
void runNeuralFailureTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTempoBrowserTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTrackOwnershipTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
void runNativePickerTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runWindowTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAlloyTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAudioUiTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runEditSelectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runEditingWorkflowTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAutoArrangementTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runLibraryPreviewTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runZoomHandleTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionSwapTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAutoArrangementCatalogueProbe(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&, const juce::String&, int, int);
void runTimelineEditingSuite(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runPresetNodeTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runLiveHotcueTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runPlacementTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runBeatGridTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
void runRhythmReliabilityTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runConstantFourFourTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runRealTrackTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
void runMusicTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTempoTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAnalysisCacheTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runMusicalEnergyTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTempoUiTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runFusionWorkflowTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runPlaybackCacheTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runCacheSettingsTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runLibraryAssetTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&, const juce::File&);
void runEditingTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAutomationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runLiveAutomationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
void runUnlinkTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runIndependentTimelineTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runFreshAnalysisProbe(const std::function<void(bool,const juce::String&)>&,const juce::File&,const juce::StringArray&);
int runSelfTests(const juce::String& commandLine)
{
    auto args = juce::StringArray::fromTokens(commandLine, true); args.removeEmptyStrings();
    juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile(".local/test-results.txt");
    auto outputIndex = args.indexOf("--report"); if (outputIndex >= 0 && outputIndex + 1 < args.size()) output = juce::File(args[outputIndex + 1].unquoted());
    output.getParentDirectory().createDirectory(); juce::String report; int failures = 0, tests = 0;
    // A retired focused selector must never fall through to the broad suite.
    if (args.contains("--visual-scenes-only") || args.contains("--visual-coverage-only"))
    {
        output.replaceWithText("Unsupported retired visual test selector. Use --original-visual-only or --visual-arrangement-only.\n");
        return 2;
    }
    const int rootIndex = args.indexOf("--playback-cache-root");
    const auto testRoot = rootIndex >= 0 && rootIndex + 1 < args.size() && juce::File::isAbsolutePath(args[rootIndex + 1].unquoted())
        ? juce::File(args[rootIndex + 1].unquoted()) : output.getParentDirectory().getChildFile("playback-test-cache-" + juce::Uuid().toString());
    Engine::useTestMidiProfile(output.getParentDirectory().getChildFile("midi-test-profile-" + juce::Uuid().toString() + ".json"));
    PcmStore::shared(testRoot);
    analysisStoreRoot(testRoot.getSiblingFile(testRoot.getFileName() + "-analysis"));
    auto check = [&](bool condition, const juce::String& name) { ++tests; if (!condition) ++failures; report += juce::String(condition ? "PASS " : "FAIL ") + name + "\n"; output.replaceWithText(report); };
    auto near = [](double a, double b, double tolerance = 0.0001) { return std::abs(a - b) < tolerance; };
    if (args.contains("--recovery-only"))
    {
        runRecoveryTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--fresh-analysis-only"))
    {
        runFreshAnalysisProbe(check,output,args);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures==0 ? 0 : 1;
    }
    if (args.contains("--performance-take-ui-only") || args.contains("--private-popup-only") || args.contains("--export-target-ui-only") || args.contains("--save-export-workflow-only") || args.contains("--destination-safety-only") || args.contains("--mode-seek-ui-only") || args.contains("--take-seek-lifecycle-only"))
    {
        runPerformanceTakeTests(check,output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures==0 ? 0 : 1;
    }
    if (args.contains("--performance-take-parser-only"))
    {
        runPerformanceTakeParserTests(check);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures==0 ? 0 : 1;
    }
    if (args.contains("--performance-take-only") || args.contains("--targeted-analysis-only"))
    {
        if (args.contains("--performance-take-only")) runPerformanceTakeTests(check,output); else runTargetedAnalysisTests(check,output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures==0 ? 0 : 1;
    }
    if (args.contains("--import-scan-only"))
    {
        runImportScanTests(check,output,args);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--grid-attack-only") || args.contains("--waveform-alignment-only"))
    {
        runBeatGridTests(check, output, {});
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--cache-settings-only"))
    {
        runCacheSettingsTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--independent-timeline-only"))
    {
        runIndependentTimelineTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--unlink-only"))
    {
        runUnlinkTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--editing-workflow-only"))
    {
        runEditingWorkflowTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--edit-selection-only"))
    {
        runEditSelectionTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--live-transition-only") || args.contains("--alignment-guide-only") || args.contains("--lane-automation-only"))
    {
        runTransitionNavigationTests(check,output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--library-preview-only") || args.contains("--zoom-handles-only") || args.contains("--transition-swaps-only"))
    {
        if (args.contains("--library-preview-only")) runLibraryPreviewTests(check,output);
        else if (args.contains("--zoom-handles-only")) runZoomHandleTests(check,output);
        else runTransitionSwapTests(check,output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--audio-ui-only"))
    {
        runAudioUiTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--markers-only"))
    {
        runSectionFlagTests(check, output); runHotcueTests(check, output); runSourceSectionTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--stems-only") || args.contains("--lane-stem-solo-only"))
    {
        runStemTests(check, output); runStemIntegration(check, output, args);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--live-hotcues-only"))
    {
        runLiveHotcueTests(check,output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--promo-control-only"))
    {
        mixworks::runPromoControlTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--recovery-lease-probe") || args.contains("--recovery-crash-fixture"))
    {
        const auto at = args.indexOf("--recovery-folder"); juce::String error;
        const bool valid = at >= 0 && at + 1 < args.size() && juce::File::isAbsolutePath(args[at + 1].unquoted());
        if (valid && args.contains("--recovery-crash-fixture"))
        {
            auto owner = RecoveryStore::create(juce::File(args[at + 1].unquoted()), error);
            const bool written = owner && owner->write(juce::JSON::toString(projectToJson(Project())), {}).wasOk();
            output.replaceWithText(written ? owner->directory().getFullPathName() : error);
            std::_Exit(written ? 0 : 1); // Deliberate test crash: OS must release the live lease.
        }
        auto lease = valid ? RecoveryStore::claim(juce::File(args[at + 1].unquoted()), error) : nullptr;
        check(valid && (lease != nullptr) == args.contains("--expect-available"), "independent process observes recovery ownership");
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--musical-energy-only"))
    {
        runMusicalEnergyTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--auto-arrange-only") || args.contains("--auto-arrange-catalogue"))
    {
        const int catalogue = args.indexOf("--auto-arrange-catalogue"), start = args.indexOf("--start-id"), count = args.indexOf("--probe-tracks");
        if (catalogue >= 0 && catalogue + 1 < args.size())
            runAutoArrangementCatalogueProbe(check, output, juce::File(args[catalogue + 1].unquoted()), start >= 0 && start + 1 < args.size() ? args[start + 1].unquoted() : juce::String(), args.contains("--energy-sample") ? 12 : 0,
                count >= 0 && count + 1 < args.size() ? juce::jlimit(1, 1000, args[count + 1].getIntValue()) : 0);
        else runAutoArrangementTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--native-picker-only"))
    {
        runNativePickerTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--long-navigation-only"))
    {
        runLongNavigationTests(check, output, args);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--long-session-only") || args.contains("--long-open-only"))
    {
        if (args.contains("--long-open-only")) runLongOpenTests(check, output, args); else runLongSessionTests(check, output, args);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--original-playback-only") || args.contains("--transition-boundary-only"))
    {
        runOriginalPlaybackTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--source-streaming-only"))
    {
        const auto at = args.indexOf("--capacity-fixture");
        const auto fixture = at >= 0 && at + 1 < args.size() && juce::File::isAbsolutePath(args[at + 1].unquoted()) ? juce::File(args[at + 1].unquoted()) : juce::File();
        runSourceStreamingTests(check, output, fixture);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--latency-only"))
    {
        const auto at = args.indexOf("--latency-plugin");
        const auto plugin = at >= 0 && at + 1 < args.size() && juce::File::isAbsolutePath(args[at + 1].unquoted()) ? juce::File(args[at + 1].unquoted()) : juce::File();
        runLatencyTests(check, output, plugin);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--export-only"))
    {
        const auto at = args.indexOf("--delay-plugin");
        const auto plugin = at >= 0 && at + 1 < args.size() && juce::File::isAbsolutePath(args[at + 1].unquoted()) ? juce::File(args[at + 1].unquoted()) : juce::File();
        runExportTests(check, output, plugin);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--mastering-only"))
    {
        runMasteringTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--rhythm-decoder-only"))
    {
        runRhythmDecoderTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--visual-arrangement-only"))
    {
        runVisualArrangementTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--milkdrop-only"))
    {
        const int presetIndex=args.indexOf("--milkdrop-preset");
        const auto preset=presetIndex>=0 && presetIndex+1<args.size() ? juce::File(args[presetIndex+1].unquoted()) : juce::File();
        runMilkDropTests(check,output,preset);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--original-visual-only"))
    {
        runOriginalVisualTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--ui-clarity-only"))
    {
        runUiClarityTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--ear-tools-only"))
    {
        runEarToolTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--grid-alignment-only"))
    {
        runGridAlignmentTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--grid-correction-only"))
    {
        runGridCorrectionTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--neural-unavailable-only"))
    {
        runNeuralFailureTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--analysis-refresh-only") || args.contains("--review-sorting-only"))
    {
        runAnalysisRefreshTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--focused-retry-probe"))
    {
        const int at = args.indexOf("--focused-retry-probe");
        if (at + 1 < args.size()) runFocusedRealProbe(check, output, juce::File(args[at + 1].unquoted()));
        else check(false, "real-source probe requires an explicit manifest");
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--analysis-cache-only"))
    {
        runAnalysisCacheTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--real-tracks-only"))
    {
        runRealTrackTests(check, output, args);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--constant-four-four-only"))
    {
        runConstantFourFourTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--rhythm-reliability-only"))
    {
        runRhythmReliabilityTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--tempo-only"))
    {
        runTempoTests(check, output); runTempoUiTests(check, output); runEarToolTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--placement-only"))
    {
        runPlacementTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--tempo-browser-only"))
    {
        runTempoBrowserTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--track-ownership-only") || args.contains("--split-effects-only"))
    {
        const int i = args.indexOf("--plugin");
        runTrackOwnershipTests(check, output, i >= 0 && i + 1 < args.size() ? juce::File(args[i + 1].unquoted()) : juce::File());
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--preset-nodes-only"))
    {
        runPresetNodeTests(check,output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--timeline-editing-only"))
    {
        runTimelineEditingSuite(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--window-only"))
    {
        runWindowTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--alloy-only"))
    {
        runAlloyTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--cache-reopen-only"))
    {
        Engine reopened; const auto sessionIndex = args.indexOf("--session"), digestIndex = args.indexOf("--expected-pcm");
        const auto error = sessionIndex >= 0 && sessionIndex + 1 < args.size() && juce::File::isAbsolutePath(args[sessionIndex + 1].unquoted())
            ? reopened.restoreProject(juce::File(args[sessionIndex + 1].unquoted()).loadFileAsString()) : juce::String("Missing test session.");
        check(error.isEmpty() && !reopened.library.empty() && !reopened.project.clips.empty()
              && std::all_of(reopened.project.clips.begin(), reopened.project.clips.end(), [](const auto& clip) { return clip.media->original && !clip.media->cache && !clip.prepared; }), "fresh-process reopen streams originals without whole-song copies: " + error);
        juce::AudioBuffer<float> pcm(2, 512); juce::MidiBuffer midi; reopened.render(pcm, 512, 2.1, midi); juce::MemoryOutputStream bytes;
        for (int ch = 0; ch < 2; ++ch) bytes.write(pcm.getReadPointer(ch), 512 * sizeof(float));
        check(digestIndex >= 0 && digestIndex + 1 < args.size() && juce::SHA256(bytes.getData(), bytes.getDataSize()).toHexString() == args[digestIndex + 1].unquoted(), "fresh-process streamed PCM equals the parent process render");
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    // Each UI suite owns one JUCE dispatch loop. Stopping it is terminal for that
    // process, so the focused Fusion workflow suite runs in a separate invocation.
    if (args.contains("--library-assets-only"))
    {
        // Reopen children have a bounded catalogue-only startup window; the
        // parent already covers stem rendering and native attachment.
        if (!args.contains("--catalogue-reopen")) runStemTests(check, output);
        const auto argumentFile = [&](const juce::String& key) { const auto i = args.indexOf(key); return i >= 0 && i + 1 < args.size() ? juce::File(args[i + 1].unquoted()) : juce::File(); };
        runLibraryAssetTests(check, output, argumentFile("--fixture-encoder"), argumentFile("--catalogue-reopen"));
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n"; output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--fusion-workflows-only") || args.contains("--playback-cache-only"))
    {
        if (args.contains("--playback-cache-only")) runPlaybackCacheTests(check, output); else runFusionWorkflowTests(check, output);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    if (args.contains("--beat-grid-only"))
    {
        const int referenceIndex = args.indexOf("--waveform-reference");
        const auto reference = referenceIndex >= 0 && referenceIndex + 1 < args.size() && juce::File::isAbsolutePath(args[referenceIndex + 1].unquoted())
            ? juce::File(args[referenceIndex + 1].unquoted()) : juce::File();
        runBeatGridTests(check, output, reference);
        report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
        output.replaceWithText(report); return failures == 0 ? 0 : 1;
    }
    Engine engine;
    auto media = std::make_shared<Media>(); media->id = "fixture"; media->name = "fixture"; media->sampleRate = 44100;
    auto fixture = output.getSiblingFile("fixture.wav"); media->file = fixture;
    media->samples.setSize(2, 44100); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 44100; ++i) media->samples.setSample(ch, i, 0.25f);
    { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = fixture.createOutputStream(); auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(44100).withNumChannels(2).withBitsPerSample(24)); if (writer) writer->writeFromAudioSampleBuffer(media->samples, 0, 44100); }
    engine.library.push_back(media); engine.project.masterDb = 0;
    Clip clip; clip.media = media; clip.length = 1; engine.project.clips.push_back(clip);
    juce::AudioBuffer<float> audio(2, 512); juce::MidiBuffer midi;
    engine.render(audio, 512, 0, midi); check(near(audio.getSample(0, 100), 0.25), "actual PCM clip playback");
    engine.project.tracks[0].gainDb = -6.0206f; engine.render(audio, 512, 0, midi); check(near(audio.getSample(0, 100), 0.125), "track gain");
    engine.project.tracks[0].gainDb = 0; engine.project.clips[0].fadeIn = 1; engine.render(audio, 512, 0.5, midi); check(near(audio.getSample(0, 0), 0.125), "fade automation");
    engine.project.clips[0].fadeIn = 0; engine.project.tracks[0].mute = true; engine.render(audio, 512, 0, midi); check(audio.getMagnitude(0, 512) == 0, "mute");
    engine.project.tracks[0].mute = false; engine.project.tracks[1].solo = true; engine.render(audio, 512, 0, midi); check(audio.getMagnitude(0, 512) == 0, "solo routing");
    engine.project.tracks[1].solo = false; engine.project.tracks[0].pan = 1; engine.render(audio, 512, 0, midi); check(audio.getMagnitude(0, 0, 512) == 0 && near(audio.getSample(1, 0), 0.25), "stereo pan"); engine.project.tracks[0].pan = 0;
    engine.render(audio, 512, -1, midi); check(audio.getMagnitude(0, 512) == 0, "clips silent while transport stopped");
    check(near(snapTime(0.76, 120, true), 1), "beat snapping");
    bool actionRoundTrip = true;
    for (auto action : midiActions()) { auto parsed = parseMidiAction(midiActionId(action)); actionRoundTrip = actionRoundTrip && parsed.hasValue() && *parsed == action; }
    check(actionRoundTrip && !parseMidiAction("volume--1").hasValue(), "named MIDI actions parse once to typed zero-based targets");
    auto volumeAction = parseMidiAction("volume-1");
    check(volumeAction.hasValue() && volumeAction->index == 0 && engine.applyMidiControl(*volumeAction, 1.0f)
        && near(juce::Decibels::decibelsToGain(engine.project.tracks[0].gainDb), 1.3), "learned fader action reaches the 130 percent ceiling when already at unity");
    engine.render(audio, 512, 0, midi); check(near(audio.getSample(0, 100), 0.25 * 1.3), "learned fader ceiling reaches the actual PCM mixer");
    engine.project.tracks[0].gainDb = 0;
    auto json = engine.captureProject(); auto text = juce::JSON::toString(json); check(parseProject(text).error.isEmpty(), "valid session contract");
    auto malformed = juce::JSON::parse(text); malformed.getDynamicObject()->setProperty("bpm", "not-a-number"); check(parseProject(juce::JSON::toString(malformed)).error.isNotEmpty(), "malformed boundary rejection");
    auto oldId = engine.project.clips[0].id; check(engine.restoreProject("{}").isNotEmpty() && engine.project.clips[0].id == oldId, "failed open preserves active session");
    auto session = output.getSiblingFile("roundtrip.mixworks"); check(engine.saveProject(session).isEmpty(), "save session");
    auto diskLibrary = readMedia(engine.formats, fixture);
    check(diskLibrary.media && diskLibrary.media->original && !diskLibrary.media->cache && diskLibrary.media->samples.getNumSamples() == 0 && near(diskLibrary.media->duration(), 1)
          && near(diskLibrary.media->sample(0,100), 0.25), "original-file library import retains no full decoded audio");
    check(engine.restoreProject(session.loadFileAsString()).isEmpty() && engine.project.clips.size() == 1, "session and source-media round trip");
    auto savedMappings = engine.mappings; engine.beginLearn("volume-1"); engine.testMidi(juce::MidiMessage::controllerEvent(1, 71, 64));
    check(engine.mappingSummary().contains("volume-1  <-  CC 71"), "FLkey MIDI learn from actual MIDI message parser");
    engine.beginLearn({}); engine.mappings = savedMappings; // Tests never save mappings to the user's profile.
    auto pluginIndex = args.indexOf("--plugin");
    if (pluginIndex >= 0 && pluginIndex + 1 < args.size())
    {
        auto error = engine.addPlugin(0, juce::File(args[pluginIndex + 1].unquoted())); check(error.isEmpty(), "load real VST3 module: " + error);
        if (error.isEmpty())
        {
            engine.render(audio, 512, 0, midi); check(near(audio.getSample(0, 100), 0.125), "VST3 processBlock changes PCM");
            check(engine.assignPluginControl(0, 0, 0, 0).isEmpty() && engine.pluginControlSummary(0).contains("Gain"), "named plugin MIDI control assignment uses the real VST3 parameter");
            check(engine.applyMidiControl({MidiActionKind::pluginParameter, 0}, 0.25f), "plugin MIDI control reaches its assigned stable parameter ID");
            auto saved = juce::JSON::toString(engine.captureProject());
            juce::String recoveryError; auto recovery = RecoveryStore::create(output.getSiblingFile("plugin-recovery"), recoveryError);
            check(recovery && recovery->write(saved, session).wasOk(), "recovery captures the real VST3 state outside the audio callback");
            const auto recoveredPlugin = recovery ? recovery->read().document : juce::String();
            Engine recovered;
            check(recovery && recovered.restoreProject(recoveredPlugin).isEmpty(), "recovery reopens the real VST3 through the production loader");
            recovered.render(audio, 512, 0, midi); check(near(audio.getSample(0, 100), .0625), "recovered VST3 state produces its exact expected PCM");
            auto unavailable = juce::JSON::parse(recoveredPlugin);
            auto missingDescription = engine.plugins[0][0]->description;
            missingDescription.fileOrIdentifier = output.getSiblingFile("missing-recovery-plugin.vst3").getFullPathName();
            unavailable["tracks"].getArray()->getReference(0)["plugins"].getArray()->getReference(0).getDynamicObject()->setProperty("description", missingDescription.createXml()->toString());
            const auto retainedProject = juce::JSON::toString(recovered.captureProject());
            check(recovered.restoreProject(juce::JSON::toString(unavailable)).isNotEmpty() && juce::JSON::toString(recovered.captureProject()) == retainedProject,
                  "an unavailable recovery VST3 cannot replace the current project or plugin state");
            if (recovery) recovery->discard();
            check(engine.restoreProject(saved).isEmpty(), "VST3 state restore");
            check(engine.pluginControlSummary(0).contains("Gain") && engine.applyMidiControl({MidiActionKind::pluginParameter, 0}, 0.25f), "named plugin MIDI routing survives session reload");
            engine.render(audio, 512, 0, midi); check(near(audio.getSample(0, 100), 0.0625), "restored VST3 parameter affects actual audio");
            engine.plugins[0][0]->bypass = true; engine.render(audio, 512, 0, midi); check(near(audio.getSample(0, 100), 0.25), "VST3 bypass"); engine.plugins[0][0]->bypass = false; engine.render(audio, 512, 0, midi);
            { std::unique_ptr<juce::AudioProcessorEditor> editor(engine.plugins[0][0]->processor->createEditorAndMakeActive()); check(editor != nullptr && editor->getWidth() > 0, "native VST3 editor creation"); }
        }
    }
    auto instrumentIndex = args.indexOf("--instrument");
    if (instrumentIndex >= 0 && instrumentIndex + 1 < args.size())
    {
        auto error = engine.addPlugin(1, juce::File(args[instrumentIndex + 1].unquoted())); check(error.isEmpty(), "installed VST3 instrument load: " + error);
        if (error.isEmpty())
        {
            engine.selectedTrack = 1;
            juce::MidiBuffer notes; notes.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
            float peak = 0;
            for (int block = 0; block < 32; ++block) { engine.render(audio, 512, -1, notes); peak = std::max(peak, audio.getMagnitude(0, 512)); notes.clear(); }
            check(peak > 0.0001f, "installed VST3 instrument produces PCM from MIDI note input");
            engine.silenceNotes(); engine.plugins[1].clear(); engine.selectedTrack = 0;
            engine.render(audio, 512, 0, midi);
        }
    }
    engine.render(audio, 512, 0, midi);
    auto exported = output.getSiblingFile("render.wav"); check(engine.exportWav(exported).isEmpty(), "offline WAV export through the same mixer and VST chain");
    std::unique_ptr<juce::AudioFormatReader> reader(engine.formats.createReaderFor(exported));
    check(reader && reader->lengthInSamples == 44100 && reader->bitsPerSample == 24, "export duration and 24-bit format");
    if (reader) { juce::AudioBuffer<float> rendered(2, 512); reader->read(&rendered, 0, 512, 0, true, true); check(near(rendered.getSample(0, 100), audio.getSample(0, 100)), "export PCM matches live render"); }
    check(engine.exportWav(fixture).isNotEmpty(), "export never replaces source media");
    runMusicTests(check, output);
    runTempoTests(check, output);
    runAnalysisCacheTests(check, output);
    runMusicalEnergyTests(check, output);
    runEditingTests(check, output);
    runAutomationTests(check, output);
    if (pluginIndex >= 0 && pluginIndex + 1 < args.size()) runLiveAutomationTests(check, output, juce::File(args[pluginIndex + 1].unquoted()));
    runTempoUiTests(check, output);
    juce::var inventory(new juce::DynamicObject()); juce::Array<juce::var> devices;
    inventory.getDynamicObject()->setProperty("purpose", "MIDI input availability, not physical-key verification");
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        juce::var entry(new juce::DynamicObject());
        entry.getDynamicObject()->setProperty("name", device.name);
        entry.getDynamicObject()->setProperty("identifier", device.identifier);
        devices.add(entry);
    }
    inventory.getDynamicObject()->setProperty("devices", devices);
    check(output.getSiblingFile("midi-input-inventory.json").replaceWithText(juce::JSON::toString(inventory)),
          "write structured MIDI input availability separately from check completion");
    report += juce::String(tests) + " checks; " + juce::String(failures) + " failures\n";
    output.replaceWithText(report); return failures == 0 ? 0 : 1;
}
}
