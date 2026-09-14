#include "style.h"

#include <windows.h>

#include <string>

namespace
{
    bool FontExists(const char* path)
    {
        return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
    }

    ImVec4 HexColor(int r, int g, int b, int a = 255)
    {
        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }
}

namespace flstyle
{
    ImFont* Bold = nullptr;
    ImFont* Small = nullptr;

    void Apply(float dpiScale)
    {
        ImGuiIO& io = ImGui::GetIO();

        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;

        const char* regular = "C:\\Windows\\Fonts\\segoeui.ttf";
        const char* bold = "C:\\Windows\\Fonts\\segoeuib.ttf";

        const float mainSize = 17.0f * dpiScale;
        const float boldSize = 18.0f * dpiScale;
        const float smallSize = 13.0f * dpiScale;

        if (FontExists(regular))
        {
            io.Fonts->AddFontFromFileTTF(regular, mainSize, &cfg);
            if (FontExists(bold))
            {
                Bold = io.Fonts->AddFontFromFileTTF(bold, boldSize, &cfg);
            }
            Small = io.Fonts->AddFontFromFileTTF(regular, smallSize, &cfg);
        }
        else
        {
            io.Fonts->AddFontDefault();
        }

        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 8.0f;
        s.ChildRounding = 8.0f;
        s.FrameRounding = 6.0f;
        s.PopupRounding = 8.0f;
        s.ScrollbarRounding = 10.0f;
        s.GrabRounding = 6.0f;
        s.TabRounding = 7.0f;
        s.WindowPadding = ImVec2(14.0f, 14.0f);
        s.FramePadding = ImVec2(10.0f, 6.0f);
        s.ItemSpacing = ImVec2(10.0f, 8.0f);
        s.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        s.IndentSpacing = 22.0f;
        s.ScrollbarSize = 14.0f;
        s.GrabMinSize = 10.0f;
        s.WindowBorderSize = 0.0f;
        s.ChildBorderSize = 1.0f;
        s.PopupBorderSize = 1.0f;
        s.FrameBorderSize = 0.0f;
        s.TabBarBorderSize = 1.0f;

        ImVec4* c = s.Colors;

        c[ImGuiCol_Text] = HexColor(233, 236, 242);
        c[ImGuiCol_TextDisabled] = HexColor(139, 146, 160);
        c[ImGuiCol_TextLink] = HexColor(255, 179, 71);
        c[ImGuiCol_WindowBg] = HexColor(15, 17, 21);
        c[ImGuiCol_ChildBg] = HexColor(20, 23, 30);
        c[ImGuiCol_PopupBg] = HexColor(22, 25, 33, 250);
        c[ImGuiCol_Border] = HexColor(46, 52, 66);
        c[ImGuiCol_BorderShadow] = HexColor(0, 0, 0, 0);
        c[ImGuiCol_FrameBg] = HexColor(27, 31, 42);
        c[ImGuiCol_FrameBgHovered] = HexColor(35, 40, 52);
        c[ImGuiCol_FrameBgActive] = HexColor(43, 50, 64);
        c[ImGuiCol_TitleBg] = HexColor(20, 23, 30);
        c[ImGuiCol_TitleBgActive] = HexColor(20, 23, 30);
        c[ImGuiCol_TitleBgCollapsed] = HexColor(20, 23, 30);
        c[ImGuiCol_MenuBarBg] = HexColor(20, 23, 30);
        c[ImGuiCol_ScrollbarBg] = HexColor(15, 17, 21, 128);
        c[ImGuiCol_ScrollbarGrab] = HexColor(42, 48, 60);
        c[ImGuiCol_ScrollbarGrabHovered] = HexColor(57, 65, 90);
        c[ImGuiCol_ScrollbarGrabActive] = HexColor(255, 179, 71);
        c[ImGuiCol_CheckMark] = HexColor(255, 179, 71);
        c[ImGuiCol_SliderGrab] = HexColor(255, 179, 71);
        c[ImGuiCol_SliderGrabActive] = HexColor(255, 200, 115);
        c[ImGuiCol_Button] = HexColor(35, 40, 52);
        c[ImGuiCol_ButtonHovered] = HexColor(48, 55, 70);
        c[ImGuiCol_ButtonActive] = HexColor(60, 69, 88);
        c[ImGuiCol_Header] = HexColor(30, 35, 45);
        c[ImGuiCol_HeaderHovered] = HexColor(42, 48, 64);
        c[ImGuiCol_HeaderActive] = HexColor(50, 58, 76);
        c[ImGuiCol_Separator] = HexColor(46, 52, 66);
        c[ImGuiCol_SeparatorHovered] = HexColor(255, 179, 71);
        c[ImGuiCol_SeparatorActive] = HexColor(255, 179, 71);
        c[ImGuiCol_ResizeGrip] = HexColor(42, 48, 60, 128);
        c[ImGuiCol_ResizeGripHovered] = HexColor(255, 179, 71, 160);
        c[ImGuiCol_ResizeGripActive] = HexColor(255, 179, 71, 255);
        c[ImGuiCol_Tab] = HexColor(20, 23, 30);
        c[ImGuiCol_TabHovered] = HexColor(35, 40, 52);
        c[ImGuiCol_TabSelected] = HexColor(27, 31, 42);
        c[ImGuiCol_TabDimmed] = HexColor(20, 23, 30);
        c[ImGuiCol_TabDimmedSelected] = HexColor(27, 31, 42);
        c[ImGuiCol_PlotLines] = HexColor(255, 179, 71);
        c[ImGuiCol_PlotLinesHovered] = HexColor(255, 200, 115);
        c[ImGuiCol_PlotHistogram] = HexColor(255, 179, 71);
        c[ImGuiCol_PlotHistogramHovered] = HexColor(255, 200, 115);
        c[ImGuiCol_TableHeaderBg] = HexColor(24, 28, 37);
        c[ImGuiCol_TableBorderStrong] = HexColor(46, 52, 66);
        c[ImGuiCol_TableBorderLight] = HexColor(38, 43, 55);
        c[ImGuiCol_TableRowBg] = HexColor(0, 0, 0, 0);
        c[ImGuiCol_TableRowBgAlt] = HexColor(255, 255, 255, 8);
        c[ImGuiCol_TextSelectedBg] = HexColor(255, 179, 71, 90);
        c[ImGuiCol_DragDropTarget] = HexColor(255, 179, 71, 200);

        if (dpiScale != 1.0f)
        {
            const auto scale2 = [&dpiScale](ImVec2& v) {
                v = ImVec2(v.x * dpiScale, v.y * dpiScale);
            };
            scale2(s.WindowPadding);
            scale2(s.FramePadding);
            scale2(s.ItemSpacing);
            scale2(s.ItemInnerSpacing);
            s.IndentSpacing *= dpiScale;
            s.ScrollbarSize *= dpiScale;
            s.GrabMinSize *= dpiScale;
        }
    }
}
