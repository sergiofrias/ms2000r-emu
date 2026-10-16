// MS2000R VST3 editor (phase 2, VST3-2). See PluginEditor.h.
//
// The panel is immediate-mode (vector_panel.h): it is walked twice. The INPUT pass runs on every mouse event
// with a backend that draws nothing and answers item() from the mouse - switches and knobs change there, at
// once, so a click shorter than a frame still reaches the machine. The DRAW pass runs in paint() with a
// backend on juce::Graphics whose items are passive (no new presses, no drag), so drawing changes nothing.
// The timer (30 Hz) reads the LEDs and the LCD and repaints only when something the panel shows changed.
#include "PluginEditor.h"
#include <cstring>

namespace {
constexpr int kTabH = 30;
const juce::Colour kBg(28, 34, 38);

juce::Colour col(uint32_t c) { return juce::Colour(juce::uint8(c), juce::uint8(c >> 8), juce::uint8(c >> 16), juce::uint8(c >> 24)); }
juce::Image s_logo;

juce::Image loadLogoImage(const juce::String& home)
{
    const auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    for (const auto& f : {
        juce::File(home).getChildFile("logo.png"),
        exe.getSiblingFile("logo.png"),
        exe.getParentDirectory().getParentDirectory().getChildFile("Resources/logo.png"),
        juce::File::getCurrentWorkingDirectory().getChildFile("logo.png")
    }) {
        if (f.existsAsFile()) {
            auto img = juce::ImageFileFormat::loadFrom(f);
            if (img.isValid()) return img;
        }
    }
    return {};
}

// the input pass: no drawing, items from the mouse
struct InputBackend final : VPanel::Backend {
    Ms2kEditor::Mouse& m;
    explicit InputBackend(Ms2kEditor::Mouse& mm) : m(mm) {}
    void rectFilled(VPanel::V2, VPanel::V2, uint32_t, float) override {}
    void rect(VPanel::V2, VPanel::V2, uint32_t, float, float) override {}
    void circleFilled(VPanel::V2, float, uint32_t, int) override {}
    void circle(VPanel::V2, float, uint32_t, int, float) override {}
    void line(VPanel::V2, VPanel::V2, uint32_t, float) override {}
    VPanel::V2 textSize(float, const char*) override { return {}; }
    void text(VPanel::V2, float, uint32_t, const char*) override {}
    VPanel::Item item(const char* id, VPanel::V2 a, VPanel::V2 b) override
    {
        const bool hit = m.inside && m.pos.x >= a.x && m.pos.x < b.x && m.pos.y >= a.y && m.pos.y < b.y;
        if (m.press && hit && m.active.empty()) m.active = id;
        VPanel::Item it;
        it.active = !m.active.empty() && m.active == id;
        it.hovered = hit && (m.active.empty() || it.active);
        it.shift = m.shift;
        it.dblClick = it.hovered && m.dbl;
        it.dragY = it.active ? m.dragY : 0.0f;
        it.wheel = it.hovered ? m.wheel : 0.0f;
        return it;
    }
    void tooltip(const char*) override {}
    void lcd(VPanel::V2, VPanel::V2) override {}
    void logo(VPanel::V2, VPanel::V2, uint32_t) override {}
};

// the draw pass: juce::Graphics, passive items
struct DrawBackend final : VPanel::Backend {
    juce::Graphics& g; const Ms2kEditor::Mouse& m; juce::Typeface::Ptr face;
    std::function<void(juce::Rectangle<float>)> lcdFn;
    juce::String tip;
    float fontPx = -1.0f; juce::Font font;
    // consecutive lines of one colour and width are stroked as ONE path (a pot's 36 knurl lines, its 11 scale
    // ticks): one edge table instead of one per line
    juce::Path lines; uint32_t linesCol = 0; float linesW = -1.0f;
    // which layer this pass draws: 0 = the printed panel (cached as a picture), 1 = what changes, 2 = both
    int pass = 2; bool dyn = false;
    bool skip() const { return pass != 2 && (pass == 1) != dyn; }
    void layer(bool d) override { if (d != dyn) { flush(); dyn = d; } }
    DrawBackend(juce::Graphics& gg, const Ms2kEditor::Mouse& mm, juce::Typeface::Ptr f) : g(gg), m(mm), face(f), font(14.0f) {}
    void flush()
    {
        if (lines.isEmpty()) return;
        g.setColour(col(linesCol));
        g.strokePath(lines, juce::PathStrokeType(linesW));
        lines.clear();
    }
    static juce::Rectangle<float> R(VPanel::V2 a, VPanel::V2 b) { return { a.x, a.y, b.x - a.x, b.y - a.y }; }
    const juce::Font& fontAt(float px)
    {
        if (px != fontPx) { fontPx = px; font = face ? juce::Font(face).withHeight(px) : juce::Font(px, juce::Font::bold); }
        return font;
    }
    void rectFilled(VPanel::V2 a, VPanel::V2 b, uint32_t c, float r) override
    {
        if (skip()) return;
        flush();
        g.setColour(col(c));
        if (r > 0.0f) g.fillRoundedRectangle(R(a, b), r); else g.fillRect(R(a, b));
    }
    void rect(VPanel::V2 a, VPanel::V2 b, uint32_t c, float r, float t) override
    {
        if (skip()) return;
        flush();
        g.setColour(col(c));
        const auto q = R(a, b).reduced(t * 0.5f);
        if (r > 0.0f) g.drawRoundedRectangle(q, r, t); else g.drawRect(q, t);
    }
    void circleFilled(VPanel::V2 c, float r, uint32_t cc, int) override { if (skip()) return; flush(); g.setColour(col(cc)); g.fillEllipse(c.x - r, c.y - r, 2 * r, 2 * r); }
    void circle(VPanel::V2 c, float r, uint32_t cc, int, float t) override { if (skip()) return; flush(); g.setColour(col(cc)); g.drawEllipse(c.x - r, c.y - r, 2 * r, 2 * r, t); }
    void line(VPanel::V2 a, VPanel::V2 b, uint32_t c, float t) override
    {
        if (skip()) return;
        if (c != linesCol || t != linesW) { flush(); linesCol = c; linesW = t; }
        lines.startNewSubPath(a.x, a.y); lines.lineTo(b.x, b.y);
    }
    VPanel::V2 textSize(float px, const char* t) override { const auto& f = fontAt(px); return { f.getStringWidthFloat(t), px }; }
    void text(VPanel::V2 p, float px, uint32_t c, const char* t) override
    {
        if (skip()) return;
        flush();
        const auto& f = fontAt(px);
        g.setFont(f); g.setColour(col(c));
        g.drawSingleLineText(t, int(std::lround(p.x)), int(std::lround(p.y + f.getAscent())));
    }
    VPanel::Item item(const char* id, VPanel::V2 a, VPanel::V2 b) override
    {
        const bool hit = m.inside && m.pos.x >= a.x && m.pos.x < b.x && m.pos.y >= a.y && m.pos.y < b.y;
        VPanel::Item it;
        it.active = !m.active.empty() && m.active == id;
        it.hovered = hit && (m.active.empty() || it.active);
        return it;
    }
    void tooltip(const char* t) override { tip = t; }
    void lcd(VPanel::V2 a, VPanel::V2 b) override { if (pass == 0) return; flush(); if (lcdFn) lcdFn(R(a, b)); }
    void logo(VPanel::V2 a, VPanel::V2 b, uint32_t) override
    {
        if (skip()) return;
        flush();
        if (s_logo.isValid()) g.drawImage(s_logo, R(a, b));
    }
};

juce::Typeface::Ptr loadPanelFace()
{
    // the standalone's panel font: a narrow bold sans from the OS (the same files, the same order - gui_platform_*)
#ifdef _WIN32
    for (const char* p : { "C:\\Windows\\Fonts\\ARIALNB.TTF", "C:\\Windows\\Fonts\\arialbd.ttf", "C:\\Windows\\Fonts\\segoeuib.ttf" }) {
#else
    for (const char* p : { "/System/Library/Fonts/Supplemental/Arial Narrow Bold.ttf", "/Library/Fonts/Arial Narrow Bold.ttf",
                           "/System/Library/Fonts/Supplemental/Arial Bold.ttf" }) {
#endif
        juce::MemoryBlock mb;
        if (juce::File(p).loadFileAsData(mb) && mb.getSize() > 0)
            if (auto tf = juce::Typeface::createSystemTypefaceFor(mb.getData(), mb.getSize())) return tf;
    }
    return {};
}
} // namespace

Ms2kEditor::Ms2kEditor(Ms2kProcessor& p) : AudioProcessorEditor(p), m_proc(p)
{
    m_face = loadPanelFace();
    if (!s_logo.isValid()) s_logo = loadLogoImage(m_proc.homePath());
    // HD44780 A00 character generator, if the local dump is present (thin_gui's loadCgRom: 16 bytes a
    // character, rows 0-7, 5 low bits). Without it the LCD is drawn as text. The working directory is the
    // MS2000 folder (the processor set it).
    for (const char* f : { "hd44780_a00.bin", "full FW/hd44780_a00.bin" }) {   // ROM-PATHS-1
        juce::MemoryBlock mb;
        if (juce::File::getCurrentWorkingDirectory().getChildFile(f).loadFileAsData(mb) && mb.getSize() >= 4096) {
            const auto* b = static_cast<const uint8_t*>(mb.getData());
            for (int c = 0; c < 256; ++c) for (int y = 0; y < 8; ++y) m_cg[c][y] = b[c * 16 + y] & 0x1F;
            m_cgOk = true;
        }
    }
    setupIo();

    for (auto* b : { &m_tabPanel, &m_tabSettings, &m_tabLibrary }) { b->setClickingTogglesState(false); b->setRadioGroupId(0); addAndMakeVisible(*b); }
    m_tabLibrary.onClick = [this] { showTab(2); };
    m_tabPanel.onClick = [this] { showTab(0); };
    m_tabSettings.onClick = [this] { showTab(1); };
    m_mic2.setToggleState(m_proc.mic2, juce::dontSendNotification);
    m_dac20.setToggleState(m_proc.dac20, juce::dontSendNotification);
    m_clock.setToggleState(m_proc.hostClock.load(), juce::dontSendNotification);
    m_clock.onClick = [this] { m_proc.hostClock = m_clock.getToggleState(); m_proc.saveGlobalSettings(); };
    m_dspThr.setToggleState(m_proc.dspThread.load(), juce::dontSendNotification);
    m_dspThr.onClick = [this] { m_proc.dspThread = m_dspThr.getToggleState(); m_proc.saveGlobalSettings(); };
    m_dspThr.setColour(juce::ToggleButton::textColourId, juce::Colour(236, 242, 244));
    addChildComponent(m_dspThr);
    m_follow.setToggleState(m_proc.knobFollow.load(), juce::dontSendNotification);
    m_follow.onClick = [this] { m_proc.knobFollow = m_follow.getToggleState(); m_proc.saveGlobalSettings(); refreshKnobs(); m_dirty = true; };
    m_follow.setColour(juce::ToggleButton::textColourId, juce::Colour(236, 242, 244));
    addChildComponent(m_follow);
    m_transport.setToggleState(m_proc.transportMsgs.load(), juce::dontSendNotification);
    m_transport.onClick = [this] { m_proc.transportMsgs = m_transport.getToggleState(); m_proc.saveGlobalSettings(); };
    m_transport.setColour(juce::ToggleButton::textColourId, juce::Colour(236, 242, 244));
    addChildComponent(m_transport);
    m_sysexOut.setToggleState(m_proc.sysexOut.load(), juce::dontSendNotification);
    m_sysexOut.onClick = [this] { m_proc.sysexOut = m_sysexOut.getToggleState(); m_proc.saveGlobalSettings(); };
    m_sysexOut.setColour(juce::ToggleButton::textColourId, juce::Colour(236, 242, 244));
    addChildComponent(m_sysexOut);
    m_powerSw.setToggleState(m_proc.powerSwitch.load(), juce::dontSendNotification);
    m_powerSw.onClick = [this] { m_proc.powerSwitch = m_powerSw.getToggleState(); m_proc.saveGlobalSettings(); };
    m_powerSw.setColour(juce::ToggleButton::textColourId, juce::Colour(236, 242, 244));
    addChildComponent(m_powerSw);
    // OUT-BOOST-1: master output boost after the VOLUME pot, 0..+12 dB in 1 dB steps (global default + project)
    m_boost.setSliderStyle(juce::Slider::LinearHorizontal);
    m_boost.setRange(0.0, 12.0, 1.0);
    m_boost.setTextValueSuffix(" dB");
    m_boost.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 22);
    m_boost.setValue(double(m_proc.boostDb.load()), juce::dontSendNotification);
    m_boost.onValueChange = [this] { m_proc.boostDb = int(m_boost.getValue()); m_proc.applyVolume(); m_proc.saveGlobalSettings(); };
    addChildComponent(m_boost);
    m_boostLbl.setText("Output boost after the VOLUME pot (+dB, above 0 dB full scale the host clips)", juce::dontSendNotification);
    m_boostLbl.setColour(juce::Label::textColourId, juce::Colour(236, 242, 244));
    addChildComponent(m_boostLbl);
    // LIBRARY-1
    m_list.setModel(this);
    m_list.setRowHeight(22);
    m_list.setColour(juce::ListBox::backgroundColourId, juce::Colour(24, 30, 34));
    for (auto* l : { &m_libFile, &m_libStatus }) { l->setColour(juce::Label::textColourId, juce::Colour(236, 242, 244)); addChildComponent(*l); }
    addChildComponent(m_libOpen); addChildComponent(m_list);
    m_libOpen.onClick = [this] {
        m_chooser = std::make_unique<juce::FileChooser>("A .syx with MS2000 programs", juce::File(m_proc.libraryPath), "*.syx");
        m_chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
            const auto f = fc.getResult();
            if (f.existsAsFile()) loadLibrary(f.getFullPathName());
        });
    };
    if (m_proc.libraryPath.isNotEmpty()) loadLibrary(m_proc.libraryPath);
    m_mic2.onClick = [this] { m_proc.mic2 = m_mic2.getToggleState(); m_proc.userInputs(); m_proc.endUserEdits(); m_proc.saveGlobalSettings(); };
    m_dac20.onClick = [this] { m_proc.dac20 = m_dac20.getToggleState(); m_proc.applyDac(); m_proc.saveGlobalSettings(); };
    m_syxLoad.onClick = [this] {
        m_chooser = std::make_unique<juce::FileChooser>("Load a .syx into the MS2000R", juce::File(), "*.syx");
        m_chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
            const auto f = fc.getResult();
            if (f.existsAsFile()) m_proc.syx().startImport(f.getFullPathName().toStdString());
        });
    };
    m_syxSave.onClick = [this] {
        m_chooser = std::make_unique<juce::FileChooser>("Save all 128 programs", juce::File(), "*.syx");
        m_chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (!f.hasFileExtension("syx")) f = f.withFileExtension("syx");
            m_proc.syx().startExport(f.getFullPathName().toStdString());
        });
    };
    m_homeBtn.onClick = [this] {
        m_chooser = std::make_unique<juce::FileChooser>("The folder with flash.bin and boot-362.bin (beside it or in 'full FW')", juce::File(m_proc.homePath()));
        m_chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& fc) {
            const auto d = fc.getResult();
            if (d.isDirectory()) { m_proc.chooseHome(d); m_dirty = true; }
        });
    };
    m_demo.onClick = [this] { if (m_demoPhase == 0) { m_demoPhase = 1; m_demoT0 = juce::Time::getMillisecondCounter(); } };
    m_help.setText("Panel: drag a knob up / down (Shift = fine), mouse wheel, double-click = centre.\n"
                   "Shift+click a program key 1-16 or EXIT: it stays held (amber ring) - chords on the pads, or EXIT held while "
                   "you press GLOBAL (demo songs). Click it again to let it go.\n"
                   "POWER / VOLUME is the plugin's output level; AUDIO IN 1 / 2 are the input level pots. The host routes "
                   "its input bus to AUDIO IN 1 (left) and 2 (right).", juce::dontSendNotification);
    m_help.setJustificationType(juce::Justification::topLeft);
    for (auto* c : std::initializer_list<juce::Component*>{ &m_mic2, &m_dac20, &m_clock, &m_demo, &m_syxLoad, &m_syxSave, &m_syxStatus, &m_homeBtn, &m_help, &m_status }) addChildComponent(*c);
    m_clock.setColour(juce::ToggleButton::textColourId, juce::Colour(236, 242, 244));
    m_syxStatus.setColour(juce::Label::textColourId, juce::Colour(236, 242, 244));
    for (auto* c : std::initializer_list<juce::Component*>{ &m_mic2, &m_dac20, &m_help, &m_status })
        c->setColour(juce::Label::textColourId, juce::Colour(236, 242, 244)), c->setColour(juce::ToggleButton::textColourId, juce::Colour(236, 242, 244));

    // UI-SIZE-1: the project's own width, else the user's last width (settings file), else the large default.
    // Read BEFORE setResizeLimits: that call clamps the empty editor to 700 x 380, resized() stores the 700 in
    // editorW, and every reopen came up at the minimum (VSTHost: lost on closing the GUI; Cubase: on reload).
    const int w = m_proc.editorW >= 700 ? m_proc.editorW : m_proc.editorWDefault >= 700 ? m_proc.editorWDefault : 1400;
    setResizable(true, true);
    setResizeLimits(700, 380, 3840, 2160);
    setSize(w, int(std::lround(w * VPanel::kH / VPanel::kW)) + kTabH);
    showTab(m_proc.editorTab >= 0 && m_proc.editorTab <= 2 ? m_proc.editorTab : 0);
    if (const char* e = std::getenv("MS2K_EDITORSHOT"); e && *e) { m_shotPath = e; m_testTick = 0; showTab(0); }
    if (const char* e = std::getenv("MS2K_EDITORTEST"); e && *e) m_testMode = e;
    if (m_testMode == "follow") m_proc.knobFollow = true;
    startTimerHz(30);
}

