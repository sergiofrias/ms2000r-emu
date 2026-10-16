// vector_panel.h - the MS2000R front panel drawn as vectors (VECTOR-PANEL, 2026-09-28).
//
// Layout: the owner's manual's own MS2000R front-panel figure (MS2000_R_OM.pdf page 11, "Front and rear
// panel", printed p.5). Its drawing is vector PDF; the positions below are that drawing's coordinates at
// 4x (panel frame 1834 x 951 units, origin at the panel's top-left): knob centres and program keys read
// from its own paths (PyMuPDF get_drawings), the rest read off the RENDERED figure. No logo, no maker's
// name, no model lettering - the space where the drawing has them is left empty. Everything is drawn here:
// nothing of the figure's artwork is copied.
//
// Wiring: the same circuits as the thin panel - switches KOD-A30414 (SS column, T row), knobs KOD-A30415
// (AN4..AN7 = mux 0..3, X0..X7), LEDs KOD-A30416 (LS row, LD column, MOD SEQ / SEQ EDIT where the firmware
// drives them - see PANEL-IO c). LEDs that sit in a key on the R (OM p.5 draws their window) light the key.
// POWER/VOLUME is the host volume; AUDIO IN 1/INST and 2/VOICE are the input level pots (VR30/VR31).
//
// VST3-2 (2026-09-28): no GUI library in here any more. The panel draws through a Backend (a handful of
// primitives in pixels, and the mouse state of one interactive rectangle), so the standalone (ImGui,
// vector_panel_imgui.h) and the plugin editor (JUCE) draw the SAME panel from this one layout.
// LATCH (Tamas, 2026-09-28; every key since PWR-SW-1 - the service manual's [ON/OFF]+[1..5]/[REC] + Power ON test
// modes): Shift+click on a key keeps that key held down until it is
// clicked again - a chord on the pads, or EXIT held while GLOBAL is pressed (demo songs). A mouse has one
// pointer; a real panel has ten fingers.
#pragma once
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <functional>

