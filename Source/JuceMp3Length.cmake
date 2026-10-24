# Reproducible, narrowly scoped patch for the pinned JUCE 8.0.16-security reader.
# Refuse unknown implementations; never depend on a manual edit in .local/deps.
set(mp3_source "${JUCE_DIR}/modules/juce_audio_formats/codecs/juce_MP3AudioFormat.cpp")
file(READ "${mp3_source}" mp3_text)
file(READ "${CMAKE_CURRENT_LIST_DIR}/JuceMp3Length.inc" mp3_replacement)
string(STRIP "${mp3_replacement}" mp3_replacement)
set(mp3_old [=[int64 findLength (int64 streamStartPos)
    {
        int64 numFrames = stream.numFrames;

        if (numFrames <= 0)
        {
            const int64 streamSize = stream.stream.getTotalLength();

            if (streamSize > 0)
            {
                const int bytesPerFrame = stream.frame.frameSize + 4;

                if (bytesPerFrame == 417 || bytesPerFrame == 418)
                    numFrames = roundToInt ((double) (streamSize - streamStartPos) / 417.95918); // more accurate for 128k
                else
                    numFrames = (streamSize - streamStartPos) / bytesPerFrame;
            }
        }

        return numFrames * 1152;
    }]=])
string(FIND "${mp3_text}" "${mp3_old}" mp3_original_at)
string(FIND "${mp3_text}" "${mp3_replacement}" mp3_patched_at)
if(NOT mp3_original_at EQUAL -1)
    string(REPLACE "${mp3_old}" "${mp3_replacement}" mp3_text "${mp3_text}")
    file(WRITE "${mp3_source}" "${mp3_text}")
elseif(mp3_patched_at EQUAL -1)
    message(FATAL_ERROR "Unknown JUCE MP3 length implementation; dependency preserved for inspection.")
endif()
