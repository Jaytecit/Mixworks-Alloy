#include "SceneLane.h"
#include "MilkDrop.h"
#include "Workstation.h"
#include "StudioWorkspace.h"
#include "VisualKick.h"
#include "StudioAudioLink.h"
#include <set>
#include "AppWindow.h"
#include <cmath>
#include <cstring>
#if JUCE_WINDOWS
#include <process.h>
#endif

namespace mixworks
{
bool SceneLane::shareStudioAudio(bool enabled, int receiverPort)
{
    if (!enabled) { studioAudio.reset(); return true; }
    if (studioAudio) return true;
    auto link = std::make_unique<StudioAudioLink>(owner.engine.studioSpectrum, owner.engine.studioTempo);
    if (!link->start(receiverPort)) return false;
    studioAudio = std::move(link);
    return true;
}

namespace
{
juce::File rendererRoot()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("original-visualiser-runtime");
}
juce::File sceneInstall() { return rendererRoot(); }
juce::File workerScript() { return rendererRoot().getChildFile("visual_scene_worker.py"); }
int previewWidth(const Workstation& owner)
{
    const int width=(int)owner.getProperties().getWithDefault("visualPreviewWidth",1280);
    return width==640 || width==1920 ? width : 1280;
}
juce::var sceneActor(const VisualSceneClip& scene, double time, double beats = 0.)
{
    auto* value = new juce::DynamicObject(); value->setProperty("clip", scene.id); value->setProperty("scene", scene.scene);
    value->setProperty("revision",juce::String(scene.start,std::numeric_limits<double>::max_digits10,true)+"/"
        +juce::String(scene.offset,std::numeric_limits<double>::max_digits10,true));
    value->setProperty("controls",visualControlsToJson(visualControlsAt(scene,time,beats)));
    value->setProperty("motionTime",visualMotionTime(scene,time));
    value->setProperty("time", time); return juce::var(value);
}
juce::var studioActors(const std::vector<VisualSceneClip>& clips, double position, const TempoMap& clock)
{
    static constexpr const char* layerNames[] {"background", "milk", "shader", "sprite"};
    auto* layers = new juce::DynamicObject();
    const double beat = clock.beatsAt(position);
    for (const auto& clip : clips)
        if (clip.start <= position && position < clip.end() && !clip.scene.startsWith("original/"))
            layers->setProperty(layerNames[(size_t)visualLayer(clip.scene)],
                sceneActor(clip, position - clip.start + clip.offset, beat - clock.beatsAt(clip.start)));
    return juce::var(layers);
}
juce::var studioUpcoming(const std::vector<VisualSceneClip>& clips, double position, const TempoMap& clock)
{
    static constexpr const char* layerNames[] {"background", "milk", "shader", "sprite"};
    std::array<const VisualSceneClip*,4> next {};
    for (const auto& clip : clips)
        if (clip.start > position && clip.start <= position + 8. && !clip.scene.startsWith("original/"))
        {
            const auto layer = (size_t)visualLayer(clip.scene);
            if (!next[layer] || clip.start < next[layer]->start) next[layer] = &clip;
        }
    auto* actors = new juce::DynamicObject();
    for (size_t layer = 0; layer < next.size(); ++layer)
        if (const auto* clip = next[layer]) actors->setProperty(layerNames[layer],sceneActor(*clip,clip->offset,0.));
    return juce::var(actors);
}
void recordScenePaint(SceneLane& lane)
{
    // Count distinct accepted images actually drawn by either visual view, once.
    auto& facts=lane.getProperties();
    const bool playing=lane.owner.engine.playing.load();
    const auto generation=(juce::int64)lane.previewFacts["generation"];
    if(playing!=(bool)facts.getWithDefault("visualPaintWasPlaying",false)
        || generation!=(juce::int64)facts.getWithDefault("visualPaintGeneration",-1)) facts.set("visualPaintTime",0.);
    facts.set("visualPaintWasPlaying",playing); facts.set("visualPaintGeneration",generation);
    const auto sequence=(juce::int64)lane.previewFacts["sequence"];
    if (sequence==(juce::int64)facts.getWithDefault("visualPaintSequence",-1)) return;
    const double now=juce::Time::getMillisecondCounterHiRes();
    const double previous=(double)facts.getWithDefault("visualPaintTime",0.);
    facts.set("visualPaintSequence",sequence); facts.set("visualPaintTime",now);
    facts.set("visualPaintCount",(int)facts.getWithDefault("visualPaintCount",0)+1);
    if(previous>0 && playing)
    {
        const double gap=now-previous;
        facts.set("visualPaintMaxGap",std::max((double)facts.getWithDefault("visualPaintMaxGap",0.),gap));
        auto histogram=facts["visualGapHistogramCeilMs"];
        if(!histogram.isObject()) { histogram=juce::var(new juce::DynamicObject()); facts.set("visualGapHistogramCeilMs",histogram); }
        const auto bin=juce::Identifier(juce::String((juce::int64)std::ceil(gap)));
        auto* values=histogram.getDynamicObject(); values->setProperty(bin,(juce::int64)values->getProperty(bin)+1);
    }
}
class SceneWindow final : public juce::DocumentWindow
{
public:
    explicit SceneWindow(juce::Component* content) : DocumentWindow("Alloy visuals", colour::panel, DocumentWindow::allButtons)
    { setUsingNativeTitleBar(false); setContentOwned(content, true); setResizable(true, true); setResizeLimits(780, 560, 1920, 1200); centreWithSize(1000, 620); setVisible(true); }
    void closeButtonPressed() override { setVisible(false); }
};
class SceneOutputCanvas final : public juce::Component
{
public:
    explicit SceneOutputCanvas(SceneLane& source) : lane(source)
    { setTitle("Clean visuals output"); setWantsKeyboardFocus(true); setMouseCursor(juce::MouseCursor::NoCursor); }
    std::function<bool(const juce::KeyPress&)> key;
    std::function<void()> toggle;
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        if (lane.preview.isValid())
        { g.setImageResamplingQuality(juce::Graphics::highResamplingQuality); g.drawImageWithin(lane.preview,0,0,getWidth(),getHeight(),juce::RectanglePlacement::centred); recordScenePaint(lane); }
    }
    bool keyPressed(const juce::KeyPress& press) override { return key && key(press); }
    void mouseDoubleClick(const juce::MouseEvent&) override { if(toggle) toggle(); }
private:
    SceneLane& lane;
};
class SceneOutputWindow final : public AppWindow
{
public:
    explicit SceneOutputWindow(SceneLane& lane) : AppWindow(new SceneOutputCanvas(lane))
    {
        setName("Alloy visuals output"); setLookAndFeel(&lane.owner.getLookAndFeel());
        const auto desktop=juce::Desktop::getInstance().getDisplays().getTotalBounds(false);
        setResizeLimits(320,180,std::max(320,desktop.getWidth()),std::max(180,desktop.getHeight()));
        setWindowMode(WindowMode::windowed);
        setBounds(getBounds().withSizeKeepingCentre(std::min(960,getWidth()),std::min(540,getHeight())));
        auto* canvas=static_cast<SceneOutputCanvas*>(getContentComponent());
        canvas->key=[this](const juce::KeyPress& press) { return keyPressed(press); };
        canvas->toggle=[this] { toggleFullscreen(); getContentComponent()->grabKeyboardFocus(); };
    }
    void closeButtonPressed() override { setVisible(false); }
    bool keyPressed(const juce::KeyPress& press) override
    {
        if(press==juce::KeyPress::escapeKey) { setWindowMode(WindowMode::windowed); getContentComponent()->grabKeyboardFocus(); return true; }
        if(press.getKeyCode()==juce::KeyPress::F11Key) { toggleFullscreen(); getContentComponent()->grabKeyboardFocus(); return true; }
        return false;
    }
    bool showOn(const juce::Rectangle<int>& target)
    {
        const auto& displays=juce::Desktop::getInstance().getDisplays().displays;
        const auto found=std::find_if(displays.begin(),displays.end(),[&](const auto& display)
            { return display.logicalBounds.getSmallestIntegerContainer()==target; });
        if(found==displays.end()) return false;
        // AppWindow owns stable monitor bindings and DPI-aware borderless chrome.
        displayMenu();
        if(!performDisplayAction(AppWindow::monitorBase+(int)std::distance(displays.begin(),found))) return false;
        setWindowMode(WindowMode::fullscreen); setVisible(true); toFront(true); getContentComponent()->grabKeyboardFocus(); return true;
    }
};
}

void SceneLane::recordPreviewPaint() { recordScenePaint(*this); }

