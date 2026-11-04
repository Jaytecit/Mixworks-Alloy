#include "MilkDrop.h"
#include "AnalysisCache.h"
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>

namespace mixworks
{
bool isMilkDropKey(const juce::String& key)
{
    return key.startsWith("milkdrop/") && key.length()==73
        && key.substring(9).containsOnly("0123456789abcdef");
}
juce::File milkDropLibraryRoot()
{
    // The development bridge redirects analysisStoreRoot before constructing the UI.
    return analysisStoreRoot().getParentDirectory().getChildFile("MilkDrop-v1");
}
bool milkDropRendererAvailable()
{
    const auto runtime=juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getSiblingFile("original-visualiser-runtime").getChildFile("milkdrop");
    return runtime.getChildFile("projectM-4.dll").existsAsFile() && runtime.getChildFile("glew32.dll").existsAsFile();
}
std::vector<VisualSceneEntry> discoverMilkDropScenes(const juce::File& library)
{
    std::vector<VisualSceneEntry> result;
    auto folders=library.findChildFiles(juce::File::findDirectories,false);
    std::sort(folders.begin(),folders.end(),[](const auto& a,const auto& b){return a.getFileName()<b.getFileName();});
    for (const auto& folder:folders)
    {
        if(result.size()>=128) break;
        const auto key="milkdrop/"+folder.getFileName();
        const auto metadata=folder.getChildFile("import.json"), preset=folder.getChildFile("preset.milk");
        if(!isMilkDropKey(key) || !metadata.existsAsFile() || metadata.getSize()>32768
            || !preset.existsAsFile() || preset.getSize()>1024*1024) continue;
        const auto record=juce::JSON::parse(metadata);
        const auto title=record["title"].toString();
        if((int)record["schema"]!=1 || record["sha256"].toString()!=folder.getFileName()
            || juce::SHA256(preset).toHexString()!=folder.getFileName() || title.isEmpty() || title.length()>96) continue;
        VisualSceneEntry entry {key,"MilkDrop: "+title};
        entry.style="MilkDrop"; entry.family="milkdrop"; entry.motif="milkdrop";
        entry.controlSpecs["intensity"]={.5,0};
        result.push_back(std::move(entry));
    }
    return result;
}
juce::Result importMilkDropPreset(const juce::File& source, const juce::File& library, juce::String& key)
{
    if(!source.existsAsFile() || !source.hasFileExtension("milk") || source.getSize()<16 || source.getSize()>1024*1024)
        return juce::Result::fail("Choose a MilkDrop .milk preset up to 1 MB.");
    juce::MemoryBlock bytes;
    if(!source.loadFileAsData(bytes)) return juce::Result::fail("Could not read the MilkDrop preset.");
    const auto text=source.loadFileAsString();
    if(!text.containsIgnoreCase("[preset00]") || bytes.getSize()>1024*1024)
        return juce::Result::fail("This file is not a MilkDrop .milk preset.");
    const auto hash=juce::SHA256(bytes).toHexString(), importedKey="milkdrop/"+hash;
    const auto folder=library.getChildFile(hash);
    const auto existing=discoverMilkDropScenes(library);
    for(const auto& entry:existing) if(entry.key==importedKey) { key=importedKey; return juce::Result::ok(); }
    if(existing.size()>=128) return juce::Result::fail("The MilkDrop library is limited to 128 imported presets.");
    if(folder.createDirectory().failed()) return juce::Result::fail("Could not create the local MilkDrop library.");
    // Commit the descriptor last; an interrupted import is never catalogued.
    if(!folder.getChildFile("preset.milk").replaceWithData(bytes.getData(),bytes.getSize()))
        return juce::Result::fail("Could not store the imported MilkDrop preset.");
    auto* record=new juce::DynamicObject();
    record->setProperty("schema",1); record->setProperty("sha256",hash);
    record->setProperty("title",source.getFileNameWithoutExtension().substring(0,86));
    record->setProperty("source",source.getFullPathName());
    juce::Array<juce::var> paths;
    paths.add(source.getParentDirectory().getFullPathName());
    const auto textures=source.getParentDirectory().getSiblingFile("Textures");
    if(textures.isDirectory()) paths.add(textures.getFullPathName());
    record->setProperty("texturePaths",paths);
    if(!folder.getChildFile("import.json").replaceWithText(juce::JSON::toString(juce::var(record))))
        return juce::Result::fail("Could not save the MilkDrop import record.");
    key=importedKey;
    return juce::Result::ok();
}
}
