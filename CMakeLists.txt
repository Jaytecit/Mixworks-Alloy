cmake_minimum_required(VERSION 3.22)
project(Mixworks VERSION 0.55.133 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.local/deps/JUCE" CACHE PATH "JUCE 8 source directory")
include(Source/JuceMp3Length.cmake)
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
set(SOUNDSTRETCH OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.local/deps/soundtouch" "${CMAKE_BINARY_DIR}/SoundTouch" EXCLUDE_FROM_ALL)
if(MSVC)
  set_property(TARGET SoundTouch PROPERTY COMPILE_DEFINITIONS SOUNDTOUCH_FLOAT_SAMPLES)
  target_compile_options(SoundTouch PRIVATE /O2 /fp:fast)
endif()
juce_add_gui_app(Mixworks PRODUCT_NAME "Mixworks Alloy" COMPANY_NAME "Mixworks" VERSION ${PROJECT_VERSION})
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/mixworks-$<CONFIG>.path" CONTENT "$<TARGET_FILE:Mixworks>")
if(WIN32)
  # Pinned JUCE omits Info.txt from the RC generator's dependencies.
  get_target_property(mixworks_generated_sources Mixworks JUCE_GENERATED_SOURCES_DIRECTORY)
  get_target_property(mixworks_info Mixworks JUCE_INFO_FILE)
  add_custom_command(OUTPUT "${mixworks_generated_sources}/Mixworks_resources.rc" APPEND DEPENDS "${mixworks_info}")
endif()
target_sources(Mixworks PRIVATE Source/Main.cpp Source/Model.cpp Source/Analysis.cpp Source/Beatmatch.cpp Source/Engine.cpp Source/Automation.cpp Source/AutomationUI.cpp Source/TimelineAutomation.cpp Source/SectionUI.cpp Source/Workstation.cpp Source/SelfTest.cpp Source/MusicTests.cpp Source/TempoTests.cpp Source/TempoUiTests.cpp Source/EditingTests.cpp Source/AutomationTests.cpp Source/LiveAutomationTests.cpp)
target_compile_definitions(Mixworks PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_VST=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Mixworks,JUCE_PRODUCT_NAME>" JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
target_sources(Mixworks PRIVATE Source/AnalysisCache.cpp Source/WorkspaceUI.cpp Source/TutorialMusic.cpp Source/AnalysisCacheTests.cpp Source/Playlist.cpp Source/FusionWorkflows.cpp Source/FusionWorkflowTests.cpp)
target_sources(Mixworks PRIVATE Source/MusicalEnergy.cpp Source/MusicalEnergyTests.cpp)
target_sources(Mixworks PRIVATE Source/StemJobs.cpp Source/StemMixing.cpp Source/StemUI.cpp Source/StemTests.cpp)
target_sources(Mixworks PRIVATE Source/GridCorrection.cpp Source/GridCorrectionUI.cpp Source/GridCorrectionTests.cpp)
target_sources(Mixworks PRIVATE Source/UiMessages.cpp Source/RetryReview.cpp Source/UiClarityTests.cpp)
target_sources(Mixworks PRIVATE Source/EarTools.cpp Source/KeyEarUI.cpp Source/HarmonyEarUI.cpp Source/EarToolWorkflows.cpp Source/EarToolTests.cpp)
target_sources(Mixworks PRIVATE Source/RhythmDecoder.cpp Source/RhythmDecoderTests.cpp Source/RhythmResearch.cpp)
target_sources(Mixworks PRIVATE Source/AuftaktFeatures.cpp Source/AuftaktFeatureTests.cpp Source/AuftaktNetwork.cpp)
target_sources(Mixworks PRIVATE Source/PcmCache.cpp Source/MusicScanner.cpp Source/PlaybackCacheTests.cpp)
target_sources(Mixworks PRIVATE Source/ImportScanTests.cpp)
target_sources(Mixworks PRIVATE Source/LibraryAssets.cpp Source/LibraryCatalogue.cpp Source/LibraryWorkflows.cpp Source/LibraryAssetTests.cpp Source/AnalysisRefreshTests.cpp)
target_sources(Mixworks PRIVATE Source/BeatGridTests.cpp)
target_sources(Mixworks PRIVATE Source/RhythmReliabilityTests.cpp)
target_sources(Mixworks PRIVATE Source/RealTrackTests.cpp)
target_sources(Mixworks PRIVATE Source/TimelinePlacement.cpp Source/PlacementTests.cpp)
target_sources(Mixworks PRIVATE Source/LongSessionTests.cpp Source/VerificationMetrics.cpp Source/SourceStreamingTests.cpp Source/PluginLatency.cpp Source/LatencyTests.cpp Source/ExportTests.cpp Source/WavExport.cpp Source/WavExportUI.cpp)
target_include_directories(Mixworks SYSTEM PRIVATE "${JUCE_DIR}/modules/juce_audio_processors_headless/format_types/VST3_SDK")
target_sources(Mixworks PRIVATE Source/Mastering.cpp Source/Spectrum.cpp Source/MasteringTests.cpp Source/MasteringUI.cpp)
target_sources(Mixworks PRIVATE Source/Recovery.cpp Source/RecoveryUI.cpp Source/RecoveryTests.cpp)
target_sources(Mixworks PRIVATE Source/AlloyLookAndFeel.cpp Source/WorkspaceLayout.cpp Source/TimelineHeader.cpp Source/AlloyTests.cpp)
target_sources(Mixworks PRIVATE Source/OutputPeaks.cpp Source/PeakMeter.cpp)
target_sources(Mixworks PRIVATE Source/PromoControl.cpp)
target_sources(Mixworks PRIVATE Source/AttractLights.cpp Source/AttractTests.cpp)
target_sources(Mixworks PRIVATE Source/TempoStrip.cpp)
target_sources(Mixworks PRIVATE Source/AppWindow.cpp Source/WindowTests.cpp)
target_sources(Mixworks PRIVATE Source/PerformanceDeck.cpp Source/PerformanceUI.cpp Source/PerformanceModeUI.cpp)
target_sources(Mixworks PRIVATE Source/PerformanceTake.cpp Source/PerformanceCapture.cpp Source/MasterRecorder.cpp Source/PrivatePreparation.cpp Source/PerformanceTakeTests.cpp Source/TargetedAnalysisTests.cpp Source/ImportRecovery.cpp)
target_sources(Mixworks PRIVATE Source/LaunchClock.cpp Source/LaunchPause.cpp Source/LaunchTests.cpp Source/LiveHotcues.cpp Source/LiveHotcueTests.cpp)
target_sources(Mixworks PRIVATE Source/SourceSectionStore.cpp Source/SourceSectionUI.cpp Source/SourceSectionTests.cpp)
target_sources(Mixworks PRIVATE Source/DownbeatReferences.cpp)
target_sources(Mixworks PRIVATE Source/PresetNodes.cpp Source/PresetNodeTests.cpp)
target_sources(Mixworks PRIVATE Source/EditSelection.cpp Source/EditSelectionTests.cpp)
target_sources(Mixworks PRIVATE Source/ElasticAudio.cpp Source/OriginalAudio.cpp)
target_sources(Mixworks PRIVATE Source/Transitions.cpp Source/TimelineEditing.cpp Source/TimelineEditingTests.cpp)
target_sources(Mixworks PRIVATE Source/BarNavigation.cpp Source/BarNavigationTests.cpp Source/RehearsalContext.cpp Source/RehearsalContextTests.cpp Source/TransitionSwapHandles.cpp Source/TransitionSwapTests.cpp Source/TransitionNavigation.cpp Source/TransitionNavigationTests.cpp Source/TransitionFilters.cpp Source/TransitionFilterTests.cpp Source/TransitionControls.cpp Source/TransitionBands.cpp Source/TransitionUI.cpp Source/TransitionBandTests.cpp)
target_sources(Mixworks PRIVATE Source/TransitionStyles.cpp Source/TransitionRehearsal.cpp Source/TransitionRehearsalUI.cpp Source/TransitionRehearsalTests.cpp)
target_sources(Mixworks PRIVATE Source/TrackOwnershipTests.cpp)
target_sources(Mixworks PRIVATE Source/TempoBrowserTests.cpp)
set(BEAT_THIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.local/deps/beat-this-cpp")
set(ONNX_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.local/deps/onnxruntime-win-x64-1.24.4")
target_sources(Mixworks PRIVATE Source/NeuralRhythm.cpp Source/ContentDigest.cpp Source/ThirdParty/BeatThis/MelSpectrogram.cpp Source/ThirdParty/BeatThis/Postprocessor.cpp)
target_sources(Mixworks PRIVATE Source/NeuralFailureTests.cpp)
target_include_directories(Mixworks SYSTEM PRIVATE "${ONNX_RUNTIME_DIR}/include" "${BEAT_THIS_DIR}/Submodule/pocketfft" "${BEAT_THIS_DIR}/Submodule/r8brain")
target_link_libraries(Mixworks PRIVATE "${ONNX_RUNTIME_DIR}/lib/onnxruntime.lib")
add_custom_command(TARGET Mixworks POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ONNX_RUNTIME_DIR}/lib/onnxruntime.dll" "$<TARGET_FILE_DIR:Mixworks>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BEAT_THIS_DIR}/onnx/beat_this.onnx" "$<TARGET_FILE_DIR:Mixworks>/beat-this-final0.onnx")
if(WIN32)
  target_compile_definitions(Mixworks PRIVATE JUCE_ASIO=1)
  target_link_libraries(Mixworks PRIVATE propsys windowscodecs dwmapi psapi bcrypt winsqlite3)
endif()
target_link_libraries(Mixworks PRIVATE SoundTouch juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography juce::juce_recommended_config_flags)
if(MSVC)
  target_compile_options(Mixworks PRIVATE /W4 /permissive- /MP)
endif()
option(MIXWORKS_BUILD_TEST_PLUGIN "Build a local test-only VST3 effect" OFF)
if(MIXWORKS_BUILD_TEST_PLUGIN)
  juce_add_plugin(MixworksTestGain COMPANY_NAME "Mixworks" PLUGIN_MANUFACTURER_CODE Mxwk PLUGIN_CODE Mtgn FORMATS VST3 PRODUCT_NAME "Mixworks Test Gain" IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(MixworksTestGain PRIVATE tests/TestGain.cpp)
  target_compile_definitions(MixworksTestGain PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(MixworksTestGain PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
  juce_add_plugin(MixworksTestLatency COMPANY_NAME "Mixworks" PLUGIN_MANUFACTURER_CODE Mxwk PLUGIN_CODE Mtlt FORMATS VST3 PRODUCT_NAME "Mixworks Test Latency" IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(MixworksTestLatency PRIVATE tests/TestLatency.cpp)
  target_compile_definitions(MixworksTestLatency PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(MixworksTestLatency PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
  juce_add_plugin(MixworksTestDelay COMPANY_NAME "Mixworks" PLUGIN_MANUFACTURER_CODE Mxwk PLUGIN_CODE Mtdl FORMATS VST3 PRODUCT_NAME "Mixworks Test Delay" IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(MixworksTestDelay PRIVATE tests/TestDelay.cpp)
  target_compile_definitions(MixworksTestDelay PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(MixworksTestDelay PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()

target_sources(Mixworks PRIVATE Source/AudioSlipUI.cpp Source/AudioSlipTests.cpp)
target_sources(Mixworks PRIVATE Source/Preroll.cpp Source/PrerollTests.cpp)
target_sources(Mixworks PRIVATE Source/TransitionCutTests.cpp)
target_sources(Mixworks PRIVATE Source/PlaylistTransitions.cpp Source/EnergySections.cpp Source/PlaylistTransitionTests.cpp Source/SectionTests.cpp)
target_sources(Mixworks PRIVATE Source/ArrangementInsertion.cpp Source/ArrangementInsertionUI.cpp Source/ArrangementInsertionTests.cpp)
target_sources(Mixworks PRIVATE Source/Hotcues.cpp Source/HotcueUI.cpp Source/HotcueTimeline.cpp Source/HotcueTests.cpp)
target_sources(Mixworks PRIVATE Source/MidiConnection.cpp Source/MidiConnectionUI.cpp Source/MidiConnectionTests.cpp)
target_sources(Mixworks PRIVATE Source/AutoArrangement.cpp Source/AutoArrangementUI.cpp Source/AutoArrangementTests.cpp Source/AutoPairComparison.cpp)
target_sources(Mixworks PRIVATE Source/LibraryPreview.cpp Source/LibraryPreviewTests.cpp)

# Visual runtime is packaged beside the executable; no reference installation is used.
target_sources(Mixworks PRIVATE Source/VisualScenes.cpp Source/VisualArrangement.cpp Source/VisualArrangementTests.cpp Source/SceneLane.cpp Source/SceneLaneTests.cpp Source/StudioWorkspace.cpp)
target_sources(Mixworks PRIVATE Source/StudioAudioLink.cpp Source/StudioAudioLinkTests.cpp)
target_sources(Mixworks PRIVATE Source/MilkDrop.cpp)


  set(alloy_original_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/visual_scene_worker.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/studio_scene_player.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/visual_compositor.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/visual_handoff.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/visual_feedback.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/visual_presenter.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/original_scene_player.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/milkdrop_scene.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/milkdrop_catalogue.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/milkdrop_preview.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/visual_catalogue.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/visual_media.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/original_visual_gl.py")
  file(GLOB_RECURSE alloy_original_assets CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/visuals/original/*")
  set(alloy_studio_media ${alloy_original_assets})
  list(FILTER alloy_studio_media INCLUDE REGEX "/(Sprites|Backgrounds)/")
  file(GLOB_RECURSE alloy_studio_scenes CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/visual-studio/scenes/*")
  set(alloy_studio_sources)
  foreach(module engine scene media audio audio_sources studio timeline deck midi display)
    list(APPEND alloy_studio_sources "${CMAKE_CURRENT_SOURCE_DIR}/visual-studio/${module}.py")
  endforeach()
  set(alloy_studio_assets
    "${CMAKE_CURRENT_SOURCE_DIR}/visual-studio/alloy-flkey49.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/visual-studio/scene-library.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/visual-studio/authoring/Prism Current.milk")
  set(alloy_studio_background "${CMAKE_CURRENT_SOURCE_DIR}/visuals/original/Backgrounds/Molten Alloy Waveform Core-1.png")
  set(alloy_studio_sprite "${CMAKE_CURRENT_SOURCE_DIR}/visuals/original/Sprites/Copper Martini - Circuit Martini.png")
  set(alloy_studio_flare "${CMAKE_CURRENT_SOURCE_DIR}/visuals/original/Sprites/Copper Cyan VFX - Horizontal Flare.png")
  list(FILTER alloy_original_assets EXCLUDE REGEX "/(Sprites|Backgrounds|Video)/")
  set_property(TARGET Mixworks APPEND PROPERTY LINK_DEPENDS ${alloy_original_sources} ${alloy_original_assets} ${alloy_studio_sources} ${alloy_studio_assets} ${alloy_studio_scenes} ${alloy_studio_media} ${alloy_studio_background} ${alloy_studio_sprite} ${alloy_studio_flare})
  add_custom_command(TARGET Mixworks POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${alloy_original_sources} "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/studio_engine" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/studio"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${alloy_studio_sources} "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/studio_engine"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${alloy_studio_assets} "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/studio"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${alloy_studio_background}" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/studio/Copper Background.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${alloy_studio_sprite}" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/studio/Circuit Sprite.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${alloy_studio_flare}" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/studio/Flare Sprite.png"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/visual-studio/scenes" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/studio/scenes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/visuals/original/scenes" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/scenes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/visuals/original/previews" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/previews"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/visuals/original/original-scenes.json" "${CMAKE_CURRENT_SOURCE_DIR}/visuals/original/media-library.json" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/visuals/original/Sprites" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/Sprites"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/visuals/original/Backgrounds" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/Backgrounds"
    VERBATIM)

# Optional shared renderer; no NestDrop binaries or third-party presets are bundled.
# Prepare with scripts/fetch-projectm.ps1. Ordinary owned scenes do not load this DLL.
set(alloy_projectm "${CMAKE_CURRENT_SOURCE_DIR}/.local/deps/projectm")
if(EXISTS "${alloy_projectm}/bin/projectM-4.dll")
  add_custom_command(TARGET Mixworks POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/milkdrop"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${alloy_projectm}/bin/projectM-4.dll" "${alloy_projectm}/bin/glew32.dll" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/milkdrop"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${alloy_projectm}/licenses" "$<TARGET_FILE_DIR:Mixworks>/original-visualiser-runtime/milkdrop/licenses")
endif()

# The connected standalone Studio uses the same private Python, with a matching
# Tk runtime copied from the developer's installed interpreter.
find_program(ALLOY_TK_PYTHON NAMES python REQUIRED)
add_custom_command(TARGET Mixworks POST_BUILD
  COMMAND "${ALLOY_TK_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/stage_studio_tk.py"
          --destination "$<TARGET_FILE_DIR:Mixworks>/visual-python"
  VERBATIM)