// Only this thread consumes the dedicated audio feed, owns the child and uses the socket.
// The audio callback never serialises, waits for a renderer or touches a scene resource.
class SceneRenderWorker final : public juce::Thread
{
public:
    SceneRenderWorker(Engine& source, int size, juce::String folder)
        : Thread("Alloy visual renderer"), engine(source), feed(source.visualSpectrum), width(size), height(size*9/16),
          spriteFolder(std::move(folder)) { startThread(juce::Thread::Priority::normal); }
    ~SceneRenderWorker() override { signalThreadShouldExit(); notify(); stopThread(5000); }
    void submit(juce::var value, const std::vector<VisualSceneClip>& scenes, const TempoMap& clock)
    {
        const juce::ScopedLock guard(mutex);
        request=std::move(value); timeline=scenes; tempo=clock; ++revision; notify();
    }
    void result(juce::Image& frame, juce::var& facts, juce::String& text)
    { const juce::ScopedLock guard(mutex); frame = image; facts = metadata; text = message; }
    int renderWidth() const { return width; }
    const juce::String& renderSpriteFolder() const { return spriteFolder; }
private:
    Engine& engine;
    SpectrumFeed& feed;
    const int width, height;
    const juce::String spriteFolder;
    juce::CriticalSection mutex;
    juce::var request, metadata;
    std::vector<VisualSceneClip> timeline;
    TempoMap tempo;
    juce::Image image;
    juce::String message {"Starting visual renderer..."};
    juce::uint64 revision = 0;
    juce::uint64 receivedFrames = 0;
    juce::String pausedKey;
    std::array<float, SpectrumFeed::capacity> left {}, right {};
    std::array<double, SpectrumFeed::capacity> rates {};
    std::array<SpectrumFeed::Clock, SpectrumFeed::capacity> clocks {};
    std::array<float, 2048> pcm {};
    VisualKickDetector kicks;
    double rate = 44100;
    bool drain()
    {
        const int count = feed.read(left.data(), right.data(), rates.data(), SpectrumFeed::capacity, clocks.data());
        if (count <= 0) return false;
        for (int i = 0; i < count; ++i)
        {
            const auto index = (size_t)i;
            kicks.push(left[index], right[index], clocks[index].position, rates[index], clocks[index].revision);
        }
        rate = rates[(size_t)count - 1]; const int n = std::min(count, (int)pcm.size());
        if (n < (int)pcm.size()) std::move(pcm.begin() + n, pcm.end(), pcm.begin());
        double l = 0, r = 0;
        for (int i = count - n; i < count; ++i) { l += left[(size_t)i] * left[(size_t)i]; r += right[(size_t)i] * right[(size_t)i]; }
        const auto& channel = l >= r ? left : right;
        std::copy(channel.begin() + count - n, channel.begin() + count, pcm.end() - n);
        return true;
    }
    void fail(const juce::String& text)
    {
        auto* failure = new juce::DynamicObject(); failure->setProperty("ok", false); failure->setProperty("error", text);
        const juce::ScopedLock guard(mutex); message = text; image = {}; metadata = juce::var(failure);
    }
    bool read(juce::StreamingSocket& socket, void* data, int bytes)
    {
        int done = 0; const auto deadline = juce::Time::getMillisecondCounterHiRes() + 60000;
        while (done < bytes && !threadShouldExit() && juce::Time::getMillisecondCounterHiRes() < deadline)
        {
            drain();
            const int ready = socket.waitUntilReady(true, 10); if (ready < 0) return false;
            if (ready == 0) continue;
            const int n = socket.read(static_cast<char*>(data) + done, bytes - done, false); if (n <= 0) return false;
            done += n;
        }
        return done == bytes;
    }
    void run() override
    {
        juce::ScopedNoDenormals noDenormals;
        const auto python = rendererRoot().getSiblingFile("visual-python").getChildFile("python.exe");
        if (!python.existsAsFile() || !workerScript().existsAsFile() || !sceneInstall().isDirectory())
        { fail("Visual renderer unavailable. Keep the scene runtime with this application; including its visual-python folder."); return; }
        const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Alloy-visuals-" + juce::Uuid().toString());
        if (folder.createDirectory().failed()) { fail("Could not create a visual renderer workspace."); return; }
        const auto readyFile = folder.getChildFile("ready.json"); const auto token = juce::Uuid().toString();
        juce::ChildProcess child; juce::StreamingSocket socket;
        juce::StringArray arguments {python.getFullPathName(), "-B", workerScript().getFullPathName(), "--renderer-root", rendererRoot().getFullPathName(),
            "--install", sceneInstall().getFullPathName(), "--ready", readyFile.getFullPathName(), "--token", token,
            "--width", juce::String(width), "--height", juce::String(height),
           #if JUCE_WINDOWS
            "--owner-pid", juce::String(_getpid())
           #else
            "--owner-pid", "0"
           #endif
        };
        arguments.add("--original");
        arguments.add("--milkdrop-library"); arguments.add(milkDropLibraryRoot().getFullPathName());
        if(spriteFolder.isNotEmpty()) { arguments.add("--sprite-folder"); arguments.add(spriteFolder); }
        bool started = child.start(arguments, 0);
        if (!started) fail("Could not start the visual renderer.");
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 15000;
        while (started && !readyFile.existsAsFile() && child.isRunning() && !threadShouldExit() && juce::Time::getMillisecondCounterHiRes() < deadline) { drain(); wait(50); }
        const int port = (int) juce::JSON::parse(readyFile)["port"];
        if (started && !threadShouldExit() && port > 0 && port <= 65535 && socket.connect("127.0.0.1", port, 1000))
        {
            juce::uint64 sent = 0;
            juce::var control;
            std::vector<VisualSceneClip> renderScenes;
            TempoMap renderClock;
            double nextFrame = 0, lastRequest = 0;
            while (!threadShouldExit() && child.isRunning())
            {
                const double now=juce::Time::getMillisecondCounterHiRes();
                if(now<nextFrame) { wait(std::max(1,(int)(nextFrame-now))); continue; }
                const auto interval = control["studioLayers"].isObject() ? 1000. / 30. : 1000. / 60.;
                if(now-nextFrame>interval) nextFrame=now;
                nextFrame+=interval;
                { const juce::ScopedLock guard(mutex); if (sent != revision)
                    { sent=revision; control=request.clone(); renderScenes=timeline; renderClock=tempo; } }
                if (control.isVoid()) { wait(25); continue; }
                // UI publishes immutable scene/control snapshots. The renderer
                // samples the device's actual atomic clock on its own schedule;
                // it never extrapolates music or reads the mutable project.
                const auto generation=engine.visualClockRevision.load();
                const double position=engine.position.load(); const bool playing=engine.playing.load();
                if ((juce::int64)control["generation"]!=(juce::int64)generation || generation!=engine.visualClockRevision.load()) continue;
                auto next=control.clone(); auto* frameObject=next.getDynamicObject();
                if (control["studioLayers"].isObject())
                {
                    frameObject->setProperty("studioLayers", studioActors(renderScenes, position, renderClock));
                    frameObject->setProperty("preload", studioUpcoming(renderScenes, position, renderClock));
                }
                else
                {
                const auto sceneFrame=visualFrameAt(renderScenes,position); juce::Array<juce::var> actors;
                if(sceneFrame.outgoing) actors.add(sceneActor(*sceneFrame.outgoing,position-sceneFrame.outgoing->start+sceneFrame.outgoing->offset));
                if(sceneFrame.incoming) actors.add(sceneActor(*sceneFrame.incoming,position-sceneFrame.incoming->start+sceneFrame.incoming->offset));
                frameObject->setProperty("scenes",actors); frameObject->setProperty("mix",sceneFrame.mix);
                frameObject->setProperty("transition",sceneFrame.incoming ? sceneFrame.incoming->transition : "dissolve");
                frameObject->setProperty("transitionSeconds",sceneFrame.incoming && sceneFrame.outgoing ? std::max(0.,sceneFrame.outgoing->end()-sceneFrame.incoming->start) : 0.);
                frameObject->setProperty("pivot",sceneFrame.incoming ? sceneFrame.incoming->pivot : .5); frameObject->removeProperty("preload");
                if(!sceneFrame.incoming) for(const auto& upcoming:renderScenes) if(upcoming.start>position)
                    { frameObject->setProperty("preload",sceneActor(upcoming,upcoming.offset)); break; }
                }
                frameObject->setProperty("position",position); frameObject->setProperty("playing",playing);
                frameObject->setProperty("beat",renderClock.beatsAt(position)); frameObject->setProperty("bpm",renderClock.bpmAt(position));
                if(!playing)
                {
                    const auto key=juce::JSON::toString(next,true,std::numeric_limits<double>::max_digits10+6);
                    if(key==pausedKey) continue;
                    pausedKey=key;
                }
                else pausedKey.clear();
                frameObject->setProperty("sequence", (juce::int64)(receivedFrames + 1));
                if (!drain() && !(bool)next["playing"]) pcm.fill(0);
                if (generation != engine.visualClockRevision.load()) continue;
                juce::Array<juce::var> events;
                if (playing && kicks.revision() == generation)
                    for (double time : kicks.events())
                        if (time >= 0. && time <= position && position - time <= .32)
                        {
                            const double beat = renderClock.beatsAt(time);
                            const double distance = std::abs(beat - std::round(beat));
                            // Grid proximity is a small accent on confirmed audio.
                            const double strength = 1. - .06 * std::min(1., distance / .12);
                            juce::Array<juce::var> event {time, strength};
                            events.add(juce::var(std::move(event)));
                        }
                frameObject->setProperty("kicks", juce::var(std::move(events)));
                auto* object = next.getDynamicObject(); object->setProperty("token", token); object->setProperty("rate", rate);
                object->setProperty("pcm", juce::Base64::toBase64(pcm.data(), pcm.size() * sizeof(float)));
                // Preserve derived clocks as well as the opaque geometry revision.
                // JUCE uses fixed notation above 1e-5, requiring fractional zeros
                // in addition to max_digits10 significant digits on this wire.
                const auto wire = juce::JSON::toString(next, true, std::numeric_limits<double>::max_digits10 + 6) + "\n"; const auto* data = wire.toRawUTF8(); int written = 0;
                while (written < wire.getNumBytesAsUTF8() && !threadShouldExit())
                { const int n = socket.write(data + written, (int)wire.getNumBytesAsUTF8() - written); if (n <= 0) break; written += n; }
                unsigned char header[8];
                if (written != wire.getNumBytesAsUTF8() || !read(socket, header, 8)) { if (!threadShouldExit()) fail("Visual renderer stopped responding. Close and reopen Visuals to retry."); break; }
                const auto jsonSize = juce::ByteOrder::bigEndianInt(header), imageSize = juce::ByteOrder::bigEndianInt(header + 4);
                if (jsonSize > 65536 || imageSize > (unsigned int)(width * height * 4 + 65536)) { fail("Invalid visual renderer response."); break; }
                juce::MemoryBlock json(jsonSize), pixels(imageSize);
                if (!read(socket, json.getData(), (int)jsonSize) || !read(socket, pixels.getData(), (int)imageSize))
                { if (!threadShouldExit()) fail("Visual renderer returned an incomplete frame. Close and reopen Visuals to retry."); break; }
                auto facts = juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(json.getData()), (int)jsonSize));
                if ((int)facts["protocol"] != 6) { fail("The visual renderer does not match this build. Keep visualiser-runtime with the application."); break; }
                juce::Image frame;
                if(imageSize)
                {
                    if (!facts["width"].isInt() || !facts["height"].isInt() || (int)facts["width"]!=width || (int)facts["height"]!=height || facts["pixelFormat"].toString()!="bgra8-bottom-up"
                        || imageSize!=(unsigned int)(width*height*4)) { fail("Invalid visual pixel layout or size."); break; }
                    frame=juce::Image(juce::Image::ARGB,width,height,false);
                    juce::Image::BitmapData bitmap(frame,juce::Image::BitmapData::writeOnly);
                    const auto* raw=static_cast<const juce::uint8*>(pixels.getData());
                    for(int y=0;y<height;++y)
                    {
                        const auto* row=raw+(height-1-y)*width*4;
                        if constexpr(juce::PixelARGB::indexB==0 && juce::PixelARGB::indexG==1 && juce::PixelARGB::indexR==2 && juce::PixelARGB::indexA==3)
                            std::memcpy(bitmap.getLinePointer(y),row,(size_t)width*4);
                        else for(int x=0;x<width;++x) bitmap.setPixelColour(x,y,juce::Colour(row[x*4+2],row[x*4+1],row[x*4],row[x*4+3]));
                    }
                }
                if (!facts.isObject() || !facts["ok"].isBool() || ((bool)facts["ok"] && !(bool)facts["empty"] && !(bool)facts["initialised"] && !(bool)facts["presented"] && !frame.isValid()))
                { fail("Visual renderer returned an invalid frame."); break; }
                const juce::ScopedLock guard(mutex);
                // Startup intentionally discards its initial clock/PCM. Permit
                // the current paused request once more after readiness.
                if ((bool)facts["initialised"]) pausedKey.clear();
                if (auto* received=facts.getDynamicObject())
                {
                    received->setProperty("transportFrames",(juce::int64)++receivedFrames); received->setProperty("transportPlaying",(bool)next["playing"]);
                    received->setProperty("requestGapMs",lastRequest>0 ? now-lastRequest : 0.); lastRequest=now;
                }
                metadata = facts;
                image = (bool)facts["ok"] ? frame : juce::Image();
                message = !(bool)facts["ok"] ? "Scene could not render: " + facts["error"].toString()
                    : (bool)facts["initialised"] ? "Renderer ready; preparing scene..."
                    : (bool)facts["empty"] ? "Place a scene on the Visuals lane, then play your mix." : "Mix audio / timeline sync";
                if(facts["mediaError"].toString().isNotEmpty()) message="Image: "+facts["mediaError"].toString();
                else if(const auto* errors=facts["spriteErrors"].getDynamicObject();errors && errors->getProperties().size()>0)
                    message="Some Sprites could not load; showing available images.";
            }
            if (!threadShouldExit() && !child.isRunning()) fail("Visual renderer exited. Close and reopen Visuals to retry.");
        }
        else if (started && !threadShouldExit()) fail("Visual renderer did not become ready.");
        socket.close();
        if (started && !child.waitForProcessToFinish(1000)) { child.kill(); child.waitForProcessToFinish(1000); }
        // This UUID folder was created solely by this worker; no caller/user paths enter cleanup.
        folder.deleteRecursively();
    }
};

class MilkDropIndexWorker final : public juce::Thread
{
public:
    explicit MilkDropIndexWorker(juce::File source) : Thread("MilkDrop folder index"), folder(std::move(source))
    { startThread(juce::Thread::Priority::low); }
    ~MilkDropIndexWorker() override { signalThreadShouldExit(); stopThread(-1); }
    juce::File folder;
    juce::Result outcome=juce::Result::ok();
    int indexed=0, skipped=0;
    std::vector<VisualSceneEntry> entries;
private:
    void run() override
    {
        outcome=linkMilkDropFolder(folder,milkDropLibraryRoot(),indexed,skipped,[this]{return threadShouldExit();});
        if(outcome.wasOk() && !threadShouldExit()) entries=discoverMilkDropScenes(milkDropLibraryRoot());
    }
};

// One bounded child at a time. Thumbnails never touch the live mix or audio feed.
class MilkDropPreviewWorker final : public juce::Thread
{
public:
    MilkDropPreviewWorker() : Thread("MilkDrop library previews") { startThread(juce::Thread::Priority::low); }
    ~MilkDropPreviewWorker() override { signalThreadShouldExit(); notify(); stopThread(3000); }
    static juce::File imageFile(const juce::String& key)
    { return milkDropLibraryRoot().getChildFile("previews").getChildFile(key.fromFirstOccurrenceOf("/",false,false)+"-v1.jpg"); }
    void submit(std::vector<juce::String> keys)
    { const juce::ScopedLock guard(mutex); pending=std::move(keys); notify(); }
    void pause(bool value) { paused=value; notify(); }
    bool take(juce::String& key, juce::String& error)
    {
        const juce::ScopedLock guard(mutex); if(completed.empty()) return false;
        key=completed.front().first; error=completed.front().second; completed.erase(completed.begin()); return true;
    }
private:
    juce::CriticalSection mutex;
    std::vector<juce::String> pending;
    std::vector<std::pair<juce::String,juce::String>> completed;
    std::atomic<bool> paused{true};
    void run() override
    {
        while(!threadShouldExit())
        {
            juce::String key;
            { const juce::ScopedLock guard(mutex); if(!paused && !pending.empty()) { key=pending.front(); pending.erase(pending.begin()); } }
            if(key.isEmpty()) { wait(100); continue; }
            const auto out=imageFile(key);
            if(out.existsAsFile()) continue;
            juce::ChildProcess child;
            const auto python=rendererRoot().getSiblingFile("visual-python").getChildFile("python.exe");
            const bool started=child.start(juce::StringArray{python.getFullPathName(),"-B",rendererRoot().getChildFile("milkdrop_preview.py").getFullPathName(),
                "--library",milkDropLibraryRoot().getFullPathName(),"--key",key,"--out",out.getFullPathName()});
            const auto deadline=juce::Time::getMillisecondCounterHiRes()+20000.;
            while(started && child.isRunning() && !threadShouldExit() && !paused && juce::Time::getMillisecondCounterHiRes()<deadline) wait(50);
            if(started && child.isRunning()) { child.kill(); child.waitForProcessToFinish(1000); }
            if(threadShouldExit()) return;
            const juce::ScopedLock guard(mutex);
            if(paused) { pending.insert(pending.begin(),key); continue; }
            auto error=out.existsAsFile()?juce::String():child.readAllProcessOutput().trim().substring(0,1200);
            if(!out.existsAsFile() && error.isEmpty()) error="Preview unavailable. Try Live mix or rescan the folder.";
            completed.emplace_back(key,error);
        }
    }
};