Ms2kEditor::~Ms2kEditor()
{
    stopTimer();
    if (getWidth() >= 700) { m_proc.editorWDefault = getWidth(); m_proc.saveGlobalSettings(); }   // UI-SIZE-1
    if (m_demoPhase) { m_proc.setSwitch(3, 6, false); m_proc.setSwitch(4, 0, false); m_demoPhase = 0; }
    m_io.releaseAll();   // STATED: a key held by the mouse or LATCH is let go when the editor closes
    m_proc.endUserEdits();   // AUTOMATION-1
}

void Ms2kEditor::setupIo()
{
    m_io.lit = m_lit; m_io.shown = m_shown; m_io.knobs = m_proc.knobs;
    m_io.volume = &m_proc.volume; m_io.in1 = &m_proc.in1; m_io.in2 = &m_proc.in2;
    m_io.sw = [this](unsigned c, unsigned r, bool d) { if (m_demoPhase == 0) m_proc.userKey(c, r, d); };   // AUTOMATION-1
    m_io.knob = [this](unsigned m, unsigned x, uint16_t v) { m_proc.userKnob(m, x, v); m_lastMoved = int(m * 8 + x); m_lastMoveMs = juce::Time::getMillisecondCounter(); };
}

void Ms2kEditor::showTab(int t)
{
    m_tab = t; m_proc.editorTab = t;
    m_tabPanel.setToggleState(t == 0, juce::dontSendNotification);
    m_tabSettings.setToggleState(t == 1, juce::dontSendNotification);
    m_tabLibrary.setToggleState(t == 2, juce::dontSendNotification);
    m_transport.setVisible(t == 1); m_sysexOut.setVisible(t == 1); m_powerSw.setVisible(t == 1); m_boost.setVisible(t == 1); m_boostLbl.setVisible(t == 1); m_follow.setVisible(t == 1); m_dspThr.setVisible(t == 1);
    for (auto* c : std::initializer_list<juce::Component*>{ &m_libOpen, &m_list, &m_libFile, &m_libStatus }) c->setVisible(t == 2);
    for (auto* c : std::initializer_list<juce::Component*>{ &m_mic2, &m_dac20, &m_clock, &m_demo, &m_syxLoad, &m_syxSave, &m_syxStatus, &m_homeBtn, &m_help, &m_status }) c->setVisible(t == 1);
    if (t != 0) { m_io.releaseAll(); m_mouse.active.clear(); }
    m_dirty = true; repaint();
}

