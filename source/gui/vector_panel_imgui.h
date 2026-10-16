// vector_panel_imgui.h - VPanel::Backend on an ImGui draw list (the standalone's thin GUI). Items are ImGui
// InvisibleButtons, so the panel behaves in the standalone exactly as before VST3-2.
#pragma once
#include "imgui.h"
#include "vector_panel.h"

namespace VPanel {

struct ImGuiBackend final : Backend {
    ImDrawList* dl = nullptr;
    ImFont* f = nullptr;
    std::function<void(ImVec2, ImVec2)> lcdFn;
    ImTextureID logoTex = 0;                                               // <--- 1. Texture handle

    static ImVec2 I(V2 v) { return ImVec2(v.x, v.y); }
    void rectFilled(V2 a, V2 b, uint32_t col, float r) override { dl->AddRectFilled(I(a), I(b), col, r); }
    void rect(V2 a, V2 b, uint32_t col, float r, float t) override { dl->AddRect(I(a), I(b), col, r, 0, t); }
    void circleFilled(V2 c, float r, uint32_t col, int seg) override { dl->AddCircleFilled(I(c), r, col, seg); }
    void circle(V2 c, float r, uint32_t col, int seg, float t) override { dl->AddCircle(I(c), r, col, seg, t); }
    void line(V2 a, V2 b, uint32_t col, float t) override { dl->AddLine(I(a), I(b), col, t); }
    V2 textSize(float px, const char* t) override { const ImVec2 s = f->CalcTextSizeA(px, 1e9f, 0.0f, t); return V2(s.x, s.y); }
    void text(V2 p, float px, uint32_t col, const char* t) override { dl->AddText(f, px, I(p), col, t); }
    Item item(const char* id, V2 a, V2 b) override
    {
        ImGui::SetCursorScreenPos(I(a));
        ImGui::InvisibleButton(id, ImVec2(b.x - a.x, b.y - a.y));
        const ImGuiIO& io = ImGui::GetIO();
        Item it;
        it.active = ImGui::IsItemActive();
        it.hovered = ImGui::IsItemHovered();
        it.dblClick = it.hovered && ImGui::IsMouseDoubleClicked(0);
        it.shift = io.KeyShift;
        it.dragY = it.active ? io.MouseDelta.y : 0.0f;
        it.wheel = it.hovered ? io.MouseWheel : 0.0f;
        return it;
    }
    void tooltip(const char* t) override { ImGui::SetTooltip("%s", t); }
    void lcd(V2 a, V2 b) override { if (lcdFn) lcdFn(I(a), I(b)); }
    void logo(V2 a, V2 b, uint32_t tint) override                          // <--- 2. Implement logo
    {
        if (logoTex)
            dl->AddImage(logoTex, I(a), I(b), ImVec2(0, 0), ImVec2(1, 1), tint);
    }
};

} // namespace VPanel