class ScenePreviewTile final : public juce::Component
{
public:
    ScenePreviewTile(juce::Image picture, const juce::String& title,
                     std::function<void()> inspectScene, juce::String placeholder="Reference image unavailable")
        : image(std::move(picture)), inspect(std::move(inspectScene)), emptyText(std::move(placeholder))
    {
        addAndMakeVisible(name);
        name.setButtonText(title); name.setTooltip(title + " — inspect reference image");
        name.onClick=inspect;
    }
    void resized() override
    {
        auto r=getLocalBounds().reduced(4);
        name.setBounds(r.removeFromBottom(30)); imageArea=r;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff171b22));
        if(image.isValid()) g.drawImageWithin(image,imageArea.getX(),imageArea.getY(),imageArea.getWidth(),imageArea.getHeight(),juce::RectanglePlacement::centred);
        else { g.setColour(colour::muted); g.drawFittedText(emptyText,imageArea,juce::Justification::centred,2); }
    }
    void mouseUp(const juce::MouseEvent& e) override { if(imageArea.contains(e.position.toInt()) && inspect) inspect(); }
private:
    juce::Image image; juce::TextButton name; juce::Rectangle<int> imageArea;
    std::function<void()> inspect;
    juce::String emptyText;
};

class SceneBrowser final : public juce::Component, public juce::ListBoxModel, public juce::DragAndDropContainer
{
public:
    explicit SceneBrowser(SceneLane& l) : lane(l)
    {
        addAndMakeVisible(viewMode); addAndMakeVisible(reviewCount);
        addAndMakeVisible(librarySource); librarySource.addItem("Originals",1); librarySource.addItem("MilkDrop",2);
        librarySource.setSelectedId(1,juce::dontSendNotification); librarySource.setTitle("Visual library collection");
        librarySource.setComponentID("visual-library-source");
        librarySource.onChange=[this] { page=0; search.clear(); filter(); resized(); help.setText(librarySource.getSelectedId()==2 ? "Link a folder to browse presets. Previews use demo audio and pause while your mix plays. Choose the same folder again to rescan." : "Choose a scene and look, then add it at the playhead or drag it onto the visual lane.",juce::dontSendNotification); };
        addAndMakeVisible(previousPage); addAndMakeVisible(nextPage);
        previousPage.setButtonText("Previous"); nextPage.setButtonText("Next");
        previousPage.onClick=[this] { if(page>0) { --page; rebuildGallery(); } };
        nextPage.onClick=[this] { if((page+1)*pageSize<(int)visible.size()) { ++page; rebuildGallery(); } };
        addAndMakeVisible(linkFolder); linkFolder.setButtonText("Link MilkDrop folder...");
        linkFolder.setComponentID("visual-milkdrop-link");
        linkFolder.setTooltip("Index all .milk presets in a folder and its subfolders. No presets or textures are copied or moved. Choose the same folder again to rescan.");
        linkFolder.onClick=[this]
        {
            milkDropChooser=std::make_unique<juce::FileChooser>("Link your MilkDrop collection (choose again to rescan)",juce::File());
            milkDropChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,
                [safe=juce::Component::SafePointer<SceneBrowser>(this)](const juce::FileChooser& chooser)
                {
                    if(!safe || !chooser.getResult().isDirectory()) return;
                    safe->librarySource.setSelectedId(2,juce::sendNotificationSync);
                    safe->lane.linkMilkDrop(chooser.getResult());
                    safe->help.setText(safe->lane.owner.status,juce::dontSendNotification);
                });
        };
        addAndMakeVisible(gallery);
        gallery.setViewedComponent(&galleryContent,false); gallery.setScrollBarsShown(true,false);
        viewMode.addItem("Scene overview",1); viewMode.addItem("Selected scene preview",2); viewMode.addItem("Live mix",3);
        viewMode.setSelectedId(1,juce::dontSendNotification); viewMode.setTitle("Scene library view");
        viewMode.setComponentID("visual-library-view");
        viewMode.onChange=[this] { updateView(); };
        reviewCount.setColour(juce::Label::textColourId,colour::muted);
        addAndMakeVisible(search); addAndMakeVisible(list); addAndMakeVisible(add); addAndMakeVisible(match); addAndMakeVisible(help); addAndMakeVisible(quality);
        addAndMakeVisible(outputButton); addAndMakeVisible(outputDisplay);
        addAndMakeVisible(imageButton); addAndMakeVisible(clearImage);
        addAndMakeVisible(spriteFolderButton); addAndMakeVisible(resetSpriteFolder);
        spriteFolderButton.setComponentID("visual-sprites-folder");
        if(lane.owner.workspacePreference!=juce::File())
        {
            spriteFolderPreference=lane.owner.workspacePreference.getSiblingFile("visual-sprites-folder.txt");
            const auto saved=spriteFolderPreference.loadFileAsString();
            if(juce::File::isAbsolutePath(saved)) spriteFolder=saved;
        }
        resetSpriteFolder.setButtonText("Reset");
        resetSpriteFolder.setTooltip("Return to the bundled Sprites folder.");
        resetSpriteFolder.onClick=[this] { saveSpriteFolder({}); };
        spriteFolderButton.onClick=[this]
        {
            const auto start=spriteFolder.isNotEmpty() ? juce::File(spriteFolder) : sceneInstall().getChildFile("Sprites");
            spriteFolderChooser=std::make_unique<juce::FileChooser>("Choose your Sprites folder",start);
            spriteFolderChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,
                [safe=juce::Component::SafePointer<SceneBrowser>(this)](const juce::FileChooser& chooser)
                {
                    if(safe && chooser.getResult().isDirectory()) safe->saveSpriteFolder(chooser.getResult().getFullPathName());
                });
        };
        showSpriteFolder();
        addAndMakeVisible(importMilkDrop);
        importMilkDrop.setButtonText("Import MilkDrop..."); importMilkDrop.setComponentID("visual-milkdrop-import");
        importMilkDrop.setTooltip("Import a .milk preset into your local library. Its supporting textures stay in their original folders. MilkDrop currently supports live playback; seeks restart its feedback.");
        importMilkDrop.onClick=[this]
        {
            milkDropChooser=std::make_unique<juce::FileChooser>("Import a MilkDrop preset",juce::File(),"*.milk");
            milkDropChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
                [safe=juce::Component::SafePointer<SceneBrowser>(this)](const juce::FileChooser& chooser)
                {
                    if(!safe || !chooser.getResult().existsAsFile()) return;
                    if(safe->lane.importMilkDrop(chooser.getResult())) { safe->librarySource.setSelectedId(2,juce::sendNotificationSync); safe->filter(); }
                    safe->help.setText(safe->lane.owner.status,juce::dontSendNotification);
                });
        };
        imageButton.setComponentID("visual-image-open"); clearImage.setComponentID("visual-image-clear");
        imageButton.setTooltip("Choose a transparent sprite to pin it in compatible Originals and MilkDrop presets. Clear returns to automatic sprite rotation every 64 bars. Opaque images supply background artwork.");
        clearImage.setButtonText("Clear"); clearImage.onClick=[this] { lane.setImage({}); };
        imageButton.onClick=[this]
        {
            imageChooser=std::make_unique<juce::FileChooser>("Choose an image to transform inside the scenes",juce::File(),"*.png;*.jpg;*.jpeg;*.bmp;*.webp;*.tif;*.tiff");
            imageChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
                [safe=juce::Component::SafePointer<SceneBrowser>(this)](const juce::FileChooser& chooser)
                { if (safe && chooser.getResult().existsAsFile()) safe->lane.setImage(chooser.getResult().getFullPathName()); });
        };
        outputButton.setComponentID("visual-output-open"); outputButton.setButtonText("Open fullscreen output");
        outputButton.setTooltip("Visuals only on the chosen monitor. Escape returns to a movable window; F11 or double-click toggles fullscreen. Uses the selected visual quality.");
        outputDisplay.setComponentID("visual-output-display"); outputDisplay.setTitle("Visual output display");
        outputDisplay.setTooltip("Choose the display for the clean visual output. The other monitor is selected initially when available.");
        outputDisplay.onChange=[this] { if(output && output->isVisible()) showOutput(); };
        outputButton.onClick=[this]
        {
            refreshDisplays();
            if(output && output->isVisible()) output->closeButtonPressed(); else showOutput();
            updateOutput();
        };
        refreshDisplays();
        quality.setComponentID("visual-quality"); quality.setTitle("Visuals quality");
        quality.addItem("360p / Faster",640); quality.addItem("720p / Sharp",1280); quality.addItem("1080p / Full HD",1920);
        quality.setSelectedId(previewWidth(lane.owner),juce::dontSendNotification);
        quality.setTooltip("Shared preview and clean-output resolution. Higher quality uses more graphics power. Changing it resets scene feedback; music playback continues.");
        quality.onChange=[this] { lane.owner.getProperties().set("visualPreviewWidth",quality.getSelectedId()); };
        search.setTextToShowWhenEmpty("Search scenes or families", colour::muted); search.onTextChange = [this] { page=0; filter(); };
        search.setTitle("Search visual scenes"); list.setModel(this); list.setRowHeight(30); list.setTitle("Visual scene library"); list.setComponentID("visual-scene-list");
        search.setComponentID("visual-scene-search");
        add.setButtonText("Add at playhead"); add.onClick = [this] { addSelected(); };
        addAndMakeVisible(presetChoice); presetChoice.setTitle("Preset for added scene");
        presetChoice.setComponentID("visual-scene-preset");
        presetChoice.setTooltip("Compare a reference image for each look, then add, double-click or drag the scene. Right-click a timeline scene to change its preset or image/view method.");
        presetChoice.onChange=[this] { loadSelectedReference(); if(selectedIndex()>=0) previewSelection(); repaint(); };
        match.setComponentID("visual-match-mix");
        match.setButtonText("Match mix");
        match.onClick = [this]
        {
            const juce::String message = "Matching visuals to the mix...";
            showMatchStatus(message, true);
            lane.owner.setStatus(message);
            // Give the browser a paint before the scene planner runs.
            juce::Timer::callAfterDelay(60, [safe = juce::Component::SafePointer<SceneBrowser>(this)]
            { if (safe) safe->lane.matchMix(); });
        };
        addAndMakeVisible(sceneSources); sceneSources.setTitle("Automatic scene sources"); sceneSources.setComponentID("visual-scene-sources");
        sceneSources.addItem("Originals",1); sceneSources.addItem("MilkDrop",2); sceneSources.addItem("Both",3);
        sceneSources.setSelectedId((int)visualSceneSourceFromId((int)lane.owner.getProperties().getWithDefault("visualSceneSource",1)),juce::dontSendNotification);
        sceneSources.setTooltip("Scene sources for Match mix and Auto Arrange. Both balances originals and imported MilkDrop presets. Images apply to original scenes.");
        sceneSources.onChange=[this] { lane.owner.getProperties().set("visualSceneSource",sceneSources.getSelectedId()); };
        match.setTooltip("Create scenes across the existing music, with blends over its transitions. This replaces the visual lane; Undo restores it.");
        help.setText("Choose a scene and look, then add it at the playhead or drag it onto the visual lane.", juce::dontSendNotification);
        help.setColour(juce::Label::textColourId, colour::muted); filter(); setSize(1000, 620);
    }
    void setReplaceTarget(const juce::String& id)
    {
        replacement=id; add.setButtonText(id.isEmpty() ? "Add at playhead" : "Replace this scene");
        if(id.isNotEmpty()) viewMode.setSelectedId(3);
        const auto target=std::find_if(lane.owner.engine.project.visualScenes.begin(),lane.owner.engine.project.visualScenes.end(),[&](const auto& c){return c.id==id;});
        const auto scope=target==lane.owner.engine.project.visualScenes.end() ? juce::String() : target->title+" / "+timeText(target->start)+" to "+timeText(target->end())+". ";

        presetChoice.setTitle(id.isEmpty() ? "Preset for added scene" : "Preset for replacement scene");
        help.setText(id.isEmpty() ? "Choose a scene and look, then add it at the playhead or drag it onto the visual lane."
            : scope+"Replace in place, including during playback. Placement, duration, joins and shared image stay; scene controls and animation reset to the chosen look.",juce::dontSendNotification);
    }
    ~SceneBrowser() override { list.setModel(nullptr); gallery.setViewedComponent(nullptr,false); }
    void showMatchStatus(const juce::String& message, bool working = false)
    {
        match.setButtonText(working ? "Matching visuals..." : "Match mix");
        match.setEnabled(!working);
        help.setText(message,juce::dontSendNotification);
    }
    void indexFinished(const juce::String& message)
    {
        previewErrors.clear();
        for(auto it=thumbnails.begin();it!=thumbnails.end();) if(isMilkDropKey(it->first)) it=thumbnails.erase(it); else ++it;
        filter(); help.setText(message,juce::dontSendNotification);
    }
    bool outputVisible() const { return output && output->isVisible(); }
    const juce::String& currentSpriteFolder() const { return spriteFolder; }
    bool needsLiveRenderer() const { return outputVisible() || (isShowing() && viewMode.getSelectedId()==3); }
    juce::var nativeViews() const
    {
        juce::Array<juce::var> views;
        const auto addView = [&](const char* name, const juce::Component& component, juce::Rectangle<int> area)
        {
            auto* peer = component.getPeer();
            if (!peer || !component.isShowing() || area.isEmpty()) return;
            const auto bounds = peer->getComponent().getLocalArea(&component, area);
            const double scale = peer->getPlatformScaleFactor();
            auto* view = new juce::DynamicObject(); view->setProperty("id", name);
            view->setProperty("parent", (juce::int64)(juce::pointer_sized_int)peer->getNativeHandle());
            view->setProperty("x", juce::roundToInt(bounds.getX()*scale)); view->setProperty("y", juce::roundToInt(bounds.getY()*scale));
            view->setProperty("width", juce::roundToInt(bounds.getWidth()*scale)); view->setProperty("height", juce::roundToInt(bounds.getHeight()*scale));
            views.add(juce::var(view));
        };
        if(viewMode.getSelectedId()==3) addView("preview", *this, canvas.withHeight(std::max(0,canvas.getHeight()-50)));
        if (outputVisible()) addView("output", *output->getContentComponent(), output->getContentComponent()->getLocalBounds());
        return views;
    }
    void updateOutput()
    {
        if(previewWorker)
        {
            previewWorker->pause(!isShowing() || librarySource.getSelectedId()!=2 || viewMode.getSelectedId()==3 || outputVisible() || lane.owner.engine.playing.load());
            juce::String key,error; bool updated=false;
            while(previewWorker->take(key,error))
            {
                previewErrors[key]=error; thumbnails.erase(key); updated=true;
                if(selectedIndex()>=0 && lane.catalogue()[(size_t)selectedIndex()].key==key) loadSelectedReference();
            }
            if(updated) { rebuildGallery(); repaint(); }
        }
        sceneSources.setSelectedId((int)visualSceneSourceFromId((int)lane.owner.getProperties().getWithDefault("visualSceneSource",1)),juce::dontSendNotification);
        const auto path=lane.owner.engine.project.visualImage;
        const int sprites=(int)lane.previewFacts["spriteCount"];
        imageButton.setButtonText(path.isEmpty() ? (sprites>0 ? juce::String((int)lane.previewFacts["backgroundCount"])+" backgrounds / "
            +juce::String((int)lane.previewFacts["cutoutCount"])+" cutouts" : "Choose image for scenes...") : "Image: "+juce::File(path).getFileName());
        clearImage.setEnabled(path.isNotEmpty());
        refreshDisplays();
        if(outputVisible() && output->windowMode()==WindowMode::fullscreen)
        {
            const auto actual=std::find(displayAreas.begin(),displayAreas.end(),output->getScreenBounds());
            if(actual!=displayAreas.end()) outputDisplay.setSelectedId((int)std::distance(displayAreas.begin(),actual)+1,juce::dontSendNotification);
        }
        outputButton.setButtonText(outputVisible() ? "Close output" : "Open fullscreen output");
        outputButton.setEnabled(!displayAreas.empty());
        if(outputVisible())
        {
            if (outputImage!=lane.preview) { outputImage=lane.preview; output->getContentComponent()->repaint(); }
        }
        else outputImage={};
    }
    void filter()
    {
        const int previous=selectedIndex();
        visible.clear(); for (int i = 0; i < (int)lane.catalogue().size(); ++i)
            if ((isMilkDropKey(lane.catalogue()[(size_t)i].key)==(librarySource.getSelectedId()==2))
                && (lane.catalogue()[(size_t)i].title.containsIgnoreCase(search.getText())
                 || lane.catalogue()[(size_t)i].style.containsIgnoreCase(search.getText())
                 || lane.catalogue()[(size_t)i].family.containsIgnoreCase(search.getText())
                 || lane.catalogue()[(size_t)i].family.replaceCharacter('-',' ').containsIgnoreCase(search.getText()))) visible.push_back(i);
        list.updateContent();
        auto found=std::find(visible.begin(),visible.end(),previous);
        if(!visible.empty()) list.selectRow(found==visible.end()?0:(int)std::distance(visible.begin(),found)); else list.deselectAllRows();
        rebuildGallery(); selectedRowsChanged(list.getSelectedRow()); repaint();
    }
    int selectedIndex() const { const int row=list.getSelectedRow(); return row>=0 && row<(int)visible.size()?visible[(size_t)row]:-1; }
    void selectedRowsChanged(int) override
    {
        const int index=selectedIndex(); selectedImage={};
        presetChoice.clear(juce::dontSendNotification);
        if(index>=0)
        {
            const auto& looks=lane.catalogue()[(size_t)index].presets;
            for(size_t i=0;i<looks.size();++i) presetChoice.addItem(looks[i].title,(int)i+1);
            presetChoice.setSelectedId(1,juce::dontSendNotification);
        }
        presetChoice.setEnabled(presetChoice.getNumItems()>0);
        loadSelectedReference();
        add.setEnabled(index>=0);
        if(librarySource.getSelectedId()==2 && list.getSelectedRow()>=0 && page!=list.getSelectedRow()/pageSize)
        { page=list.getSelectedRow()/pageSize; rebuildGallery(); }
        repaint();
    }
    void previewSelection()
    {
        // Browsing a replacement must not tear down the running renderer or
        // hide the actual programme behind a static reference image.
        if(replacement.isEmpty()) viewMode.setSelectedId(2);
    }
    void listBoxItemClicked(int, const juce::MouseEvent&) override { previewSelection(); }
    int getNumRows() override { return (int)visible.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int h, bool selected) override
    {
        if (row < 0 || row >= getNumRows()) return;
        if (selected) g.fillAll(colour::orange.withAlpha(.2f));
        g.setColour(selected ? colour::text : colour::muted); g.setFont(14.f);
        g.drawText(lane.catalogue()[(size_t)visible[(size_t)row]].title, 10, 0, width - 16, h, juce::Justification::centredLeft);
    }
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rows) override
    {
        if(!rows.size() || rows[0]>=getNumRows()) return {};
        const auto& entry=lane.catalogue()[(size_t)visible[(size_t)rows[0]]];
        const int preset=presetChoice.getSelectedId()-1;
        auto description="alloy-scene:"+entry.key;
        if(preset>=0 && preset<(int)entry.presets.size()) description+="|"+entry.presets[(size_t)preset].id;
        return description;
    }
    void listBoxItemDoubleClicked(int, const juce::MouseEvent&) override { addSelected(); }
    juce::String replacement;
    void addSelected()
    {
        const int row = list.getSelectedRow(); if (row < 0 || row >= getNumRows()) return;
        const auto key=lane.catalogue()[(size_t)visible[(size_t)row]].key;
        if(replacement.isEmpty()) lane.place(key,lane.owner.engine.position.load(),presetChoice.getSelectedId()-1);
        else if(lane.replaceScene(replacement,key,presetChoice.getSelectedId()-1)) viewMode.setSelectedId(3);
        help.setText(lane.owner.status, juce::dontSendNotification);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced(14); auto library = r.removeFromLeft(250); r.removeFromLeft(14);
        librarySource.setBounds(library.removeFromTop(30)); library.removeFromTop(8);
        search.setBounds(library.removeFromTop(30)); library.removeFromTop(8);
        const bool milk=librarySource.getSelectedId()==2;
        linkFolder.setVisible(milk); importMilkDrop.setVisible(milk); imageButton.setVisible(!milk); clearImage.setVisible(!milk);
        spriteFolderButton.setVisible(!milk); resetSpriteFolder.setVisible(!milk);
        if(milk) { linkFolder.setBounds(library.removeFromTop(30)); library.removeFromTop(5); importMilkDrop.setBounds(library.removeFromTop(30)); library.removeFromTop(8); }
        else
        {
            auto imageRow=library.removeFromTop(30); clearImage.setBounds(imageRow.removeFromRight(52)); imageRow.removeFromRight(4); imageButton.setBounds(imageRow);
            library.removeFromTop(5);
            auto folderRow=library.removeFromTop(30); resetSpriteFolder.setBounds(folderRow.removeFromRight(52)); folderRow.removeFromRight(4);
            spriteFolderButton.setBounds(folderRow); library.removeFromTop(8);
        }
        outputButton.setBounds(library.removeFromBottom(30)); library.removeFromBottom(5);
        outputDisplay.setBounds(library.removeFromBottom(30)); library.removeFromBottom(10);
        match.setBounds(library.removeFromBottom(30)); library.removeFromBottom(5);
        sceneSources.setBounds(library.removeFromBottom(30)); library.removeFromBottom(10);
        add.setBounds(library.removeFromBottom(30)); library.removeFromBottom(5);
        presetChoice.setBounds(library.removeFromBottom(30)); library.removeFromBottom(8); list.setBounds(library);
        auto footer=r.removeFromBottom(54); quality.setBounds(footer.removeFromRight(170).reduced(0,12));
        footer.removeFromRight(8); help.setBounds(footer);
        auto toolbar=r.removeFromTop(34); viewMode.setBounds(toolbar.removeFromLeft(190).reduced(0,2));
        if(librarySource.getSelectedId()==2)
        { nextPage.setBounds(toolbar.removeFromRight(62).reduced(2,2)); previousPage.setBounds(toolbar.removeFromRight(78).reduced(2,2)); }
        reviewCount.setBounds(toolbar.reduced(8,0));
        r.removeFromTop(6); canvas = r; gallery.setBounds(canvas); layoutGallery();
        updateView();
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(colour::panel); g.setColour(juce::Colours::black); g.fillRoundedRectangle(canvas.toFloat(), 6);
        if(viewMode.getSelectedId()==1)
        {
            if(visible.empty()) { g.setColour(colour::muted); g.drawFittedText(librarySource.getSelectedId()==2 ? "Link your MilkDrop folder to browse its presets here.\nPresets and textures stay in their existing folders." : "No scenes match this view.",canvas.reduced(20),juce::Justification::centred,3); }
            return;
        }
        if(viewMode.getSelectedId()==2)
        {
            auto area=canvas.withHeight(std::max(0,canvas.getHeight()-42)).reduced(8);
            if(selectedImage.isValid()) { g.setImageResamplingQuality(juce::Graphics::highResamplingQuality); g.drawImageWithin(selectedImage,area.getX(),area.getY(),area.getWidth(),area.getHeight(),juce::RectanglePlacement::centred); }
            g.setColour(colour::text); g.setFont(15.f);
            const int index=selectedIndex();
            const bool milk=index>=0 && isMilkDropKey(lane.catalogue()[(size_t)index].key);
            if(milk && !selectedImage.isValid())
            {
                const auto found=previewErrors.find(lane.catalogue()[(size_t)index].key);
                g.setColour(colour::muted); g.drawFittedText(found==previewErrors.end() || found->second.isEmpty() ? "Preparing preview..." : "Preview unavailable. Try Live mix, or link the folder again to rescan.",area,juce::Justification::centred,3);
            }
            g.setColour(colour::text);
            const auto caption=index<0 ? "Select a scene" : lane.catalogue()[(size_t)index].title+(milk ? (lane.catalogue()[(size_t)index].referenceImage.isNotEmpty() ? " / Preset image" : " / Generated preview") : " / "+referenceLook+(selectedImage.isValid()?" / Reference image":" / Reference image unavailable"));
            g.drawFittedText(caption,canvas.withTop(canvas.getBottom()-36).withHeight(28),juce::Justification::centred,2);
            return;
        }
        if (lane.preview.isValid())
        { g.setImageResamplingQuality(juce::Graphics::highResamplingQuality); g.drawImageWithin(lane.preview, canvas.getX(), canvas.getY(), canvas.getWidth(), canvas.getHeight() - 50, juce::RectanglePlacement::centred); recordScenePaint(lane); }
        g.setColour(colour::text); g.setFont(14.f);
        g.drawFittedText(lane.previewStatus, canvas.reduced(14).withTop(canvas.getBottom() - 46), juce::Justification::centred, 2);
        if (!lane.catalogue().size()) g.drawFittedText("No installed scenes found.", list.getBounds(), juce::Justification::centred, 2);
    }