juce::Rectangle<float> Ms2kEditor::panelArea() const
{
    const auto a = getLocalBounds().withTrimmedTop(kTabH).toFloat();
    const float s = juce::jmin(a.getWidth() / VPanel::kW, a.getHeight() / VPanel::kH);
    return { a.getX() + (a.getWidth() - VPanel::kW * s) * 0.5f, a.getY(), VPanel::kW * s, VPanel::kH * s };
}

void Ms2kEditor::resized()
{
    m_tabPanel.setBounds(6, 4, 90, kTabH - 8);
    m_tabSettings.setBounds(100, 4, 90, kTabH - 8);
    m_tabLibrary.setBounds(194, 4, 90, kTabH - 8);
    {   // LIBRARY-1
        auto lr = getLocalBounds().withTrimmedTop(kTabH).reduced(24, 12);
        auto top = lr.removeFromTop(30); m_libOpen.setBounds(top.removeFromLeft(150)); top.removeFromLeft(12); m_libFile.setBounds(top);
        m_libStatus.setBounds(lr.removeFromBottom(26));
        lr.removeFromTop(8); m_list.setBounds(lr);
    }
    auto r = getLocalBounds().withTrimmedTop(kTabH).reduced(24, 16);
    m_mic2.setBounds(r.removeFromTop(30));
    m_dac20.setBounds(r.removeFromTop(30));
    m_clock.setBounds(r.removeFromTop(30));
    m_transport.setBounds(r.removeFromTop(30));
    m_sysexOut.setBounds(r.removeFromTop(30));
    m_powerSw.setBounds(r.removeFromTop(30));
    { auto row = r.removeFromTop(30); m_boost.setBounds(row.removeFromLeft(260)); m_boostLbl.setBounds(row); }
    m_follow.setBounds(r.removeFromTop(30));
    m_dspThr.setBounds(r.removeFromTop(30));
    r.removeFromTop(10);
    m_demo.setBounds(r.removeFromTop(30).withWidth(300));
    r.removeFromTop(10);
    { auto row = r.removeFromTop(30); m_syxLoad.setBounds(row.removeFromLeft(180)); row.removeFromLeft(10); m_syxSave.setBounds(row.removeFromLeft(260)); }
    m_syxStatus.setBounds(r.removeFromTop(26));
    r.removeFromTop(6);
    m_homeBtn.setBounds(r.removeFromTop(30).withWidth(300));
    r.removeFromTop(16);
    m_help.setBounds(r.removeFromTop(110));
    m_status.setBounds(r.removeFromTop(48));
    m_proc.editorW = getWidth();
    m_dirty = true;
}

