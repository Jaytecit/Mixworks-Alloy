#include "MusicScanner.h"
#include <cmath>

namespace mixworks
{
void runImportScanTests(const std::function<void(bool,const juce::String&)>& check, const juce::File& report, const juce::StringArray& args)
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    const auto inputs = report.getSiblingFile("inputs");
    const auto manifest = juce::JSON::parse(inputs.getChildFile("fixtures.json"));
    if (auto* fixtures = manifest.getArray()) for (const auto& fixture : *fixtures)
    {
        const auto file = inputs.getChildFile(fixture["file"].toString());
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        bool decoded = reader && reader->lengthInSamples > 0; double energy = 0;
        juce::AudioBuffer<float> block(2,4096);
        if (reader) for (juce::int64 pos=0;decoded && pos<reader->lengthInSamples;pos+=4096)
        {
            const int n = (int) std::min<juce::int64>(4096,reader->lengthInSamples-pos);
            decoded = reader->read(&block,0,n,pos,true,true);
            for (int c=0;c<2;++c) for (int i=0;i<n;++i) { const auto s=block.getSample(c,i); decoded &= std::isfinite(s); energy += s*s; }
        }
        check(decoded && energy > 0, "complete finite audio read: " + file.getFileName());
        if (fixture.hasProperty("frames")) check(reader && reader->lengthInSamples == (juce::int64)fixture["frames"],"exact packet length: " + file.getFileName());
    }
    else check(false,"generated import fixtures are present");
    const int sourceIndex = args.indexOf("--source");
    if (sourceIndex>=0 && sourceIndex+1<args.size())
    {
        const juce::File file(args[sourceIndex+1].unquoted());
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        bool ok = reader && reader->lengthInSamples == 16160256;
        juce::int64 lastRead = -1;
        juce::AudioBuffer<float> block(2,4096);
        if (reader) for (juce::int64 pos=0;ok && pos<reader->lengthInSamples;pos+=4096)
        {
            lastRead = pos;
            ok = reader->read(&block,0,(int)std::min<juce::int64>(4096,reader->lengthInSamples-pos),pos,true,true);
        }
        check(ok,"previously failing real MP3 reads all 14028 frames; no analysis/cache writes / frames="
            + juce::String(reader ? reader->lengthInSamples : -1) + " / read=" + juce::String(lastRead) + " / file=" + file.getFullPathName());
    }
    const auto scanRoot = report.getSiblingFile("broken-scan"); scanRoot.createDirectory();
    const auto bad = scanRoot.getChildFile("unreadable.wav"); bad.replaceWithText("not an audio file");
    MusicScan scan(scanRoot,{},{}); const auto failed = scan.step();
    const auto text = failed.report.loadFileAsString();
    check(failed.file == bad && !failed.loaded.media && failed.reportError.isEmpty() && text.contains("FAILED")
          && text.contains(bad.getFullPathName()) && text.contains(failed.loaded.error),"failed imports retain the exact filename and decoder reason");
    MusicScan unchanged(scanRoot,{},{{bad.getFullPathName(),failed.stamp}});
    check(unchanged.step().file == juce::File(),"automatic passes avoid retrying an unchanged failed file continuously");
    MusicScan retry(scanRoot,{},{});
    check(retry.step().file == bad,"explicit retry can read a previously failed source again");
    auto media=std::make_shared<Media>(); media->playbackCacheWarning="Fixture cache write failure";
    const auto warningReport=report.getSiblingFile("warning-report.txt");
    check(writeMusicScanIssue(warningReport,bad,{media,{}}).wasOk()
        && warningReport.loadFileAsString().contains("CACHE WARNING\n" + bad.getFullPathName()),"successful audio with a cache warning is reported separately from an import failure");
}
}