namespace VPanel {

constexpr float kW = 1834.0f, kH = 951.0f;

struct V2 { float x, y; constexpr V2() : x(0), y(0) {} constexpr V2(float X, float Y) : x(X), y(Y) {} };

// colours packed as ImGui's IM_COL32 packs them: R in the low byte, A in the high byte
constexpr uint32_t RGBA(unsigned r, unsigned g, unsigned b, unsigned a = 255) { return r | (g << 8) | (b << 16) | (a << 24); }
inline uint32_t RGBAf(float r, float g, float b, float a)
{
    auto q = [](float v) { v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; return unsigned(int(v * 255.0f + 0.5f)); };
    return RGBA(q(r), q(g), q(b), q(a));
}

// One interactive rectangle's mouse state for this frame.
struct Item { bool active = false, hovered = false, dblClick = false, shift = false; float dragY = 0.0f, wheel = 0.0f; };

// What draws the panel: coordinates in pixels.
struct Backend {
    virtual ~Backend() = default;
    virtual void rectFilled(V2 a, V2 b, uint32_t col, float rounding = 0.0f) = 0;
    virtual void rect(V2 a, V2 b, uint32_t col, float rounding, float thick) = 0;
    virtual void circleFilled(V2 c, float r, uint32_t col, int seg) = 0;
    virtual void circle(V2 c, float r, uint32_t col, int seg, float thick) = 0;
    virtual void line(V2 a, V2 b, uint32_t col, float thick) = 0;
    virtual V2   textSize(float px, const char* t) = 0;                   // the panel font at px
    virtual void text(V2 topLeft, float px, uint32_t col, const char* t) = 0;
    virtual Item item(const char* id, V2 a, V2 b) = 0;                    // an interactive rectangle
    virtual void tooltip(const char* t) = 0;
    virtual void lcd(V2 a, V2 b) = 0;                                     // the backlit LCD glass into (a, b)
        virtual void logo(V2 a, V2 b, uint32_t tint) = 0;                 // <--- added this one
    // What follows changes while the machine runs (LEDs, keys, pot pointers) - true - or is the printed
    // panel - false. A backend may keep the printed panel as a picture and redraw only the rest.
    virtual void layer(bool dynamic) { (void)dynamic; }
};

struct IO {
    const float (*lit)[12] = nullptr;                         // [LS][LD] duty 0..1
    bool (*shown)[12] = nullptr;                              // marks the LEDs this panel draws
    std::function<void(unsigned, unsigned, bool)> sw;         // (SS column, T row, down)
    std::function<void(unsigned, unsigned, uint16_t)> knob;   // (mux, x, value 0..1023)
    uint16_t (*knobs)[8] = nullptr;                           // [mux][x] positions
    float* volume = nullptr; float* in1 = nullptr; float* in2 = nullptr;
    bool stageChanged = false, volumeChanged = false;
    bool swHeld[7][8] = {};                                   // what the panel holds down now (mouse or latch)
    bool mouse[7][8] = {};                                    // held by the mouse
    bool latched[7][8] = {};                                  // LATCH: held by a Shift+click
    void releaseAll()
    {
        for (unsigned c = 0; c < 7; ++c) for (unsigned r = 0; r < 8; ++r) {
            if (swHeld[c][r] && sw) sw(c, r, false);
            swHeld[c][r] = mouse[c][r] = latched[c][r] = false;
        }
    }
};

struct Ctx {
    Backend* b; V2 o; float s; IO* io;
    V2 P(float x, float y) const { return V2(o.x + x * s, o.y + y * s); }
};

// colours
inline uint32_t cPanel()  { return RGBA(46, 49, 52, 255); }   // Matte dark charcoal chassis
inline uint32_t cFrame()  { return RGBA(140, 146, 150, 255); } // Neutral gray frame borders
inline uint32_t cPrint()  { return RGBA(225, 230, 232, 255); } // Crisp off-white silkscreen
inline uint32_t cInvBg()  { return RGBA(175, 182, 185, 255); } // Inverted background pill
inline uint32_t cInvFg()  { return RGBA(46, 49, 52, 255); }   // Inverted foreground text

// text centred (align 0), left (-1) or right (+1) on x, top at y; size in panel units
inline void text(const Ctx& c, float x, float y, const char* t, float size = 17.0f, int align = 0, bool inverse = false)
{
    const float px = size * c.s;
    const V2 ts = c.b->textSize(px, t);
    V2 p = c.P(x, y);
    if (align == 0) p.x -= ts.x * 0.5f; else if (align > 0) p.x -= ts.x;
    if (inverse) {
        const float pad = 3.0f * c.s;
        c.b->rectFilled(V2(p.x - pad, p.y - pad * 0.3f), V2(p.x + ts.x + pad, p.y + ts.y + pad * 0.3f), cInvBg(), 1.5f * c.s);
        c.b->text(p, px, cInvFg(), t);
    } else {
        c.b->text(p, px, cPrint(), t);
    }
}
// ---> INSERT HERE <---
inline void textBold(const Ctx& c, float x, float y, const char* t, float size, int align = 0, float weight = 1.2f)
{
    text(c, x - weight, y, t, size, align);
    text(c, x + weight, y, t, size, align);
    text(c, x, y - weight * 0.6f, t, size, align);
    text(c, x, y + weight * 0.6f, t, size, align);
    text(c, x, y, t, size, align);
}
inline void frame(const Ctx& c, float x0, float y0, float x1, float y1)
{
    c.b->rect(c.P(x0, y0), c.P(x1, y1), cFrame(), 5.0f * c.s, 1.6f * c.s);
}

inline uint32_t ledCol(float r, float g)
{
    r = r > 1 ? 1 : r * 1.15f; g = g > 1 ? 1 : g * 1.15f; if (r > 1) r = 1; if (g > 1) g = 1;
    return RGBAf(0.20f + 0.80f * r, 0.07f + 0.83f * g + 0.10f * r, 0.06f, 1.0f);
}

inline void led(const Ctx& c, float x, float y, unsigned ls, unsigned ld, float rad = 7.0f)
{
    c.io->shown[ls][ld] = true;
    const float v = c.io->lit[ls][ld];
    const V2 p = c.P(x, y);
    c.b->layer(true);
    if (v > 0.05f) c.b->circleFilled(p, (rad + 5.0f) * c.s, RGBAf(1.0f, 0.15f, 0.08f, 0.25f * v), 20);
    c.b->circleFilled(p, rad * c.s, ledCol(v, 0), 20);
    c.b->circle(p, rad * c.s, RGBA(15, 15, 15, 255), 20, 1.2f * c.s);
    c.b->layer(false);
}

// Key cap styles, from photos of the R (Tamas): black keys, light-grey keys, the red WRITE, and the
// whitish rubber program pads that glow red when their LED is on.
enum KeyStyle { kBlack, kGray, kRed, kPad };

// A key of the switch matrix. ledLs/ledLd >= 0: the LED in the key (single colour); gLs/rLs >= 0: bicolor
// LED on LD11 with its green and red rows. On the R that LED shows through a short vertical slit at the top
// centre of the cap; a program pad lights as a whole. Program pads (and a key given latchable) take LATCH.
inline void key(Ctx& c, float cx, float cy, float w, float h, unsigned col, unsigned row, const char* id, KeyStyle st,
                int ledLs = -1, int ledLd = -1, int gLs = -1, int rLs = -1, bool latchable = false)
{
    const V2 a = c.P(cx - w * 0.5f, cy - h * 0.5f), b = c.P(cx + w * 0.5f, cy + h * 0.5f);
    const Item it = c.b->item(id, a, b);
    bool& ms = c.io->mouse[col][row];
    bool& lat = c.io->latched[col][row];
    if (it.active && !ms) lat = it.shift ? !lat : false; (void)latchable;   // pressed now
    ms = it.active;
    const bool down = ms || lat, hov = it.hovered;
    bool& held = c.io->swHeld[col][row];
    if (down != held) { held = down; c.io->sw(col, row, down); }
    float r = 0, g = 0; bool hasLed = false;
    if (ledLs >= 0) { c.io->shown[ledLs][ledLd] = true; r = c.io->lit[ledLs][ledLd]; hasLed = true; }
    if (gLs >= 0)   { c.io->shown[gLs][11] = c.io->shown[rLs][11] = true; g = c.io->lit[gLs][11]; r = c.io->lit[rLs][11]; hasLed = true; }
    c.b->layer(true);
    const float rr = (st == kPad ? 5.0f : 3.5f) * c.s, sh = down ? 0.0f : 2.5f * c.s;
    c.b->rectFilled(V2(a.x - 2 * c.s, a.y - 2 * c.s), V2(b.x + 2 * c.s + sh, b.y + 2 * c.s + sh), RGBA(12, 26, 32, 200), rr);  // shadow
    struct F4 { float x, y, z, w; } face;
    switch (st) {
    case kBlack: face = { 0.11f, 0.11f, 0.12f, 1 }; break;
    case kGray:  face = { 0.78f, 0.79f, 0.80f, 1 }; break;
    case kRed:   face = { 0.80f, 0.19f, 0.16f, 1 }; break;
    default: {   // rubber pad: whitish, glowing red-pink by its LED's duty
        const float v = (std::min)(1.0f, r * 1.1f);
        face = { 0.87f + 0.13f * v, 0.87f - 0.52f * v, 0.85f - 0.50f * v, 1 };
    } }
    if (down) { face.x *= 0.88f; face.y *= 0.88f; face.z *= 0.88f; }
    else if (hov) { face.x = (std::min)(1.0f, face.x * 1.06f + 0.02f); face.y = (std::min)(1.0f, face.y * 1.06f + 0.02f); face.z = (std::min)(1.0f, face.z * 1.06f + 0.02f); }
    c.b->rectFilled(a, b, RGBAf(face.x, face.y, face.z, face.w), rr);
    c.b->rectFilled(V2(a.x + 2 * c.s, a.y + 2 * c.s), V2(b.x - 2 * c.s, a.y + (b.y - a.y) * 0.40f), RGBA(255, 255, 255, st == kBlack ? 14 : 34), rr);
    c.b->rect(a, b, st == kBlack ? RGBA(0, 0, 0, 255) : RGBA(60, 64, 68, 255), rr, 1.0f * c.s);
    if (st == kPad) {
        if (r > 0.05f) c.b->rect(V2(a.x - 3 * c.s, a.y - 3 * c.s), V2(b.x + 3 * c.s, b.y + 3 * c.s),
                                 RGBAf(1.0f, 0.25f, 0.2f, 0.35f * r), rr, 4.0f * c.s);
    } else if (hasLed) {   // the LED slit: short, vertical, top centre
        const float m = (a.x + b.x) * 0.5f, hw = 1.8f * c.s;
        const V2 s0(m - hw, a.y + 3.0f * c.s), s1(m + hw, a.y + (b.y - a.y) * 0.34f);
        const bool lit = r + g > 0.05f;
        c.b->rectFilled(s0, s1, lit ? ledCol(r, g) : (st == kBlack ? RGBA(52, 30, 28, 255) : RGBA(120, 104, 102, 255)), 1.0f * c.s);
        if (lit) c.b->rectFilled(V2(s0.x - 2.5f * c.s, s0.y - 1.5f * c.s), V2(s1.x + 2.5f * c.s, s1.y + 1.5f * c.s),
                                 RGBAf(1.0f, 0.3f + 0.6f * g, 0.1f, 0.30f * (std::min)(1.0f, r + g)), 2.0f * c.s);
    }
    if (lat)   // LATCH: an amber ring round a key the panel keeps held
        c.b->rect(V2(a.x - 5 * c.s, a.y - 5 * c.s), V2(b.x + 5 * c.s, b.y + 5 * c.s), RGBA(255, 190, 60, 235), rr + 2 * c.s, 2.5f * c.s);
    c.b->layer(false);
}

// A pot. value 0..1023; returns true when it moved. Drag up/down (Shift = fine), wheel, double-click = centre.
inline bool pot(Ctx& c, float cx, float cy, float rad, uint16_t& v, bool detent, const char* id, int ticks = 11)
{
    const V2 p = c.P(cx, cy);
    const float R = rad * c.s;
    const Item it = c.b->item(id, V2(p.x - R * 1.25f, p.y - R * 1.25f), V2(p.x + R * 1.25f, p.y + R * 1.25f));
    int nv = v;
    if (it.active && it.dragY != 0.0f) nv -= int(it.dragY * (it.shift ? 1.0f : 4.0f));
    if (it.hovered && it.wheel != 0.0f) nv += int(it.wheel * (it.shift ? 1.0f : 16.0f));
    if (it.hovered && it.dblClick) nv = 512;
    nv = nv < 0 ? 0 : nv > 1023 ? 1023 : nv;
    const bool moved = nv != v; v = uint16_t(nv);
    if (it.hovered || it.active) { char tt[24]; std::snprintf(tt, sizeof tt, "%u / 1023", unsigned(v)); c.b->tooltip(tt); }
    const float PI = 3.14159265f;
    for (int i = 0; i < ticks; ++i) {   // the scale printed around the knob
        const float a = PI * (0.75f + 1.5f * float(i) / float(ticks - 1));
        const bool mid = detent && i == ticks / 2;
        const float r0 = R * 1.12f, r1 = R * (mid ? 1.30f : 1.22f);
        c.b->line(V2(p.x + std::cos(a) * r0, p.y + std::sin(a) * r0), V2(p.x + std::cos(a) * r1, p.y + std::sin(a) * r1),
                  cPrint(), (mid ? 2.2f : 1.3f) * c.s);
    }
    c.b->circleFilled(V2(p.x + 1.5f * c.s, p.y + 2.5f * c.s), R, RGBA(10, 10, 12, 160), 40);   // shadow
    c.b->circleFilled(p, R, RGBA(22, 22, 24, 255), 40);                                         // knurled skirt
    for (int i = 0; i < 36; ++i) {
        const float a = 2 * PI * i / 36.0f;
        c.b->line(V2(p.x + std::cos(a) * R * 0.80f, p.y + std::sin(a) * R * 0.80f),
                  V2(p.x + std::cos(a) * R * 0.98f, p.y + std::sin(a) * R * 0.98f), RGBA(55, 56, 60, 255), 1.0f * c.s);
    }
    c.b->circleFilled(p, R * 0.74f, RGBA(44, 45, 49, 255), 40);                                 // cap
    c.b->circle(p, R * 0.74f, RGBA(80, 82, 88, 255), 40, 1.2f * c.s);
    const float a = PI * (0.75f + 1.5f * float(v) / 1023.0f);
    c.b->layer(true);
    c.b->line(V2(p.x + std::cos(a) * R * 0.18f, p.y + std::sin(a) * R * 0.18f),
              V2(p.x + std::cos(a) * R * 0.92f, p.y + std::sin(a) * R * 0.92f), RGBA(245, 245, 245, 255), 2.6f * c.s);
    c.b->layer(false);
    return moved;
}

inline bool panelPot(Ctx& c, float cx, float cy, unsigned mux, unsigned x, bool detent, const char* id)
{
    uint16_t& v = c.io->knobs[mux][x];
    if (pot(c, cx, cy, 24.0f, v, detent, id)) { c.io->knob(mux, x, v); return true; }
    return false;
}

inline bool floatPot(Ctx& c, float cx, float cy, float rad, float& f, const char* id)
{
    uint16_t v = uint16_t(f * 1023.0f + 0.5f);
    if (pot(c, cx, cy, rad, v, false, id)) { f = float(v) / 1023.0f; return true; }
    return false;
}

// the whole panel, fitted into (pos, avail)
inline void draw(IO& io, Backend& be, V2 pos, V2 avail)
{
    Ctx c{ &be, pos, (std::min)(avail.x / kW, avail.y / kH), &io };
    std::memset(io.shown, 0, sizeof(bool) * 8 * 12);
    c.b->rectFilled(c.P(0, 0), c.P(kW, kH), cPanel(), 8.0f * c.s);
    c.b->rect(c.P(0, 0), c.P(kW, kH), RGBA(90, 92, 98, 255), 8.0f * c.s, 2.0f * c.s);

    // ---------------- section frames and headers ----------------
    frame(c, 10, 32, 205, 178);   text(c, 107, 8, "AUDIO IN", 20);
    frame(c, 10, 205, 205, 310);  text(c, 107, 181, "POWER / VOLUME", 20);
    frame(c, 10, 342, 205, 442);  text(c, 107, 317, "PHONES", 20);
    frame(c, 10, 500, 205, 625);  text(c, 107, 476, "KEYBOARD", 20);
    frame(c, 10, 657, 205, 775);  text(c, 64, 634, "SEQ EDIT /", 19); text(c, 150, 634, "CH PARAM", 19, 0, true);
    frame(c, 10, 812, 205, 940);  text(c, 107, 786, "BANK / OCTAVE", 20);
    frame(c, 213, 32, 410, 470);  text(c, 310, 8, "OSCILLATOR 1", 20);
    frame(c, 415, 32, 610, 470);  text(c, 512, 8, "OSCILLATOR 2", 20);
    frame(c, 617, 32, 712, 470);  text(c, 664, 8, "MIXER", 20);
    frame(c, 718, 32, 912, 470);  text(c, 815, 8, "FILTER", 20);
    frame(c, 918, 32, 1012, 625); text(c, 965, 8, "AMP", 20);
    frame(c, 213, 500, 410, 625); text(c, 310, 476, "TIMBRE SELECT", 20);
    frame(c, 415, 500, 712, 625); text(c, 563, 476, "MOD SEQUENCE", 20);
    frame(c, 718, 500, 912, 625); text(c, 815, 476, "PORTAMENTO", 20);
    frame(c, 1022, 180, 1418, 410); text(c, 1220, 153, "ARPEGGIATOR", 20);
    frame(c, 1022, 462, 1120, 775); text(c, 1069, 436, "LFO 1", 20);
    frame(c, 1124, 462, 1222, 775); text(c, 1171, 436, "LFO 2", 20);
    frame(c, 1226, 462, 1418, 775); text(c, 1322, 436, "EFFECTS", 20);
    frame(c, 1430, 462, 1818, 648); text(c, 1624, 436, "VIRTUAL PATCH", 20);
    frame(c, 1430, 32, 1818, 425);
    frame(c, 213, 657, 1012, 775); text(c, 410, 634, "EG 1 (FILTER)", 20); text(c, 816, 634, "EG 2 (AMP)", 20);
    frame(c, 213, 812, 1818, 900);
    frame(c, 1352, 70, 1422, 140); text(c, 1387, 78, "ORIGINAL", 13); text(c, 1387, 93, "VALUE", 13);
    // ---------------- MS2000BR LOGO & BRANDING ----------------
    const float lx = 1035.0f, ly = 42.0f;
    const float lh = 96.0f, lw = lh * (799.0f / 355.0f);
    c.b->logo(c.P(lx, ly), c.P(lx + lw, ly + lh), cPrint());

    // ---------------- AUDIO IN, POWER/VOLUME, PHONES ----------------
    text(c, 57, 70, "1 /", 15, 1); text(c, 80, 70, "INST", 15, 0, true);
    text(c, 150, 70, "2 /", 15, 1); text(c, 175, 70, "VOICE", 15, 0, true);
    c.b->circle(c.P(57, 55), 6 * c.s, cPrint(), 16, 1.2f * c.s);    // input LEDs (analog, not on the matrix)
    c.b->circle(c.P(157, 55), 6 * c.s, cPrint(), 16, 1.2f * c.s);
    if (floatPot(c, 57, 122, 22, *io.in1, "##in1")) io.stageChanged = true;
    if (floatPot(c, 157, 122, 22, *io.in2, "##in2")) io.stageChanged = true;
    if (floatPot(c, 105, 268, 22, *io.volume, "##vol")) io.volumeChanged = true;
    text(c, 34, 288, "POWER OFF", 11, 0); text(c, 150, 288, "10", 11, 0);
    c.b->circle(c.P(107, 390), 30 * c.s, cPrint(), 40, 2.0f * c.s);
    c.b->circleFilled(c.P(107, 390), 18 * c.s, RGBA(12, 12, 14, 255), 32);
    c.b->rectFilled(c.P(14, 447), c.P(58, 459), cInvBg()); text(c, 66, 446, ": VOCODER PARAMETER", 13, -1);

    // ---------------- OSCILLATOR 1 ----------------
    led(c, 231, 108, 2, 4); text(c, 247, 99, "SAW", 14, -1);
    led(c, 231, 132, 3, 4); text(c, 247, 123, "SQU / PWM", 14, -1);
    led(c, 231, 155, 4, 4); text(c, 247, 146, "TRI", 14, -1);
    led(c, 231, 178, 5, 4); text(c, 247, 169, "SIN / CROSS", 14, -1);
    led(c, 332, 108, 2, 5); text(c, 344, 99, "VOX WAVE", 14, -1);
    led(c, 332, 132, 3, 5); text(c, 344, 123, "DWGS", 14, -1);
    led(c, 332, 155, 4, 5); text(c, 344, 146, "NOISE", 14, -1);
    led(c, 332, 178, 5, 5); text(c, 344, 169, "AUDIO IN", 14, -1);
    text(c, 311, 216, "WAVE", 15); text(c, 311, 233, "WAVE", 14, 0, true);
    key(c, 311, 272, 76, 34, 0, 7, "##o1wave", kBlack);
    text(c, 258, 350, "CONTROL 1", 15); text(c, 258, 367, "CONTROL 1", 14, 0, true);
    text(c, 360, 350, "CONTROL 2", 15); text(c, 360, 367, "CONTROL 2", 14, 0, true);
    panelPot(c, 258, 412, 2, 6, false, "##o1c1");
    panelPot(c, 360, 412, 2, 1, false, "##o1c2");

    // ---------------- OSCILLATOR 2 ----------------
    led(c, 432, 108, 3, 6); text(c, 446, 99, "SAW", 14, -1);
    led(c, 432, 132, 4, 6); text(c, 446, 123, "SQU", 14, -1);
    led(c, 432, 155, 5, 6); text(c, 446, 146, "TRI", 14, -1);
    led(c, 533, 108, 2, 7); text(c, 547, 99, "RING", 14, -1);
    led(c, 533, 132, 3, 7); text(c, 547, 123, "SYNC", 14, -1);
    text(c, 462, 233, "WAVE", 15); text(c, 562, 233, "OSC MOD", 15);
    key(c, 462, 272, 76, 34, 1, 1, "##o2wave", kBlack);
    key(c, 562, 272, 76, 34, 1, 0, "##o2mod", kBlack);
    text(c, 512, 332, "OSC 2 PITCH", 15);
    text(c, 461, 350, "SEMITONE", 15); text(c, 461, 367, "HPF LEVEL", 14, 0, true);
    text(c, 562, 350, "TUNE", 15); text(c, 562, 367, "THRESHOLD", 14, 0, true);
    panelPot(c, 461, 412, 2, 7, true, "##o2semi");
    panelPot(c, 562, 412, 2, 0, true, "##o2tune");

    // ---------------- MIXER ----------------
    text(c, 664, 56, "OSC 1", 15); text(c, 664, 73, "OSC 1", 14, 0, true);
    panelPot(c, 664, 122, 2, 2, false, "##mx1");
    text(c, 664, 204, "OSC 2", 15); text(c, 664, 221, "INST", 14, 0, true);
    panelPot(c, 664, 268, 2, 4, false, "##mx2");
    text(c, 664, 350, "NOISE", 15); text(c, 664, 367, "NOISE", 14, 0, true);
    panelPot(c, 664, 412, 2, 5, false, "##mxn");

    // ---------------- FILTER ----------------
    text(c, 765, 56, "CUTOFF", 15); text(c, 765, 73, "CUTOFF", 14, 0, true);
    text(c, 867, 56, "RESONANCE", 15); text(c, 867, 73, "RESONANCE", 14, 0, true);
    panelPot(c, 765, 122, 3, 7, false, "##cut");
    panelPot(c, 867, 122, 3, 2, false, "##res");
    text(c, 765, 204, "EG 1 INT", 15); text(c, 765, 221, "FC MOD INT", 14, 0, true);
    text(c, 867, 204, "KBD TRACK", 15); text(c, 867, 221, "E.F.SENSE", 14, 0, true);
    panelPot(c, 765, 268, 3, 4, true, "##eg1int");
    panelPot(c, 867, 268, 3, 6, true, "##kbdtr");
    text(c, 885, 297, "HOLD", 14, 0, true);
    led(c, 838, 340, 2, 2); text(c, 850, 331, "24LPF", 14, -1); text(c, 896, 331, "+1", 13, 0, true);
    led(c, 838, 363, 3, 2); text(c, 850, 354, "12LPF", 14, -1); text(c, 896, 354, "+2", 13, 0, true);
    led(c, 838, 386, 4, 2); text(c, 850, 377, "12BPF", 14, -1); text(c, 896, 377, "-1", 13, 0, true);
    led(c, 838, 409, 2, 3); text(c, 850, 400, "12HPF", 14, -1); text(c, 896, 400, "-2", 13, 0, true);
    text(c, 765, 364, "FILTER TYPE", 15); text(c, 765, 381, "FORMANT SHIFT", 12, 0, true);
    key(c, 765, 418, 76, 34, 1, 4, "##ftype", kBlack);

    // ---------------- AMP ----------------
    text(c, 968, 56, "LEVEL", 15); text(c, 968, 73, "LEVEL", 14, 0, true);
    panelPot(c, 968, 122, 3, 3, false, "##lvl");
    text(c, 968, 204, "PAN", 15); text(c, 968, 221, "DIRECT LEVEL", 12, 0, true);
    panelPot(c, 968, 268, 3, 1, true, "##pan");
    text(c, 968, 378, "EG 2 / GATE", 14);
    key(c, 968, 418, 70, 34, 1, 7, "##eg2gate", kBlack, 6, 4);
    text(c, 968, 521, "DISTORTION", 14); text(c, 968, 538, "DISTORTION", 12, 0, true);
    key(c, 968, 575, 70, 34, 1, 6, "##dist", kBlack, 6, 5);

    // ---------------- TIMBRE SELECT / MOD SEQUENCE / PORTAMENTO ----------------
    text(c, 300, 537, "SELECT", 14);
    key(c, 310, 575, 76, 34, 1, 2, "##timbre", kGray);
    led(c, 374, 544, 5, 3); text(c, 388, 535, "1", 14, -1);
    led(c, 374, 566, 5, 2); text(c, 388, 557, "2", 14, -1);
    text(c, 513, 537, "ON / OFF", 14); text(c, 613, 537, "REC", 14);
    key(c, 513, 575, 76, 34, 0, 6, "##mseqon", kBlack, 7, 1);
    key(c, 613, 575, 76, 34, 1, 5, "##mseqrec", kGray, 6, 6);
    text(c, 816, 510, "TIME", 14); text(c, 816, 527, "TIME", 13, 0, true);
    panelPot(c, 816, 575, 2, 3, false, "##porta");

    // ---------------- KEYBOARD, SEQ EDIT, BANK ----------------
    key(c, 70, 572, 76, 34, 0, 4, "##kbd", kGray, -1, -1, 1, 0);
    for (int i = 0; i < 7; ++i) c.b->rectFilled(c.P(122.0f + i * 10.0f, 552), c.P(128.0f + i * 10.0f, 596), cPrint());
    text(c, 60, 668, "SELECT", 14); text(c, 60, 684, "SELECT", 13, 0, true);
    key(c, 60, 722, 70, 34, 0, 3, "##seqedit", kGray);
    led(c, 116, 695, 6, 1); text(c, 128, 686, "SEQ 1", 13, -1); text(c, 167, 687, "LEVEL", 11, -1, true);
    led(c, 116, 722, 7, 0); text(c, 128, 713, "SEQ 2", 13, -1); text(c, 167, 714, "PAN", 11, -1, true);
    led(c, 116, 749, 6, 0); text(c, 128, 740, "SEQ 3", 13, -1);
    text(c, 68, 829, "DOWN", 14); text(c, 146, 829, "UP", 14);
    key(c, 68, 867, 70, 34, 2, 7, "##bankdn", kGray, -1, -1, 5, 4);
    key(c, 146, 867, 70, 34, 2, 6, "##bankup", kGray, -1, -1, 7, 6);

    // ---------------- ARPEGGIATOR ----------------
    led(c, 1069, 203, 6, 8);
    text(c, 1069, 221, "TEMPO", 15); text(c, 1171, 221, "GATE", 15);
    panelPot(c, 1069, 268, 3, 0, false, "##tempo");
    panelPot(c, 1171, 268, 3, 5, false, "##gate");
    frame(c, 1238, 244, 1392, 344);
    c.b->line(c.P(1316, 244), c.P(1316, 344), cFrame(), 1.4f * c.s);
    static const char* rng[4] = { "1 OCTAVE", "2 OCTAVE", "3 OCTAVE", "4 OCTAVE" };
    static const char* typ[6] = { "UP", "DOWN", "ALT 1", "ALT 2", "RANDOM", "TRIGGER" };
    for (int i = 0; i < 4; ++i) text(c, 1277, 248.0f + i * 16.0f, rng[i], 12);
    for (int i = 0; i < 6; ++i) text(c, 1354, 248.0f + i * 16.0f, typ[i], 12);
    text(c, 1117, 358, "ON / OFF", 14); text(c, 1193, 358, "LATCH", 14); text(c, 1273, 358, "RANGE", 14); text(c, 1352, 358, "TYPE", 14);
    key(c, 1117, 395, 70, 30, 2, 4, "##arpon", kBlack, 6, 7);
    key(c, 1193, 395, 70, 30, 2, 3, "##arplatch", kBlack, 5, 7);
    key(c, 1273, 395, 70, 30, 2, 2, "##arprange", kBlack);
    key(c, 1352, 395, 70, 30, 2, 1, "##arptype", kBlack);

    // ---------------- LCD and its keys ----------------
    c.b->rectFilled(c.P(1462, 50), c.P(1792, 164), RGBA(14, 16, 18, 255), 5 * c.s);   // bezel
    be.lcd(c.P(1474, 62), c.P(1780, 152));                                                    // backlit glass
    led(c, 1387, 123, 4, 7);
    text(c, 1548, 178, "CURSOR", 14); text(c, 1665, 192, "- / NO", 14); text(c, 1745, 192, "+ / YES", 14);
    text(c, 1510, 192, "<", 16); text(c, 1585, 192, ">", 16);
    key(c, 1510, 230, 70, 30, 4, 7, "##curl", kGray);
    key(c, 1585, 230, 70, 30, 4, 6, "##curr", kGray);
    key(c, 1665, 230, 70, 30, 4, 5, "##no", kGray);
    key(c, 1745, 230, 70, 30, 4, 4, "##yes", kGray);
    text(c, 1548, 262, "PAGE", 14); text(c, 1510, 276, "-", 16); text(c, 1585, 276, "+", 16); text(c, 1745, 276, "WRITE", 14);
    key(c, 1510, 313, 70, 30, 4, 3, "##pgdn", kGray);
    key(c, 1585, 313, 70, 30, 4, 2, "##pgup", kGray);
    key(c, 1745, 313, 70, 30, 4, 1, "##write", kRed);
    text(c, 1510, 358, "EDIT", 14); text(c, 1587, 358, "GLOBAL", 14); text(c, 1745, 358, "EXIT", 14);
    text(c, 1680, 341, "COMPARE", 12); text(c, 1666, 358, "DEMO", 12);
    key(c, 1510, 395, 70, 30, 3, 7, "##edit", kGray);
    key(c, 1587, 395, 70, 30, 3, 6, "##global", kGray);
    key(c, 1745, 395, 70, 30, 4, 0, "##exit", kGray, -1, -1, -1, -1, true);   // LATCH

    // ---------------- LFO 1 / LFO 2 / EFFECTS ----------------
    led(c, 1040, 484, 4, 3); text(c, 1054, 475, "SAW", 13, -1);
    led(c, 1040, 506, 7, 3); text(c, 1054, 497, "SQU", 13, -1);
    led(c, 1040, 529, 7, 4); text(c, 1054, 520, "TRI", 13, -1);
    led(c, 1040, 552, 7, 5); text(c, 1054, 543, "S / H", 13, -1);
    led(c, 1141, 484, 3, 3); text(c, 1155, 475, "SAW", 13, -1);
    led(c, 1141, 506, 7, 6); text(c, 1155, 497, "SQU +", 13, -1);
    led(c, 1141, 529, 7, 7); text(c, 1155, 520, "SIN", 13, -1);
    led(c, 1141, 552, 7, 8); text(c, 1155, 543, "S / H", 13, -1);
    text(c, 1069, 567, "SELECT", 14); text(c, 1069, 583, "SELECT", 13, 0, true);
    text(c, 1171, 567, "SELECT", 14); text(c, 1171, 583, "SELECT", 13, 0, true);
    key(c, 1069, 620, 70, 30, 3, 5, "##lfo1", kBlack);
    key(c, 1171, 620, 70, 30, 3, 4, "##lfo2", kBlack);
    text(c, 1069, 664, "FREQUENCY", 13); text(c, 1069, 680, "FREQUENCY", 12, 0, true);
    text(c, 1171, 664, "FREQUENCY", 13); text(c, 1171, 680, "FREQUENCY", 12, 0, true);
    panelPot(c, 1069, 730, 1, 4, false, "##lfo1f");
    panelPot(c, 1171, 730, 1, 6, false, "##lfo2f");
    text(c, 1322, 567, "MOD / DELAY", 14); text(c, 1322, 583, "MOD / DELAY", 13, 0, true);
    key(c, 1322, 620, 70, 30, 3, 3, "##moddelay", kBlack, 6, 2);
    text(c, 1272, 664, "SPEED / TIME", 12); text(c, 1272, 680, "SPEED / TIME", 11, 0, true);
    text(c, 1373, 664, "DEPTH / FEEDBACK", 11); text(c, 1373, 680, "DEPTH / FEEDBACK", 10, 0, true);
    panelPot(c, 1272, 730, 1, 2, false, "##fxspeed");
    panelPot(c, 1373, 730, 1, 5, false, "##fxdepth");

    // ---------------- VIRTUAL PATCH ----------------
    static const char* src[8] = { "EG 1", "EG 2", "LFO 1", "LFO 2", "VELOCITY", "KBD TRACK", "MIDI 1", "MIDI 2" };
    static const char* dst[8] = { "PITCH", "OSC 2 PITCH", "OSC 1 CTRL 1", "NOISE LEVEL", "CUTOFF", "AMP", "PAN", "LFO 2 FREQ" };
    for (int i = 0; i < 8; ++i) {
        const float x = i < 4 ? 1445.0f : 1545.0f, y = 484.0f + 23.0f * float(i & 3);
        led(c, x, y, unsigned(i), 9); text(c, x + 13, y - 9, src[i], 12, -1);
        const float xd = i < 4 ? 1648.0f : 1748.0f;
        led(c, xd, y, unsigned(i), 10); text(c, xd + 13, y - 9, dst[i], 12, -1);
    }
    text(c, 1474, 583, "SELECT", 14);
    text(c, 1575, 567, "SOURCE", 14); text(c, 1575, 583, "FC MOD SOURCE", 11, 0, true);
    text(c, 1677, 583, "DESTINATION", 14);
    key(c, 1474, 620, 70, 30, 3, 2, "##vpsel", kBlack);
    key(c, 1575, 620, 70, 30, 3, 1, "##vpsrc", kBlack);
    key(c, 1677, 620, 70, 30, 3, 0, "##vpdst", kBlack);
    c.b->line(c.P(1474, 670), c.P(1778, 670), cFrame(), 1.4f * c.s);
    static const unsigned vpX[4] = { 7, 0, 3, 1 };
    for (int i = 0; i < 4; ++i) {
        const float x = 1474.0f + 101.33f * float(i);
        char lb[16]; snprintf(lb, sizeof lb, "PATCH %d", i + 1);
        led(c, x, 670, unsigned(i), 8, 6.0f);
        text(c, x, 683, lb, 14);
        char id[16]; snprintf(id, sizeof id, "##vp%d", i);
        panelPot(c, x, 730, 1, vpX[i], true, id);
    }

    // ---------------- EG 1 / EG 2 ----------------
    static const char* adsr[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
    static const unsigned eg1X[4] = { 4, 6, 2, 0 }, eg2X[4] = { 7, 1, 5, 3 };
    for (int i = 0; i < 4; ++i) {
        const float x1 = 258.0f + 101.33f * float(i), x2 = 663.0f + 101.33f * float(i);
        text(c, x1, 664, adsr[i], 13); text(c, x1, 680, adsr[i], 12, 0, true);
        text(c, x2, 664, adsr[i], 13); text(c, x2, 680, adsr[i], 12, 0, true);
        char id[16];
        snprintf(id, sizeof id, "##eg1%d", i); panelPot(c, x1, 730, 0, eg1X[i], false, id);
        snprintf(id, sizeof id, "##eg2%d", i); panelPot(c, x2, 730, 0, eg2X[i], false, id);
    }

    // ---------------- program keys 1-16 ----------------
    static const char* l1[16] = { "COMMON", "VOICE", "PITCH", "OSC 1", "OSC 2", "FILTER", "AMP", "EG", "LFO", "PATCH", "SEQ", "MOD FX", "DELAY FX", "EQ", "ARPEGGIO", "UTILITY" };
    static const char* l2[16] = { "COMMON", "VOICE", "PITCH", "OSC 1", "AUDIO IN 2", "FILTER", "AMP", "EG", "LFO", "CH LEVEL", "CH PAN", "MOD FX", "DELAY FX", "EQ", "ARPEGGIO", "UTILITY" };
    for (unsigned k = 0; k < 16; ++k) {
        const float x = 258.0f + 101.33f * float(k);
        char n[4]; snprintf(n, sizeof n, "%u", k + 1);
        text(c, x, 784, n, 20);
        char id[16]; snprintf(id, sizeof id, "##prog%u", k);
        key(c, x, 855, 72, 72, k < 8 ? 6u : 5u, 7u - (k % 8), id, kPad, int(k / 8), int(k % 8));
        text(c, x, 907, l1[k], 13); text(c, x, 923, l2[k], 12, 0, true);
    }
}

} // namespace VPanel