// ---- mouse -> the input pass ----
void Ms2kEditor::runInput()
{
    if (m_tab != 0) return;
    const auto pa = panelArea();
    InputBackend be(m_mouse);
    m_io.stageChanged = m_io.volumeChanged = false;
    VPanel::draw(m_io, be, VPanel::V2(pa.getX(), pa.getY()), VPanel::V2(pa.getWidth(), pa.getHeight()));
    if (m_io.stageChanged) m_proc.userInputs();
    if (m_io.volumeChanged) m_proc.userVolume();
    m_mouse.press = m_mouse.dbl = false; m_mouse.dragY = m_mouse.wheel = 0.0f;
    m_dirty = true; repaint();
}
static VPanel::V2 P(const juce::MouseEvent& e) { return { e.position.x, e.position.y }; }
void Ms2kEditor::mouseDown(const juce::MouseEvent& e)
{
    m_mouse.pos = P(e); m_mouse.inside = true; m_mouse.btn = true; m_mouse.press = true;
    m_mouse.shift = e.mods.isShiftDown(); m_mouse.lastY = e.position.y; m_mouse.active.clear();
    runInput();
}
void Ms2kEditor::mouseDrag(const juce::MouseEvent& e)
{
    m_mouse.pos = P(e); m_mouse.shift = e.mods.isShiftDown();
    m_mouse.dragY += e.position.y - m_mouse.lastY; m_mouse.lastY = e.position.y;
    runInput();
}
void Ms2kEditor::mouseUp(const juce::MouseEvent& e)
{
    m_mouse.pos = P(e); m_mouse.btn = false; m_mouse.active.clear();
    runInput();
}
void Ms2kEditor::mouseMove(const juce::MouseEvent& e) { m_mouse.pos = P(e); m_mouse.inside = true; m_mouse.shift = e.mods.isShiftDown(); m_dirty = true; repaint(); }
void Ms2kEditor::mouseExit(const juce::MouseEvent&) { m_mouse.inside = false; m_dirty = true; repaint(); }
void Ms2kEditor::mouseDoubleClick(const juce::MouseEvent& e) { m_mouse.pos = P(e); m_mouse.dbl = true; runInput(); }
void Ms2kEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // one notch = 1.0 as ImGui counts it (JUCE on Windows: 120 wheel units -> deltaY 0.234375)
    m_mouse.pos = P(e); m_mouse.inside = true; m_mouse.shift = e.mods.isShiftDown();
    m_mouse.wheel += (w.isReversed ? -w.deltaY : w.deltaY) / 0.234375f;
    runInput();
}