private:
    void showSpriteFolder()
    {
        spriteFolderButton.setButtonText(spriteFolder.isEmpty() ? "Choose Sprites folder..." : "Sprites: "+juce::File(spriteFolder).getFileName());
        spriteFolderButton.setTooltip(spriteFolder.isEmpty() ? "Choose the folder Alloy remembers for automatic transparent sprites." : spriteFolder);
        resetSpriteFolder.setEnabled(spriteFolder.isNotEmpty());
    }
    void saveSpriteFolder(const juce::String& path)
    {
        if(spriteFolderPreference==juce::File() || spriteFolderPreference.getParentDirectory().createDirectory().failed())
        {
            help.setText("Could not save the Sprites folder preference.",juce::dontSendNotification);
            return;
        }
        juce::TemporaryFile temp(spriteFolderPreference);
        if(!temp.getFile().replaceWithText(path,false,false) || !temp.overwriteTargetFileWithTemporary())
        {
            help.setText("Could not save the Sprites folder preference.",juce::dontSendNotification);
            return;
        }
        spriteFolder=path;
        showSpriteFolder();
        help.setText(path.isEmpty() ? "Using the bundled Sprites folder." : "Sprites folder saved. A pinned image still takes precedence.",juce::dontSendNotification);
    }
    void loadSelectedReference()
    {
        const int index=selectedIndex(); selectedImage={}; referenceLook="Signature";
        if(index<0) return;
        const auto& entry=lane.catalogue()[(size_t)index]; auto file=previewFile(entry);
        const int look=presetChoice.getSelectedId()-1;
        if(look>=0 && look<(int)entry.presets.size())
        {
            const auto& preset=entry.presets[(size_t)look];
            const auto candidate=file.getSiblingFile(file.getFileNameWithoutExtension()+"--"+preset.id+".jpg");
            if(candidate.existsAsFile()) { file=candidate; referenceLook=preset.title; }
            else if(preset.id=="signature") referenceLook=preset.title;
        }
        selectedImage=isMilkDropKey(entry.key) ? readPreview(entry) : juce::ImageFileFormat::loadFrom(file);
    }
    juce::Image readPreview(const VisualSceneEntry& entry) const
    {
        if(entry.referenceImage.isNotEmpty())
        {
            const juce::File file(entry.referenceImage);
            if(file.getSize()<=16*1024*1024)
            { const auto image=juce::ImageFileFormat::loadFrom(file); if(image.isValid()) return image; }
        }
        return juce::ImageFileFormat::loadFrom(previewFile(entry));
    }
    juce::File previewFile(const VisualSceneEntry& entry) const
    { return isMilkDropKey(entry.key) ? MilkDropPreviewWorker::imageFile(entry.key) : sceneInstall().getChildFile("previews").getChildFile(entry.key.fromFirstOccurrenceOf("/",false,false)+".jpg"); }
    void updateView()
    {
        gallery.setVisible(viewMode.getSelectedId()==1 && !visible.empty());
        quality.setEnabled(viewMode.getSelectedId()==3 || outputVisible()); repaint();
    }
    void layoutGallery()
    {
        const int width=std::max(1,gallery.getWidth()-16),columns=std::max(1,width/210),tileWidth=width/columns;
        const int tileHeight=juce::roundToInt((tileWidth-8)*9./16.)+38;
        galleryContent.setSize(width,((int)tiles.size()+columns-1)/columns*tileHeight);
        for(int i=0;i<(int)tiles.size();++i) tiles[(size_t)i]->setBounds((i%columns)*tileWidth,(i/columns)*tileHeight,tileWidth-6,tileHeight-6);
    }
    void rebuildGallery()
    {
        tiles.clear();
        const bool milk=librarySource.getSelectedId()==2;
        if(page*pageSize>=(int)visible.size()) page=0;
        if(milk)
        {
            std::set<juce::String> keep;
            for(int row=page*pageSize;row<std::min((page+1)*pageSize,(int)visible.size());++row) keep.insert(lane.catalogue()[(size_t)visible[(size_t)row]].key);
            for(auto it=thumbnails.begin();it!=thumbnails.end();) if(isMilkDropKey(it->first) && !keep.count(it->first)) it=thumbnails.erase(it); else ++it;
        }
        previousPage.setVisible(milk); nextPage.setVisible(milk);
        previousPage.setEnabled(page>0); nextPage.setEnabled((page+1)*pageSize<(int)visible.size());
        std::vector<juce::String> pending;
        const int first=milk?page*pageSize:0, last=milk?std::min(first+pageSize,(int)visible.size()):(int)visible.size();
        for(int row=first;row<last;++row)
        {
            const int index=visible[(size_t)row]; const auto& entry=lane.catalogue()[(size_t)index];
            auto found=thumbnails.find(entry.key);
            if(found==thumbnails.end()) thumbnails[entry.key]=readPreview(entry).rescaled(320,180);
            if(milk && !thumbnails[entry.key].isValid() && previewErrors.find(entry.key)==previewErrors.end()) pending.push_back(entry.key);
            auto tile=std::make_unique<ScenePreviewTile>(thumbnails[entry.key],entry.title,
                [this,row] { list.selectRow(row); previewSelection(); },milk ? (previewErrors.count(entry.key) && previewErrors[entry.key].isNotEmpty() ? "Preview unavailable" : "Preparing preview...") : "Reference image unavailable");
            galleryContent.addAndMakeVisible(*tile); tiles.push_back(std::move(tile));
        }
        if(milk)
        {
            if(!previewWorker) previewWorker=std::make_unique<MilkDropPreviewWorker>();
            const int selected=selectedIndex();
            if(selected>=0)
            {
                const auto key=lane.catalogue()[(size_t)selected].key;
                const auto found=std::find(pending.begin(),pending.end(),key);
                if(found!=pending.end()) std::rotate(pending.begin(),found,found+1);
            }
            previewWorker->submit(std::move(pending));
        }
        reviewCount.setText(juce::String((int)visible.size())+(milk ? " presets / "+juce::String(visible.empty()?0:page+1)+" of "+juce::String(((int)visible.size()+pageSize-1)/pageSize) : " scenes"),juce::dontSendNotification);
        layoutGallery(); updateView();
    }
    void showOutput()
    {
        const int selected=outputDisplay.getSelectedId()-1;
        if(selected<0 || selected>=(int)displayAreas.size()) return;
        if(!output) output=std::make_unique<SceneOutputWindow>(lane);
        if(!output->showOn(displayAreas[(size_t)selected]))
            lane.owner.setStatus("The chosen output display is unavailable. Choose a connected monitor.");
    }
    void refreshDisplays()
    {
        const auto& displays=juce::Desktop::getInstance().getDisplays();
        std::vector<juce::Rectangle<int>> areas; juce::StringArray names;
        for(int i=0;i<displays.displays.size();++i)
        {
            const auto& display=displays.displays[i]; areas.push_back(display.logicalBounds.getSmallestIntegerContainer());
            names.add("Display "+juce::String(i+1)+" / "+juce::String(display.physicalBounds.getWidth())+" x "+juce::String(display.physicalBounds.getHeight()));
        }
        if(areas==displayAreas && names==displayNames) return;
        const int previous=outputDisplay.getSelectedId()-1;
        const auto selected=previous>=0 && previous<(int)displayAreas.size() ? displayAreas[(size_t)previous] : juce::Rectangle<int>();
        displayAreas=std::move(areas); displayNames=names; outputDisplay.clear(juce::dontSendNotification);
        outputDisplay.addItemList(names,1);
        auto found=std::find(displayAreas.begin(),displayAreas.end(),selected);
        int index=found==displayAreas.end() ? -1 : (int)std::distance(displayAreas.begin(),found);
        if(index<0 && !displayAreas.empty())
        {
            index=0; const auto* current=displays.getDisplayForRect(lane.owner.getScreenBounds());
            for(int i=0;i<displays.displays.size();++i)
                if(!current || displayAreas[(size_t)i]!=current->logicalBounds.getSmallestIntegerContainer()) { index=i; break; }
        }
        outputDisplay.setSelectedId(index+1,juce::dontSendNotification);
        if(outputVisible() && output->windowMode()==WindowMode::fullscreen
            && std::find(displayAreas.begin(),displayAreas.end(),output->getScreenBounds())==displayAreas.end())
        {
            output->setWindowMode(WindowMode::windowed);
            lane.owner.setStatus("Output display changed. Visuals returned to a movable window.");
        }
    }
    SceneLane& lane;
    juce::TextEditor search;
    juce::ListBox list;
    juce::ComboBox viewMode, presetChoice, sceneSources, librarySource;
    juce::TextButton linkFolder, previousPage, nextPage;
    static constexpr int pageSize=24;
    int page=0;
    std::unique_ptr<MilkDropPreviewWorker> previewWorker;
    std::map<juce::String,juce::String> previewErrors;
    juce::Label reviewCount;
    juce::Viewport gallery;
    juce::Component galleryContent;
    std::vector<std::unique_ptr<ScenePreviewTile>> tiles;
    std::map<juce::String,juce::Image> thumbnails;
    juce::Image selectedImage;
    juce::String referenceLook;
    juce::TextButton add, match, outputButton, imageButton, clearImage;
    juce::TextButton importMilkDrop;
    juce::TextButton spriteFolderButton, resetSpriteFolder;
    juce::File spriteFolderPreference;
    juce::String spriteFolder;
    std::unique_ptr<juce::FileChooser> imageChooser, milkDropChooser, spriteFolderChooser;
    juce::ComboBox quality, outputDisplay;
    std::unique_ptr<SceneOutputWindow> output;
    juce::Image outputImage;
    std::vector<juce::Rectangle<int>> displayAreas;
    juce::StringArray displayNames;
    juce::Label help;
    std::vector<int> visible;
    juce::Rectangle<int> canvas;
};

