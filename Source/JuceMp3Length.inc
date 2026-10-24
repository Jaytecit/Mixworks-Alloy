    int64 findLength (int64 streamStartPos)
    {
        // Keep the established Xing/VBR clock. Files without that header must
        // count frames: the first frame's padding is not the padding of every
        // frame (e.g. 320 kbps / 44.1 kHz alternates 1044 and 1045 bytes).
        if (stream.numFrames > 0)
            return (int64) stream.numFrames * 1152;

        auto& input = stream.stream;
        const auto restore = input.getPosition();
        const auto size = input.getTotalLength();
        int64 samples = 0;
        bool valid = true;
        for (auto pos = streamStartPos; pos < size;)
        {
            input.setPosition (pos);
            const auto header = (uint32) input.readIntBigEndian();
            // Standard trailing metadata is not audio. A damaged audio frame
            // is rejected below rather than being silently accepted as EOF.
            if (samples > 0 && ((size - pos == 128 && (header >> 8) == 0x544147)
                || (header == 0x41504554 && (uint32) input.readIntBigEndian() == 0x41474558)))
                break;
            if (samples > 0 && header == 0)
            {
                input.setPosition (pos);
                while (! input.isExhausted())
                    if (input.readByte() != 0) { valid = false; break; }
                break;
            }
            const bool headerValid = (header & 0xffe00000) == 0xffe00000
                && ((header >> 19) & 3) != 1 && ((header >> 17) & 3) != 0
                && ((header >> 12) & 15) > 0 && ((header >> 12) & 15) < 15
                && ((header >> 10) & 3) != 3;
            MP3Frame frame {};
            if (! headerValid || frame.decodeHeader (header) == MP3Frame::ParseSuccessful::no
                || frame.layer != stream.frame.layer || frame.getFrequency() != stream.frame.getFrequency())
            {
                if (samples == 0 && pos < streamStartPos + 32768) { ++pos; continue; }
                valid = false; break;
            }
            const auto bytes = frame.frameSize + 4;
            if (bytes <= 4 || pos + bytes > size) { valid = false; break; }
            samples += frame.layer == 1 ? 384 : (frame.layer == 3 && frame.lsf ? 576 : 1152);
            pos += bytes;
        }
        input.setPosition (restore);
        return valid ? samples : 0;
    }