void Ms2kEditor::testClick(float px, float py, bool shift)
{
    const auto pa = panelArea();
    const float s = pa.getWidth() / VPanel::kW;
    m_mouse.pos = VPanel::V2(pa.getX() + px * s, pa.getY() + py * s); m_mouse.inside = true;
    m_mouse.btn = true; m_mouse.press = true; m_mouse.shift = shift; m_mouse.active.clear();
    runInput();
    m_mouse.btn = false; m_mouse.active.clear();
    runInput();
    m_mouse.inside = false;
}

// ---- the timer: LEDs, LCD, demo keys ----
void Ms2kEditor::timerCallback()
{
    // AUTOMATION-1: a host gesture lasts while the mouse button is down (wheel steps end on the next tick)
    if (!juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) m_proc.endUserEdits();
    if (!isShowing() && m_testTick < 0 && m_demoPhase == 0) return;   // DSP-THREAD b: nothing to draw while hidden/minimised
    if (m_testTick >= 0) {
        ++m_testTick;
        if (m_testTick == 60) setSize(1400, int(std::lround(1400.0f * VPanel::kH / VPanel::kW)) + kTabH);
        if (m_testMode == "follow" && m_proc.runner()) {   // the firmware's pot -> value, read back from the edit buffer
            auto cut = [&] { return unsigned(m_proc.runner()->getEmulator().peekExternal(MS2000::kEditBufferAddr + 58)); };
            static unsigned c0 = 0, c1 = 0, c2 = 0;
            if (m_testTick == 95) c0 = cut();
            if (m_testTick == 100) m_proc.setKnob(3, 7, 138);
            if (m_testTick == 130) c1 = cut();
            if (m_testTick == 140) m_proc.setKnob(3, 7, 1000);
            if (m_testTick == 170) { c2 = cut(); m_testResult = juce::String::formatted("cutoff byte: %u, pot 138 -> %u, pot 1000 -> %u", c0, c1, c2); }
        }
        if (m_testMode == "latch") {
            if (m_testTick == 120) testClick(258.0f, 855.0f, true);                   // pad 1
            if (m_testTick == 130) testClick(258.0f + 101.33f * 4, 855.0f, true);     // pad 5
            if (m_testTick == 140) testClick(1745.0f, 395.0f, true);                  // EXIT
        }
        if (m_testTick == 180) {
            const float sc = juce::jmax(1.0f, juce::Component::getApproximateScaleFactorForComponent(this));
            const juce::Image img = createComponentSnapshot(getLocalBounds(), true, sc);
            juce::File(m_shotPath + ".txt").replaceWithText(m_proc.status() + juce::String::formatted(
                "\neditor %d x %d (scale %.2f), last panel draw %.1f ms\ndrawn pots: CUTOFF %u RESONANCE %u EG1 ATTACK %u LEVEL %u TEMPO %u (follow %d)\n",
                getWidth(), getHeight(), double(sc), m_paintMs, unsigned(m_io.knobs[3][7]), unsigned(m_io.knobs[3][2]), unsigned(m_io.knobs[0][4]),
                unsigned(m_io.knobs[3][3]), unsigned(m_io.knobs[3][0]), int(m_proc.knobFollow.load())) + m_testResult + "\n");
            juce::File f(m_shotPath); f.deleteFile();
            juce::FileOutputStream os(f);
            juce::PNGImageFormat png;
            if (os.openedOk()) png.writeImageToStream(img, os);
            m_testTick = -1;
        }
    }
    if (m_demoPhase) {   // the standalone's demo button: EXIT down, GLOBAL down 200 ms later, both up after 1.4 s
        const auto dt = juce::Time::getMillisecondCounter() - m_demoT0;
        if (m_demoPhase == 1) { m_proc.setSwitch(4, 0, true); m_demoPhase = 2; }
        else if (m_demoPhase == 2 && dt >= 200) { m_proc.setSwitch(3, 6, true); m_demoPhase = 3; }
        else if (m_demoPhase == 3 && dt >= 1400) { m_proc.setSwitch(3, 6, false); m_proc.setSwitch(4, 0, false); m_demoPhase = 0; }
    }
    bool changed = m_dirty;
    if (auto* r = m_proc.runner()) {
        const auto leds = r->getEmulator().panelLeds();
        for (int i = 0; i < 8; ++i) for (int j = 0; j < 12; ++j) {
            if (std::abs(leds.lit[i][j] - m_lit[i][j]) > 1.0f / 64.0f) changed = true;
            m_lit[i][j] = leds.lit[i][j];
        }
        const LcdGuiSnapshot s = r->getLcdGuiSnapshot();
        if (std::memcmp(s.line0, m_lcd.line0, sizeof s.line0) || std::memcmp(s.line1, m_lcd.line1, sizeof s.line1) ||
            std::memcmp(s.cgram, m_lcd.cgram, sizeof s.cgram) || s.displayOn != m_lcd.displayOn) changed = true;
        m_lcd = s;
    } else if (m_lcd.displayOn || std::any_of(&m_lit[0][0], &m_lit[0][0] + 8 * 12, [](float v) { return v != 0.0f; })) {
        // PWR-SW-1: no machine (switched off) - the LCD glass and every LED go dark, nothing keeps its last state
        for (int i = 0; i < 8; ++i) for (int j = 0; j < 12; ++j) m_lit[i][j] = 0.0f;
        m_lcd.displayOn = false;
        changed = true;
    }
    if (m_tab == 2) {
        const juce::String ls(m_proc.syx().status());
        if (ls != m_libStatus.getText()) m_libStatus.setText(ls, juce::dontSendNotification);
    }
    if (m_tab == 1) {
        const juce::String ss(m_proc.syx().status());
        if (ss != m_syxStatus.getText()) m_syxStatus.setText(ss, juce::dontSendNotification);
        const juce::String st = juce::String("MS2000R v" MS2K_VERSION "\n") + m_proc.status() + juce::String::formatted("\npanel draw %.1f ms", m_paintMs);
        if (st != m_status.getText()) m_status.setText(st, juce::dontSendNotification);
    }
    if (m_tab == 0 && refreshKnobs()) changed = true;
    if (changed) { m_dirty = false; repaint(); }
}