SceneLane::SceneLane(Workstation& w) : owner(w), scenes(discoverVisualScenes(sceneInstall()))
{
    const auto imported=discoverMilkDropScenes(milkDropLibraryRoot());
    scenes.insert(scenes.end(),imported.begin(),imported.end());
    setTitle("Visual scene timeline lane"); setWantsKeyboardFocus(true);
    setTooltip("Drag scenes here from Visuals / scenes. Move blocks or drag their edges to trim; right-click for Duplicate or Remove.");
    browserButton.setTooltip("Open the scene library and a preview driven by your mix."); browserButton.onClick = [this] { showBrowser(); };
    startTimerHz(30);
}
SceneLane::~SceneLane()
{
    stopTimer(); owner.engine.visualSpectrum.enabled = false; milkDropIndexer.reset(); worker.reset(); browser.reset();
}
bool SceneLane::importMilkDrop(const juce::File& source)
{
    if(owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return false;
    if(!rendererRoot().getChildFile("milkdrop/projectM-4.dll").existsAsFile())
    { owner.setStatus("MilkDrop support is unavailable in this build. Install the optional projectM runtime first."); return false; }
    juce::String key;
    const auto imported=importMilkDropPreset(source,milkDropLibraryRoot(),key);
    if(imported.failed()) { owner.setStatus(imported.getErrorMessage()); return false; }
    scenes.erase(std::remove_if(scenes.begin(),scenes.end(),[](const auto& e){return isMilkDropKey(e.key);}),scenes.end());
    const auto entries=discoverMilkDropScenes(milkDropLibraryRoot());
    scenes.insert(scenes.end(),entries.begin(),entries.end());
    // A new worker reads the committed imports; playback and original assets are untouched.
    worker.reset();
    owner.setStatus("MilkDrop preset imported. Select it and Add at playhead, then choose Live mix.");
    return true;
}
bool SceneLane::linkMilkDrop(const juce::File& source)
{
    if(milkDropIndexer) { owner.setStatus("MilkDrop folder indexing is already running."); return false; }
    if(!source.isDirectory()) { owner.setStatus("Choose an available MilkDrop folder."); return false; }
    milkDropIndexer=std::make_unique<MilkDropIndexWorker>(source);
    owner.setStatus("Indexing MilkDrop presets in the background. Source files stay in place...");
    return true;
}
bool SceneLane::hasSelectedScene() const
{
    return owner.selectedClip.isEmpty() && std::any_of(owner.engine.project.visualScenes.begin(), owner.engine.project.visualScenes.end(),
        [&](const auto& c) { return c.id == selection; });
}
const VisualSceneClip* SceneLane::selectedVisual() const
{
    if (!owner.selectedClip.isEmpty()) return nullptr;
    const auto& clips = owner.engine.project.visualScenes;
    const auto found = std::find_if(clips.begin(), clips.end(), [&](const auto& clip) { return clip.id == selection; });
    return found == clips.end() ? nullptr : &*found;
}
void SceneLane::setStudioWorkspace(StudioWorkspace* studio) { studioWorkspace = studio; }
bool SceneLane::placeStudio(const juce::String& key, const juce::String& title, double start, double length)
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return false;
    auto clips = owner.engine.project.visualScenes;
    const auto grid = owner.engine.project.gridClock();
    start = grid.snap(std::max(0., start), owner.engine.project.snap, owner.engine.project.snapStep());
    if (length <= 0) length = grid.timeAtBeat(grid.beatsAt(start) + 32.) - start;
    const auto layer = visualLayer(key);
    for (const auto& clip : clips)
        if (visualLayer(clip.scene) == layer)
        {
            if (start >= clip.start && start < clip.end())
            { owner.setStatus("Choose an empty part of this visual lane."); return false; }
            if (clip.start > start) length = std::min(length, clip.start - start);
        }
    VisualSceneClip clip;
    clip.scene = key; clip.title = title; clip.start = start; clip.length = length; clip.transition = "cut";
    clips.push_back(clip);
    std::stable_sort(clips.begin(), clips.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    const auto result = validateVisualScenes(clips);
    if (result.failed()) { owner.setStatus(result.getErrorMessage()); return false; }
    owner.remember();
    { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes = std::move(clips); }
    owner.clearEditSelection(); owner.selectedClip.clear(); owner.engine.selectedClip.clear();
    selection = clip.id; owner.refresh(); repaint(); return true;
}
void SceneLane::showBrowser(bool replaceSelection)
{
    if (!browser) { browserContent = new SceneBrowser(*this); browser = std::make_unique<SceneWindow>(browserContent); browser->setLookAndFeel(&owner.getLookAndFeel()); }
    browserContent->setReplaceTarget(replaceSelection ? selection : juce::String());
    browser->setVisible(true); browser->toFront(true);
}
void SceneLane::matchMix()
{
    const auto notify = [safe = juce::Component::SafePointer<SceneLane>(this)](const juce::String& message)
    {
        if (!safe) return;
        safe->owner.setStatus(message);
        if (safe->browserContent) safe->browserContent->showMatchStatus(message);
    };
    if (owner.busy || owner.engine.playing || owner.engine.exporting || owner.engine.capturingPerformance())
    { notify("Pause playback and wait for preparation before matching visuals."); return; }
    auto candidate = owner.engine.project; juce::Random random;
    const auto source=visualSceneSourceFromId((int)owner.getProperties().getWithDefault("visualSceneSource",1));
    if(source!=VisualSceneSource::originals && !milkDropRendererAvailable())
    { notify("MilkDrop support is unavailable in this build. The current visual lane was kept."); return; }
    const auto result = arrangeVisualScenes(candidate, scenes, random,source);
    if (result.failed()) { notify(result.getErrorMessage()); return; }
    const auto before = juce::JSON::toString(projectToJson(owner.engine.project));
    auto apply = [safe = juce::Component::SafePointer<SceneLane>(this), before, planned = std::move(candidate.visualScenes), notify]() mutable
    {
        if (!safe) return;
        auto& w = safe->owner;
        if (w.busy || w.engine.playing || w.engine.exporting || w.engine.capturingPerformance()
            || before != juce::JSON::toString(projectToJson(w.engine.project)))
        { notify("The mix changed. Match visuals again when it is ready."); return; }
        w.remember(); { const juce::ScopedLock guard(w.engine.lock); w.engine.project.visualScenes = std::move(planned); }
        std::set<juce::String> looks;
        for (const auto& scene : w.engine.project.visualScenes) looks.insert(scene.scene);
        safe->selection.clear(); w.refresh(); safe->repaint(); notify("Visual show: "
            +juce::String((int)w.engine.project.visualScenes.size())+" scenes, "+juce::String((int)looks.size())
            +" different looks, through the final track. Every handover has a visual transition.");
    };
    if (owner.engine.project.visualScenes.empty()) apply();
    else
    {
        notify("Visuals are matched. Confirm replacement to apply them.");
        juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Match visuals to mix")
            .withMessage("Replace the visual lane with a scene for each song? Music stays as arranged. Undo restores the previous scenes.")
            .withButton("Replace visuals").withButton("Cancel").withAssociatedComponent(this),
            juce::ModalCallbackFunction::create([apply = std::move(apply), notify](int answer) mutable
            { if (answer == 1) apply(); else notify("Visual matching cancelled."); }));
    }
}

