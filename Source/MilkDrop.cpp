#include "MilkDrop.h"
#include "AnalysisCache.h"
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>
#include <filesystem>
#include <set>

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
        const auto source=record["source"].toString();
        if(juce::File::isAbsolutePath(source))
        { const auto image=juce::File(source).withFileExtension(".jpg"); if(image.existsAsFile()) entry.referenceImage=image.getFullPathName(); }
        entry.controlSpecs["intensity"]={.5,0};
        result.push_back(std::move(entry));
    }
    std::set<juce::String> seen;
    for(const auto& entry:result) seen.insert(entry.key);
    for(const auto& file:library.getChildFile("linked").findChildFiles(juce::File::findFiles,false,"*.json"))
    {
        const auto manifest=juce::JSON::parse(file);
        if((int)manifest["schema"]!=2 || !juce::File::isAbsolutePath(manifest["root"].toString())) continue;
        const auto* entries=manifest["entries"].getArray(); if(!entries) continue;
        for(const auto& value:*entries)
        {
            const auto key="milkdrop/"+value["sha256"].toString(), title=value["title"].toString();
            if(!isMilkDropKey(key) || title.isEmpty() || title.length()>96
                || !juce::File::isAbsolutePath(value["source"].toString())
                || !juce::File(value["source"].toString()).isAChildOf(juce::File(manifest["root"].toString())) || !seen.insert(key).second) continue;
            VisualSceneEntry entry {key,"MilkDrop: "+title};
            entry.style="MilkDrop"; entry.family="milkdrop"; entry.motif="milkdrop";
            const auto image=value["preview"].toString();
            if(juce::File::isAbsolutePath(image)) entry.referenceImage=image;
            entry.controlSpecs["intensity"]={.5,0}; result.push_back(std::move(entry));
        }
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.title.compareNatural(b.title)<0;});
    return result;
}
juce::Result linkMilkDropFolder(const juce::File& source, const juce::File& library,
                               int& indexed, int& skipped, const std::function<bool()>& cancelled)
{
    indexed=skipped=0;
    if(!source.isDirectory()) return juce::Result::fail("MilkDrop folder is unavailable. Choose its current location.");
    namespace fs=std::filesystem;
    juce::Array<juce::var> entries; std::set<juce::String> seen;
    std::error_code error;
    fs::recursive_directory_iterator it(fs::path(source.getFullPathName().toWideCharPointer()),fs::directory_options::skip_permission_denied,error), end;
    for(; !error && it!=end; it.increment(error))
    {
        if(cancelled && cancelled()) return juce::Result::fail("MilkDrop indexing cancelled; the previous index was kept.");
        // Never descend into junctions/symlinks or modify collection contents.
        const juce::File file(juce::String(it->path().wstring().c_str()));
        std::error_code entryError;
        if(it->is_symlink(entryError)) { it.disable_recursion_pending(); continue; }
        // NestDrop may create/replace JPGs during a scan. A vanished entry is
        // local to that file, not a traversal failure that invalidates the index.
        if(!file.hasFileExtension("milk")) continue;
        if(entryError || !it->is_regular_file(entryError)) { ++skipped; continue; }
        juce::MemoryBlock bytes;
        if(file.getSize()<16 || file.getSize()>1024*1024 || !file.loadFileAsData(bytes) || bytes.getSize()>1024*1024
            || !juce::String::fromUTF8(static_cast<const char*>(bytes.getData()),(int)bytes.getSize()).containsIgnoreCase("[preset00]")) { ++skipped; continue; }
        const auto hash=juce::SHA256(bytes).toHexString();
        if(!seen.insert(hash).second) { ++skipped; continue; }
        auto* record=new juce::DynamicObject(); record->setProperty("sha256",hash);
        record->setProperty("title",file.getFileNameWithoutExtension().substring(0,86));
        record->setProperty("source",file.getFullPathName());
        const auto preview=file.withFileExtension(".jpg");
        if(preview.existsAsFile()) record->setProperty("preview",preview.getFullPathName());
        entries.add(juce::var(record)); ++indexed;
    }
    if(error) return juce::Result::fail("Could not finish reading the MilkDrop folder; the previous index was kept.");
    const auto linked=library.getChildFile("linked");
    const auto name=juce::SHA256(source.getFullPathName().toLowerCase().toUTF8()).toHexString()+".json";
    if(!indexed && !linked.getChildFile(name).existsAsFile()) return juce::Result::fail("No supported .milk presets found; the previous index was kept.");
    if(linked.createDirectory().failed()) return juce::Result::fail("Could not create the MilkDrop index folder.");
    auto* record=new juce::DynamicObject(); record->setProperty("schema",2);
    record->setProperty("root",source.getFullPathName()); record->setProperty("entries",entries);
    // projectM scans these paths recursively, including custom texture subfolders.
    juce::Array<juce::var> paths; paths.add(source.getFullPathName());
    const auto textures=source.getSiblingFile("Textures");
    if(textures.isDirectory()) paths.add(textures.getFullPathName());
    record->setProperty("texturePaths",paths);
    juce::TemporaryFile temporary(linked.getChildFile(name));
    if(!temporary.getFile().replaceWithText(juce::JSON::toString(juce::var(record))) || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not save the MilkDrop index; the previous index was kept.");
    return juce::Result::ok();
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