// the backlit LCD glass (thin_gui's drawLcdPanel, on juce::Graphics): 16 x 2 cells of 5 x 8 dots fitted into r,
// every dot on whole pixels; below 3 px a dot keeps no gap
void Ms2kEditor::drawLcd(juce::Graphics& g, juce::Rectangle<float> r) const
{
    const juce::Colour glass(186, 220, 64), on(26, 38, 16), off(172, 206, 58);
    g.setColour(glass); g.fillRoundedRectangle(r, 2.0f);
    const float dot = juce::jmin(r.getWidth() / 101.0f, r.getHeight() / 23.0f);
    const float ox = std::floor(r.getCentreX() - 47.5f * dot), oy = std::floor(r.getCentreY() - 8.5f * dot);
    const float gap = dot >= 3.0f ? juce::jmax(1.0f, std::floor(dot * 0.15f + 0.5f)) : 0.0f;
    for (int ln = 0; ln < 2; ++ln)
        for (int c = 0; c < 16; ++c) {
            const uint8_t ch = uint8_t((ln ? m_lcd.line1 : m_lcd.line0)[c]);
            for (int y = 0; y < 8; ++y) {
                const uint8_t bits = !m_lcd.displayOn ? 0 : (ch < 16 ? m_lcd.cgram[ch & 7][y] : (m_cgOk ? m_cg[ch][y] : 0));
                const float y0 = std::floor(oy + (ln * 9 + y) * dot), y1 = std::floor(oy + (ln * 9 + y + 1) * dot) - gap;
                for (int x = 0; x < 5; ++x) {
                    const float x0 = std::floor(ox + (c * 6 + x) * dot), x1 = std::floor(ox + (c * 6 + x + 1) * dot) - gap;
                    g.setColour(((bits >> (4 - x)) & 1) ? on : off);
                    g.fillRect(x0, y0, x1 - x0, y1 - y0);
                }
            }
            if (!m_cgOk && m_lcd.displayOn && ch >= 0x20 && ch < 0x7F) {
                g.setColour(on); g.setFont(juce::Font(8.0f * dot, juce::Font::bold));
                g.drawText(juce::String::charToString(juce::juce_wchar(ch)), juce::Rectangle<float>(ox + c * 6 * dot, oy + ln * 9 * dot, 5 * dot, 8 * dot),
                           juce::Justification::centred, false);
            }
        }
}