bool SceneLane::setImage(const juce::String& path)
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return false;
    if (path.isNotEmpty() && (!juce::File::isAbsolutePath(path) || !juce::File(path).existsAsFile()))
    { owner.setStatus("Choose an existing local image. The current image was kept."); return false; }
    if (path==owner.engine.project.visualImage) return true;
    owner.remember();
    { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualImage=path; }
    owner.refresh(); repaint();
    owner.setStatus(path.isEmpty() ? "Scene image cleared." : "Image selected. Match mix or Auto Arrange chooses scenes that transform it.");
    return true;
}

double SceneLane::at(float x, bool snap) const
{
    const double t = std::max(0., studioWorkspace ? (x - 113) / studioScale + studioStart
        : (x + owner.viewport.getViewPositionX()) / owner.timeline.pixelsPerSecond);
    return owner.engine.project.gridClock().snap(t, snap && owner.engine.project.snap, owner.engine.project.snapStep());
}
double SceneLane::studioX(double seconds) const { return 113 + (seconds - studioStart) * studioScale; }
juce::Rectangle<float> SceneLane::bounds(const VisualSceneClip& c) const
{
    if (studioWorkspace)
    {
        const float rowHeight = std::max(1.f, (getHeight() - 18.f) / 4.f);
        return {(float)studioX(c.start), 19.f + (int)visualLayer(c.scene) * rowHeight,
                std::max(3.f, (float)(c.length * studioScale)), std::max(1.f, rowHeight - 2.f)};
    }
    return {(float)(c.start * owner.timeline.pixelsPerSecond - owner.viewport.getViewPositionX()), 14, std::max(3.f, (float)(c.length * owner.timeline.pixelsPerSecond)), height - 28.f};
}
bool SceneLane::applyLook(const juce::String& id, const std::map<juce::String,double>& values, bool replace, const juce::String& title)
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return false;
    auto next=owner.engine.project.visualScenes;
    auto clip=std::find_if(next.begin(),next.end(),[&](const auto& c){return c.id==id;});
    if (clip==next.end()) return false;
    const auto entry=std::find_if(scenes.begin(),scenes.end(),[&](const auto& s){return s.key==clip->scene;});
    if (entry==scenes.end()) return false;
    for (const auto& [name,value] : values)
        if (!entry->controlSpecs.count(name) || !std::isfinite(value) || value<0 || value>1) return false;
    if (replace)
    {
        clip->controls.clear(); clip->automation.clear();
        for (const auto& [name,spec] : entry->controlSpecs) clip->controls[name]=spec.initial;
    }
    for (const auto& [name,value] : values) { clip->controls[name]=value; clip->automation.erase(name); }
    if (validateVisualScenes(next).failed()) return false;
    owner.remember();
    { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes=std::move(next); }
    owner.refresh(); repaint(); owner.setStatus("Scene look: "+title+". Undo restores the previous settings.");
    return true;
}
bool SceneLane::place(const juce::String& key, double start, int preset)
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return false;
    const auto found = std::find_if(scenes.begin(), scenes.end(), [&](const auto& s) { return s.key == key; }); if (found == scenes.end()) return false;
    const auto grid = owner.engine.project.gridClock(); start = grid.snap(std::max(0., start), owner.engine.project.snap, owner.engine.project.snapStep());
    const double length = grid.timeAtBeat(grid.beatsAt(start) + 32) - start;
    auto next = owner.engine.project.visualScenes; VisualSceneClip clip; clip.scene = key; clip.title = found->title; clip.start = start; clip.length = length;
    if(preset>=0 && preset<(int)found->presets.size()) clip.controls=found->presets[(size_t)preset].controls;
    for (const auto& c : next) if (start >= c.start && start < c.end()) { owner.setStatus("There is already a scene here. Move it or choose an empty part of the Visuals lane."); return false; }
    for (const auto& c : next) if (c.start > start) clip.length = std::min(clip.length, c.start - start);
    next.push_back(clip); std::sort(next.begin(), next.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    const auto valid = validateVisualScenes(next); if (valid.failed()) return false;
    owner.remember(); { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes = std::move(next); }
    selection = clip.id; owner.selectedClip.clear(); owner.engine.selectedClip.clear(); owner.refresh(); repaint(); owner.setStatus("Added " + clip.title + " to Visuals."); return true;
}
bool SceneLane::replaceScene(const juce::String& id, const juce::String& key, int preset)
{
    if(owner.busy || owner.engine.exporting) { owner.setStatus("Finish preparation or export before replacing a scene."); return false; }
    const auto entry=std::find_if(scenes.begin(),scenes.end(),[&](const auto& e){return e.key==key;});
    if(entry==scenes.end()) { owner.setStatus("This scene is no longer available. Choose another scene; the visual lane was kept."); return false; }
    if(isMilkDropKey(key) && !milkDropRendererAvailable()) { owner.setStatus("MilkDrop rendering is unavailable in this candidate. The selected scene was kept."); return false; }
    if(preset < -1 || preset >= (int)entry->presets.size()) { owner.setStatus("Choose an available look for the replacement scene."); return false; }
    auto next=owner.engine.project.visualScenes;
    auto clip=std::find_if(next.begin(),next.end(),[&](const auto& c){return c.id==id;});
    if(clip==next.end()) { owner.setStatus("The selected scene was removed. Select a timeline scene and open Replace again."); return false; }
    clip->scene=key; clip->title=entry->title; clip->style=entry->style; clip->offset=0; clip->controls.clear(); clip->automation.clear();
    if(preset>=0) clip->controls=entry->presets[(size_t)preset].controls;
    for(const auto& [name,spec]:entry->controlSpecs) clip->controls.emplace(name,spec.initial);
    if(validateVisualScenes(next).failed()) { owner.setStatus("The replacement is invalid. The visual lane was kept."); return false; }
    owner.remember();
    { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes=std::move(next); }
    selection=id; owner.selectedClip.clear(); owner.engine.selectedClip.clear(); owner.refresh(); repaint();
    owner.setStatus("Replaced scene with "+entry->title+". Placement, duration, joins and shared image kept; scene controls/animation reset. Programme transport kept.");
    return true;
}
bool SceneLane::change(const juce::String& id, double start, double length, double offset, bool remember)
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return false;
    auto next = owner.engine.project.visualScenes; auto found = std::find_if(next.begin(), next.end(), [&](const auto& c) { return c.id == id; });
    if (found == next.end()) return false;
    if (found->start == start && found->length == length && found->offset == offset) return false;
    found->start = start; found->length = length; found->offset = offset;
    std::sort(next.begin(), next.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    if (validateVisualScenes(next).failed()) return false;
    if (remember) owner.remember();
    { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes = std::move(next); }
    owner.dirty = true; owner.timeline.updateSize(); repaint(); return true;
}
void SceneLane::removeSelected()
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return;
    auto next = owner.engine.project.visualScenes; const auto n = next.size();
    next.erase(std::remove_if(next.begin(), next.end(), [&](const auto& c) { return c.id == selection; }), next.end()); if (n == next.size()) return;
    owner.remember(); { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes = std::move(next); } selection.clear(); owner.refresh(); repaint();
}
void SceneLane::duplicateSelected()
{
    const auto selected = std::find_if(owner.engine.project.visualScenes.begin(), owner.engine.project.visualScenes.end(), [&](const auto& c) { return c.id == selection; });
    if (selected == owner.engine.project.visualScenes.end() || owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return;
    auto next = owner.engine.project.visualScenes; auto copy = *selected; copy.id = juce::Uuid().toString(); copy.start = copy.end(); next.push_back(copy);
    std::sort(next.begin(), next.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    if (validateVisualScenes(next).failed()) { owner.setStatus("There is no room immediately after this scene."); return; }
    owner.remember(); { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes = std::move(next); } selection = copy.id; owner.refresh(); repaint();
}
void SceneLane::paint(juce::Graphics& g)
{
    if (studioWorkspace) { paintStudio(g); return; }
    g.fillAll(colour::panel.darker(.15f)); g.setColour(colour::border); g.drawHorizontalLine(0, 0, (float)getWidth());
    if (owner.engine.project.visualScenes.empty()) { g.setColour(colour::muted); g.setFont(13.f); g.drawText("VISUALS   Drag a scene here or double-click to open the library", getLocalBounds().reduced(14), juce::Justification::centredLeft); }
    const auto& scenes = owner.engine.project.visualScenes;
    for (size_t index=0;index<scenes.size();++index)
    {
        const auto& c=scenes[index];
        const auto r = bounds(c); if (!r.intersects(getLocalBounds().toFloat())) continue;
        g.setColour(colour::orange.withAlpha(owner.selectedClip.isEmpty() && selection == c.id ? .34f : .15f)); g.fillRoundedRectangle(r.reduced(1, 0), 4);
        g.setColour(owner.selectedClip.isEmpty() && selection == c.id ? colour::orange : colour::border); g.drawRoundedRectangle(r.reduced(1, 0), 4, 1);
        const auto visible = r.getIntersection(getLocalBounds().toFloat());
        const juce::Graphics::ScopedSaveState clip(g); g.reduceClipRegion(visible.toNearestInt()); g.setColour(colour::text); g.setFont(14.f);
        // Overlapping blocks share time, but their labels must not paint over
        // each other. Keep each title before the next scene's leading edge.
        auto title=visible.reduced(6,0);
        if (index+1<scenes.size()) title.setRight(std::min(title.getRight(),bounds(scenes[index+1]).getX()-4));
        if (title.getWidth()>16) g.drawText(c.title,title.toNearestInt(),juce::Justification::centredLeft,true);
        g.setColour(colour::orange.withAlpha(.6f)); for (auto x : {r.getX() + 5, r.getRight() - 5}) g.drawVerticalLine((int)x, r.getY() + 17, r.getBottom() - 17);
    }
    const auto& clips = owner.engine.project.visualScenes;
    for (size_t i=1;i<clips.size();++i) if (std::abs(clips[i].start-clips[i-1].end())<1.e-6)
    {
        const int x=Clip::gridPixel(clips[i].start,owner.timeline.pixelsPerSecond)-owner.viewport.getViewPositionX();
        g.setColour(colour::orange); g.drawVerticalLine(x,48,68);
        g.setColour(colour::text); g.setFont(11.f); g.drawText("Feedback",x+3,52,65,16,juce::Justification::centredLeft);
    }
    for (size_t i = 1; i < clips.size(); ++i) if (clips[i].start < clips[i-1].end())
    {
        const float left = (float)(clips[i].start * owner.timeline.pixelsPerSecond - owner.viewport.getViewPositionX());
        const float right = (float)(clips[i-1].end() * owner.timeline.pixelsPerSecond - owner.viewport.getViewPositionX());
        auto blend = juce::Rectangle<float>(left, 52, right-left, 16).getIntersection(getLocalBounds().toFloat());
        if (blend.isEmpty()) continue;
        g.setColour(colour::orange.withAlpha(.3f)); g.fillRect(blend);
        if (blend.getWidth() > 45)
        {
            const juce::String effect="Feedback";
            g.setColour(colour::text); g.setFont(11.f); g.drawText(effect, blend.toNearestInt(), juce::Justification::centred);
        }
    }
    const float x = (float)(owner.engine.position.load() * owner.timeline.pixelsPerSecond - owner.viewport.getViewPositionX());
    g.setColour(colour::orange); g.drawVerticalLine((int)x, 0, (float)getHeight());
    if (dropTime >= 0) { g.setColour(colour::focus); g.drawVerticalLine((int)(dropTime * owner.timeline.pixelsPerSecond - owner.viewport.getViewPositionX()), 0, (float)getHeight()); }
}
void SceneLane::paintStudio(juce::Graphics& g)
{
    g.fillAll(colour::panel.darker(.23f));
    const auto grid = owner.engine.project.gridClock();
    const float row = std::max(1.f, (getHeight() - 18.f) / 4.f);
    const juce::StringArray names {"Backgrounds", "MilkDrop presets", "GLSL scenes", "Sprites"};
    const std::array<juce::Colour, 4> accents {juce::Colour(0xff9a8e72), juce::Colour(0xffa58dea), juce::Colour(0xff51c6c3), juce::Colour(0xffeda469)};
    g.setFont(11.f);
    for (int i = 0; i < 4; ++i)
    {
        const auto top = 18 + (int)std::round(i * row);
        g.setColour(colour::border); g.drawHorizontalLine(top, 0, (float)getWidth());
        g.setColour(accents[(size_t)i]); g.drawText(names[i], 7, top, 101, (int)row, juce::Justification::centredLeft, true);
    }
    const juce::Graphics::ScopedSaveState saved(g);
    g.reduceClipRegion(getLocalBounds().withTrimmedLeft(113));
    const double stride = studioScale < 6 ? 32. : studioScale < 15 ? 16. : 4.;
    double beat = std::ceil(grid.beatsAt(studioStart) / stride) * stride;
    for (int i = 0; i < 512; ++i, beat += stride)
    {
        const auto x = (int)std::round(studioX(grid.timeAtBeat(beat)));
        if (x >= getWidth()) break;
        g.setColour(colour::border.withAlpha(.5f)); g.drawVerticalLine(x, 0, (float)getHeight());
        g.setColour(colour::muted); g.drawText(juce::String((int)std::round(beat / 4) + 1), x + 4, 0, 46, 17, juce::Justification::centredLeft);
    }
    for (const auto& clip : owner.engine.project.visualScenes)
    {
        const auto box = bounds(clip);
        if (!box.intersects(getLocalBounds().toFloat())) continue;
        const auto tint = accents[(size_t)visualLayer(clip.scene)];
        const bool selected = selection == clip.id && owner.selectedClip.isEmpty();
        g.setColour(tint.withAlpha(selected ? .39f : .21f)); g.fillRoundedRectangle(box.reduced(1.f), 3.f);
        g.setColour(tint.withAlpha(selected ? 1.f : .5f)); g.drawRoundedRectangle(box.reduced(1.f), 3.f, 1.f);
        g.setColour(colour::text);
        g.drawText(clip.title, box.getIntersection(getLocalBounds().toFloat()).reduced(6.f, 0.f).toNearestInt(),
                   juce::Justification::centredLeft, true);
        const auto curve = clip.automation.find(studioParameter);
        if (selected && curve != clip.automation.end())
        {
            juce::Path line;
            const auto y = [&](double value) { return box.getBottom() - 3.f - (float)value * std::max(1.f, box.getHeight() - 6.f); };
            g.setColour(colour::text.withAlpha(.85f));
            if (curve->second.cycle)
            {
                const auto firstBeat = grid.beatsAt(clip.start);
                const auto firstX = std::max(113, (int)box.getX()), lastX = std::min(getWidth(), (int)box.getRight());
                for (int x = firstX; x <= lastX; x += 2)
                {
                    const auto v = visualCycleAt(grid.beatsAt(studioStart + (x - 113) / studioScale) - firstBeat);
                    if (x == firstX) line.startNewSubPath((float)x, y(v)); else line.lineTo((float)x, y(v));
                }
            }
            else for (const auto& point : curve->second.points)
            {
                const auto x = box.getX() + (float)point.at * box.getWidth();
                if (line.isEmpty()) line.startNewSubPath(x, y(point.value)); else line.lineTo(x, y(point.value));
                g.fillEllipse(x - 2.5f, y(point.value) - 2.5f, 5.f, 5.f);
            }
            g.strokePath(line, juce::PathStrokeType(1.f));
        }
    }
    g.setColour(colour::orange); g.drawVerticalLine((int)studioX(owner.engine.position.load()), 0, (float)getHeight());
}
const VisualSceneClip* SceneLane::hit(juce::Point<float> point) const
{
    if (studioWorkspace && point.x < 113) return nullptr;
    for (const auto& c : owner.engine.project.visualScenes)
        if (c.id == selection && bounds(c).contains(point) && (point.x < bounds(c).getX() + 8 || point.x > bounds(c).getRight() - 8)) return &c;
    for (auto it=owner.engine.project.visualScenes.rbegin();it!=owner.engine.project.visualScenes.rend();++it)
        if (bounds(*it).contains(point)) return &*it;
    return nullptr;
}
void SceneLane::mouseDown(const juce::MouseEvent& e)
{
    dragOriginal.reset();
    if (const auto* scene=hit(e.position)) dragOriginal=*scene;
    if (studioWorkspace && dragOriginal && selection == dragOriginal->id && (e.mods.isCtrlDown() || e.mods.isShiftDown()))
    {
        const auto box = bounds(*dragOriginal);
        studioWorkspace->editPoint(at(e.position.x, true), juce::jlimit(0., 1., (double)(box.getBottom() - e.position.y) / box.getHeight()), e.mods.isShiftDown());
        dragOriginal.reset(); return;
    }
    owner.clearEditSelection(); owner.selectedClip.clear(); owner.engine.selectedClip.clear();
    grabKeyboardFocus(); selection = dragOriginal ? dragOriginal->id : juce::String();
    owner.refresh(); repaint();
    if (studioWorkspace)
    {
        if (!dragOriginal && e.x >= 113) { owner.engine.seek(at(e.position.x, !e.mods.isAltDown())); return; }
        if (dragOriginal && e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Duplicate clip"); menu.addItem(2, "Remove clip");
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                [safe = juce::Component::SafePointer<SceneLane>(this), id = selection](int answer)
                {
                    if (!safe || safe->selection != id) return;
                    if (answer == 1) safe->duplicateSelected();
                    if (answer == 2) safe->removeSelected();
                });
            return;
        }
        dragX = e.position.x; dragRemembered = false; dragEdge = 0;
        if (dragOriginal) { const auto box = bounds(*dragOriginal); dragEdge = e.x < box.getX() + 7 ? -1 : e.x > box.getRight() - 7 ? 1 : 0; }
        return;
    }
    if (e.mods.isPopupMenu() && selection.isNotEmpty())
    {
        juce::PopupMenu m; m.addItem(1, "Duplicate scene"); m.addItem(2, "Remove scene"); m.addItem(3, "Replace scene..."); m.addItem(4,"Open scene library / add new");
        std::vector<VisualScenePreset> looks;
        const auto entry=std::find_if(scenes.begin(),scenes.end(),[&](const auto& s){return s.key==dragOriginal->scene;});
        if (entry!=scenes.end()) looks=entry->presets;
        juce::PopupMenu presets, views, materials, palettes;
        auto current=dragOriginal->controls;
        if(entry!=scenes.end()) for(const auto& [name,spec]:entry->controlSpecs) current.emplace(name,spec.initial);
        for (size_t i=0;i<looks.size();++i) presets.addItem(100+(int)i,looks[i].title,true,dragOriginal->automation.empty() && current==looks[i].controls);
        const juce::StringArray viewNames {"Original composition","Mirror pair","Four-way reflection","Six-sector kaleidoscope","Ten-sector kaleidoscope"};
        const juce::StringArray materialNames {"Scene material","Flowing image","Turning image tiles","Travelling image ribbons"};
        const juce::StringArray paletteNames {"Original colours","Teal and silver","Acid lime","Copper and slate","Jewel tones","Laser primaries","Mono with accent","Soft colours"};
        for (int i=0;i<viewNames.size();++i) views.addItem(200+i,viewNames[i],true,juce::roundToInt(current["viewPattern"]*4.)==i);
        for (int i=0;i<materialNames.size();++i) materials.addItem(300+i,materialNames[i],true,juce::roundToInt(current["materialMode"]*3.)==i);
        for (int i=0;i<paletteNames.size();++i) palettes.addItem(400+i,paletteNames[i],true,juce::roundToInt(current["paletteStyle"]*7.)==i);
        if(!isMilkDropKey(dragOriginal->scene))
        {
            m.addSeparator(); m.addSubMenu("Preset",presets); m.addSubMenu("View method",views); m.addSubMenu("Image method",materials);
            m.addSubMenu("Colour palette",palettes);
        }
        if(entry!=scenes.end() && entry->controlSpecs.count("reliefDepth"))
        {
            const juce::StringArray names {"Off","Gentle","Strong"};
            const std::array<juce::String,3> keys {"reliefDepth","meltWarp","displacementColour"};
            const std::array<juce::String,3> titles {"Relief depth","Domain warping","Displacement colour"};
            for(int method=0;method<3;++method)
            {
                juce::PopupMenu choices;
                for(int level=0;level<3;++level) choices.addItem(500+method*10+level,names[level],true,
                    std::abs(current[keys[(size_t)method]]-level*.5)<.01);
                m.addSubMenu(titles[(size_t)method],choices);
            }
        }
        const auto clipId=selection;
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe = juce::Component::SafePointer<SceneLane>(this),clipId,looks,viewNames,materialNames,paletteNames](int action)
        {
            if (!safe) return;
            if (action == 1) safe->duplicateSelected(); if (action == 2) safe->removeSelected(); if (action == 3) { safe->selection=clipId; safe->showBrowser(true); } if(action==4) safe->showBrowser();
            if (action>=100 && action<100+(int)looks.size()) { const auto& p=looks[(size_t)(action-100)]; safe->applyLook(clipId,p.controls,true,p.title); }
            if (action>=200 && action<205) safe->applyLook(clipId,{{"viewPattern",(action-200)/4.}},false,viewNames[action-200]);
            if (action>=300 && action<304) safe->applyLook(clipId,{{"materialMode",(action-300)/3.}},false,materialNames[action-300]);
            if (action>=400 && action<408) safe->applyLook(clipId,{{"paletteStyle",(action-400)/7.},{"paletteAmount",1.}},false,paletteNames[action-400]);
            if(action>=500 && action<=522 && (action-500)%10<3)
            {
                const std::array<juce::String,3> keys {"reliefDepth","meltWarp","displacementColour"};
                const int method=(action-500)/10,level=(action-500)%10;
                safe->applyLook(clipId,{{keys[(size_t)method],level*.5}},false,keys[(size_t)method]);
            }
        }); dragOriginal.reset(); return;
    }
    dragX = e.position.x; dragRemembered = false; dragEdge = 0;
    if (dragOriginal) { const auto r = bounds(*dragOriginal); dragEdge = e.x < r.getX() + 8 ? -1 : e.x > r.getRight() - 8 ? 1 : 0; }
}
void SceneLane::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragOriginal || !e.mouseWasDraggedSinceMouseDown()) return;
    auto c = *dragOriginal; const double delta = (e.position.x - dragX) / (studioWorkspace ? studioScale : owner.timeline.pixelsPerSecond);
    const auto grid = owner.engine.project.gridClock(); const bool snap = owner.engine.project.snap && !e.mods.isAltDown();
    if (dragEdge == 0) c.start = grid.snap(std::max(0., c.start + delta), snap, owner.engine.project.snapStep());
    else if (dragEdge == 1) c.length = grid.snap(c.end() + delta, snap, owner.engine.project.snapStep()) - c.start;
    else { const double start = grid.snap(std::max(0., c.start + delta), snap, owner.engine.project.snapStep()); c.offset += start - c.start; c.length -= start - c.start; c.start = start; }
    if (change(c.id, c.start, c.length, c.offset, !dragRemembered)) dragRemembered = true;
}
void SceneLane::mouseUp(const juce::MouseEvent&) { dragOriginal.reset(); if (dragRemembered) owner.refresh(); }
void SceneLane::mouseMove(const juce::MouseEvent& e)
{
    if (const auto* c=hit(e.position))
    { setTooltip(c->title + ": " + juce::String(c->start, 2) + " to " + juce::String(c->end(), 2) + " seconds. Move to overlap another scene for a blend; drag edges to trim."); return; }
    setTooltip("Drag a scene here from Visuals / scenes, or double-click to open the library.");
}
void SceneLane::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!studioWorkspace) { showBrowser(); return; }
    if (!hit(e.position))
    {
        const auto layer = juce::jlimit(0, 3, (int)((e.y - 18) * 4 / std::max(1, getHeight() - 18)));
        studioWorkspace->pick((VisualLayer)layer, e.x < 113 ? owner.engine.position.load() : at(e.position.x, true));
    }
}
bool SceneLane::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey) { if (studioWorkspace) studioWorkspace->pick(VisualLayer::shader, owner.engine.position.load()); else showBrowser(); return true; }
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey)
    {
        const auto& clips = owner.engine.project.visualScenes;
        if (!clips.empty())
        {
            auto selected = std::find_if(clips.begin(), clips.end(), [&](const auto& c) { return c.id == selection; });
            int index = selected == clips.end() ? 0 : (int)std::distance(clips.begin(), selected) + (key == juce::KeyPress::leftKey ? -1 : 1);
            selection = clips[(size_t)juce::jlimit(0, (int)clips.size() - 1, index)].id;
            owner.clearEditSelection(); owner.selectedClip.clear(); owner.engine.selectedClip.clear(); owner.refresh(); repaint();
        }
        return true;
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { removeSelected(); return true; }
    if (key.getModifiers().isCommandDown() && key.getTextCharacter() == 'd') { duplicateSelected(); return true; }
    return false;
}
bool SceneLane::isInterestedInDragSource(const SourceDetails& d)
{ return d.description.isString() && d.description.toString().startsWith("alloy-scene:") && !owner.busy && !owner.engine.exporting; }
void SceneLane::itemDragMove(const SourceDetails& d) { dropTime = at((float)d.localPosition.x, true); repaint(); }
void SceneLane::itemDragExit(const SourceDetails&) { dropTime = -1; repaint(); }
void SceneLane::itemDropped(const SourceDetails& d)
{
    dropTime=-1; const auto payload=d.description.toString().substring(12);
    const auto key=payload.upToFirstOccurrenceOf("|",false,false),look=payload.fromFirstOccurrenceOf("|",false,false);
    int preset=-1;
    const auto entry=std::find_if(scenes.begin(),scenes.end(),[&](const auto& s){return s.key==key;});
    if(entry!=scenes.end()) for(size_t i=0;i<entry->presets.size();++i) if(entry->presets[i].id==look) preset=(int)i;
    place(key,at((float)d.localPosition.x,true),preset);repaint();
}
void SceneLane::timerCallback()
{
    if(milkDropIndexer && !milkDropIndexer->isThreadRunning())
    {
        const auto result=milkDropIndexer->outcome;
        const auto message=result.failed() ? result.getErrorMessage() : "Linked "+juce::String(milkDropIndexer->indexed)+" MilkDrop presets; "+juce::String(milkDropIndexer->skipped)+" duplicates or unsupported files skipped. No source files copied. Choose the folder again to rescan.";
        if(result.wasOk())
        {
            scenes.erase(std::remove_if(scenes.begin(),scenes.end(),[](const auto& e){return isMilkDropKey(e.key);}),scenes.end());
            auto& entries=milkDropIndexer->entries;
            scenes.insert(scenes.end(),std::make_move_iterator(entries.begin()),std::make_move_iterator(entries.end()));
            worker.reset();
        }
        milkDropIndexer.reset();
        owner.setStatus(message);
        if(browserContent) browserContent->indexFinished(message);
    }
    const auto previousPreview=preview;
    const auto previousStatus=previewStatus;
    repaint();
    if (studioWorkspace && (studioWorkspace->isShowing() || studioWorkspace->outputShowing()))
    {
        constexpr int width = 640;
        if (worker && worker->renderWidth() != width) { worker.reset(); preview = {}; }
        if (!worker)
        {
            owner.engine.visualSpectrum.discardAvailable();
            worker = std::make_unique<SceneRenderWorker>(owner.engine, width, juce::String());
            owner.engine.visualSpectrum.enabled = true;
        }
        const auto grid = owner.engine.project.gridClock();
        const auto generation = (juce::int64)owner.engine.visualClockRevision.load();
        auto* request = new juce::DynamicObject();
        request->setProperty("generation", generation);
        request->setProperty("studioLayers", juce::var(new juce::DynamicObject()));
        worker->submit(juce::var(request), owner.engine.project.visualScenes, grid);
        juce::Image frame; juce::var facts; juce::String text;
        worker->result(frame, facts, text);
        if ((juce::int64)facts["generation"] == generation && facts.isObject() && (bool)facts["ok"] && !(bool)facts["initialised"])
        { preview = frame; previewFacts = facts; previewStatus = text; }
        else
            previewStatus = facts.isObject() && !(bool)facts["ok"] ? text : "Preparing current playback position...";
        // Hold the last accepted picture during a seek/startup response. Its
        // original facts remain intact; only a new-generation frame replaces it.
        studioWorkspace->update();
        return;
    }
    if(browserContent) browserContent->updateOutput();
    if (!browserContent || !browserContent->needsLiveRenderer())
    { if (worker) { owner.engine.visualSpectrum.enabled = false; worker.reset(); preview = {}; previewFacts = juce::var(); } return; }
    const int width=previewWidth(owner);
    const auto& spriteFolder=browserContent->currentSpriteFolder();
    if (worker && (worker->renderWidth()!=width || worker->renderSpriteFolder()!=spriteFolder))
    { owner.engine.visualSpectrum.enabled=false; worker.reset(); preview={}; previewFacts=juce::var(); }
    if (!worker) { owner.engine.visualSpectrum.discardAvailable(); worker = std::make_unique<SceneRenderWorker>(owner.engine,width,spriteFolder); owner.engine.visualSpectrum.enabled = true; }
    const double position = owner.engine.position.load();
    const auto frame = visualFrameAt(owner.engine.project.visualScenes, position);
    const auto actor = [&](const VisualSceneClip& scene, double time)
    {
        auto* value = new juce::DynamicObject(); value->setProperty("clip", scene.id); value->setProperty("scene", scene.scene);
        // The native timeline owns geometry. Echo a lossless opaque revision;
        // decimal JSON parsers must not determine whether a finished frame is stale.
        value->setProperty("revision",juce::String(scene.start,std::numeric_limits<double>::max_digits10,true)+"/"
            +juce::String(scene.offset,std::numeric_limits<double>::max_digits10,true));
        value->setProperty("controls",visualControlsToJson(visualControlsAt(scene,time)));
        value->setProperty("motionTime",visualMotionTime(scene,time));
        value->setProperty("time", time); return juce::var(value);
    };
    juce::Array<juce::var> actors;
    if (frame.outgoing) actors.add(actor(*frame.outgoing, position - frame.outgoing->start + frame.outgoing->offset));
    if (frame.incoming) actors.add(actor(*frame.incoming, position - frame.incoming->start + frame.incoming->offset));
    auto* request = new juce::DynamicObject(); request->setProperty("protocol", 6); request->setProperty("scenes", actors); request->setProperty("mix", frame.mix); request->setProperty("position", position);
    request->setProperty("views", browserContent ? browserContent->nativeViews() : juce::var(juce::Array<juce::var>()));
    request->setProperty("capture", (bool)owner.getProperties().getWithDefault("visualDiagnosticCapture",false));
    request->setProperty("image",owner.engine.project.visualImage);
    request->setProperty("transition",frame.incoming ? frame.incoming->transition : "dissolve");
    request->setProperty("transitionSeconds",frame.incoming && frame.outgoing ? std::max(0.,frame.outgoing->end()-frame.incoming->start) : 0.);
    request->setProperty("pivot",frame.incoming ? frame.incoming->pivot : .5);
    request->setProperty("generation", (juce::int64)owner.engine.visualClockRevision.load()); request->setProperty("playing", owner.engine.playing.load());
    if (!frame.incoming)
        for (const auto& next : owner.engine.project.visualScenes) if (next.start > position)
        { request->setProperty("preload", actor(next, next.offset)); break; }
    const auto grid = owner.engine.project.gridClock(); request->setProperty("beat", grid.beatsAt(position)); request->setProperty("bpm", grid.bpmAt(position)); worker->submit(juce::var(request),owner.engine.project.visualScenes,grid);
    worker->result(preview, previewFacts, previewStatus);
    bool exact = previewFacts["scenes"].size() == actors.size(), shared = false;
    for (const auto& desired : actors)
    {
        bool found = false;
        for (int i = 0; i < previewFacts["scenes"].size(); ++i)
        {
            const auto& actual = previewFacts["scenes"][i];
            if (actual["clip"] == desired["clip"] && actual["scene"] == desired["scene"] && actual["revision"] == desired["revision"])
            { found = true; shared = true; break; }
        }
        exact &= found;
    }
    // Generation and actor identity invalidate seeks/edits. Ordinary frame latency holds the last picture, rather than flashing black.
    if (!frame.primary() || !shared || (juce::int64)previewFacts["generation"] != (juce::int64)owner.engine.visualClockRevision.load()) preview = {};
    if (frame.primary() && shared && (preview.isValid() || (bool)previewFacts["presented"]))
    {
        const auto title = frame.incoming ? frame.outgoing->title + " + " + frame.incoming->title + " / " + juce::String(frame.mix * 100, 0) + "% blend" : frame.outgoing->title;
        previewStatus = (exact ? title : "Preparing " + title) + " / " + juce::String(position, 2) + " s / " + (owner.engine.playing ? "Playing" : "Paused");
    }
    else if (frame.primary() && (!previewFacts.isObject() || (bool)previewFacts["ok"])) previewStatus = "Preparing " + frame.primary()->title + "...";
    if (previewFacts["mediaError"].toString().isNotEmpty())
        previewStatus += " / Image unavailable: "+previewFacts["mediaError"].toString()+" Previous image retained.";
    if (browserContent)
    {
        if (preview!=previousPreview || previewStatus!=previousStatus) browserContent->repaint();
        browserContent->updateOutput();
    }
}
}