void Ms2kEditor::paint(juce::Graphics& g)
{
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    g.fillAll(kBg);
    if (m_tab != 0) { g.setColour(col(VPanel::cPanel())); g.fillRect(getLocalBounds().withTrimmedTop(kTabH)); return; }
    const auto pa = panelArea();
    // the printed panel as a picture at the display's pixel scale, redrawn only when the size changes
    const float ps = juce::jmax(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int iw = juce::roundToInt(float(getWidth()) * ps), ih = juce::roundToInt(float(getHeight()) * ps);
    if (!m_static.isValid() || m_static.getWidth() != iw || m_static.getHeight() != ih) {
        m_static = juce::Image(juce::Image::ARGB, iw, ih, true);
        juce::Graphics sg(m_static);
        sg.addTransform(juce::AffineTransform::scale(ps));
        DrawBackend sb(sg, m_mouse, m_face); sb.pass = 0;   // the same passive items as the draw pass: no key changes
        VPanel::draw(m_io, sb, VPanel::V2(pa.getX(), pa.getY()), VPanel::V2(pa.getWidth(), pa.getHeight()));
        sb.flush();
    }
    g.drawImageTransformed(m_static, juce::AffineTransform::scale(1.0f / ps));
    DrawBackend be(g, m_mouse, m_face); be.pass = 1;
    be.lcdFn = [this, &g](juce::Rectangle<float> r) { drawLcd(g, r); };
    VPanel::draw(m_io, be, VPanel::V2(pa.getX(), pa.getY()), VPanel::V2(pa.getWidth(), pa.getHeight()));
    be.flush();
    if (be.tip.isNotEmpty() && m_mouse.inside) {   // the knob's value
        const juce::Font f(14.0f);
        const float w = f.getStringWidthFloat(be.tip) + 12.0f;
        const juce::Rectangle<float> r(m_mouse.pos.x + 14.0f, m_mouse.pos.y + 16.0f, w, 20.0f);
        g.setColour(juce::Colour(20, 22, 26).withAlpha(0.92f)); g.fillRoundedRectangle(r, 3.0f);
        g.setColour(juce::Colour(236, 242, 244)); g.setFont(f); g.drawText(be.tip, r, juce::Justification::centred, false);
    }
    if (!m_proc.runner() && !m_proc.poweredOff()) {   // HOME-1 (PWR-SW-1: switched off = just a dark panel): not powered on - say why and where to fix it, over the panel
        const auto box = pa.withSizeKeepingCentre(pa.getWidth() * 0.62f, pa.getHeight() * 0.22f);
        g.setColour(juce::Colour(20, 22, 26).withAlpha(0.92f)); g.fillRoundedRectangle(box, 8.0f);
        g.setColour(juce::Colour(236, 242, 244)); g.setFont(juce::Font(juce::jmax(13.0f, pa.getHeight() * 0.024f)));
        g.drawFittedText(m_proc.status() + "\n\nSettings -> \"Choose the MS2000 folder...\" (the folder with flash.bin; boot-362.bin beside it or in its 'full FW' subfolder; hd44780_a00.bin optional).",
                         box.reduced(18.0f).toNearestInt(), juce::Justification::centred, 6);
    }
    m_paintMs = juce::Time::getMillisecondCounterHiRes() - t0;
}

// ---- LIBRARY-1: the Library page ----
void Ms2kEditor::loadLibrary(const juce::String& path)
{
    std::string err;
    std::vector<MS2000::SyxTool::Program> progs;
    if (!MS2000::SyxTool::parsePrograms(path.toStdString(), progs, err)) { m_libFile.setText(err, juce::dontSendNotification); return; }
    m_programs = std::move(progs);
    m_proc.libraryPath = path;
    m_libFile.setText(juce::File(path).getFileName() + "  -  " + juce::String(int(m_programs.size())) + " programs. Click one: it goes into the edit buffer (the memory is not changed; WRITE stores it).",
                      juce::dontSendNotification);
    m_list.updateContent(); m_list.repaint();
}

void Ms2kEditor::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= int(m_programs.size())) return;
    if (selected) g.fillAll(juce::Colour(44, 96, 114));
    const juce::String slot = m_programs.size() == 128 ? juce::String::charToString(juce::juce_wchar('A' + row / 16)) + juce::String(row % 16 + 1).paddedLeft('0', 2) : juce::String(row + 1);
    g.setColour(juce::Colour(150, 170, 178)); g.setFont(juce::Font(15.0f)); g.drawText(slot, 8, 0, 44, h, juce::Justification::centredLeft);
    g.setColour(juce::Colour(236, 242, 244)); g.setFont(juce::Font(15.0f, juce::Font::bold)); g.drawText(m_programs[size_t(row)].name, 56, 0, w - 60, h, juce::Justification::centredLeft);
}

void Ms2kEditor::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= int(m_programs.size())) return;
    if (!m_proc.syx().startProgram(m_programs[size_t(row)].data, m_programs[size_t(row)].name))
        m_libStatus.setText("Busy - wait for the transfer in progress", juce::dontSendNotification);
}

// ---- KNOB-FOLLOW ----
// Off: the pots are drawn where the physical pots stand (m_proc.knobs). On: from the edit buffer (programKnobs),
// for the timbre whose TIMBRE SELECT LED is lit; a pot being turned keeps the mouse's value until 300 ms after
// the last move (the firmware takes a few ms to read it back). Turning a pot always moves the physical pot.
bool Ms2kEditor::refreshKnobs()
{
    auto* r = m_proc.runner();
    if (!m_proc.knobFollow.load() || !r) {
        const bool was = m_io.knobs != m_proc.knobs;
        m_io.knobs = m_proc.knobs;
        return was;
    }
    uint8_t prog[254];
    for (int i = 0; i < 254; ++i) prog[i] = r->getEmulator().peekExternal(MS2000::kEditBufferAddr + uint32_t(i));
    uint16_t want[4][8]; bool fol[4][8];
    MS2000::programKnobs(prog, m_lit[5][2] > 0.5f ? 1 : 0, want, fol);   // TIMBRE SELECT "2" = LS5.LD02
    const bool held = (m_mouse.btn || juce::Time::getMillisecondCounter() - m_lastMoveMs < 300);
    // KNOB-FOLLOW-2 (2026-10-08, Tamas: a pot jumps while turned or snaps back to the preset value): the pots
    // follow the PROGRAM when it comes up; a pot the user has moved since then shows where the user left it,
    // not the edit buffer read back - the firmware may hold the old value (knob catch, a parameter the page
    // does not take, its own quantisation), and the read-back pulled the pot away from the hand. A new program
    // (name or TIMBRE SELECT changes) hands all pots back to the program.
    {
        uint64_t id = (m_lit[5][2] > 0.5f) ? 1u : 0u;
        for (int i = 0; i < 12; ++i) id = id * 131u + prog[i];
        if (id != m_progId) { m_progId = id; for (auto& row : m_userSet) for (auto& u : row) u = false; }
        if (m_lastMoved >= 0 && juce::Time::getMillisecondCounter() - m_lastMoveMs < 300) m_userSet[m_lastMoved / 8][m_lastMoved % 8] = true;
    }
    bool changed = m_io.knobs != m_disp;
    for (int m = 0; m < 4; ++m) for (int x = 0; x < 8; ++x) {
        if (held && m * 8 + x == m_lastMoved) continue;
        const uint16_t v = (fol[m][x] && !m_userSet[m][x]) ? want[m][x] : m_proc.knobs[m][x];
        if (v != m_disp[m][x]) { m_disp[m][x] = v; changed = true; }
    }
    m_io.knobs = m_disp;
    return changed;
}
