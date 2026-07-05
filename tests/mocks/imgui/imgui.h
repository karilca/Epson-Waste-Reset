#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdarg>
#include <cstring>

// Standard ImGui type definitions for compiler compatibility
struct ImVec2 {
    float x, y;
    ImVec2() : x(0.0f), y(0.0f) {}
    ImVec2(float _x, float _y) : x(_x), y(_y) {}
};

struct ImVec4 {
    float x, y, z, w;
    ImVec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    ImVec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

typedef int ImGuiWindowFlags;
typedef int ImGuiInputTextFlags;
typedef int ImGuiSelectableFlags;
typedef int ImGuiConfigFlags;

enum ImGuiConfigFlags_ {
    ImGuiConfigFlags_None = 0,
    ImGuiConfigFlags_NavEnableKeyboard = 1 << 0,
    ImGuiConfigFlags_NavEnableGamepad = 1 << 1
};

struct ImGuiViewport {
    ImVec2 Pos;
    ImVec2 Size;
    ImVec2 WorkPos;
    ImVec2 WorkSize;
    ImGuiViewport() : Pos(0,0), Size(800,600), WorkPos(0,0), WorkSize(800,600) {}
};

struct ImGuiIO {
    ImVec2 DisplaySize;
    float DeltaTime;
    float Framerate;
    ImGuiConfigFlags ConfigFlags;
    ImGuiIO() : DisplaySize(1280, 720), DeltaTime(1.0f/60.0f), Framerate(60.0f), ConfigFlags(0) {}
};

struct ImGuiStyle {
    ImVec4 Colors[80];
    float WindowRounding;
    float FrameRounding;
    float PopupRounding;
    float ScrollbarRounding;
    float GrabRounding;
    float TabRounding;
    ImVec2 FramePadding;
    ImVec2 ItemSpacing;
    float WindowBorderSize;
};

enum ImGuiWindowFlags_ {
    ImGuiWindowFlags_None = 0,
    ImGuiWindowFlags_NoTitleBar = 1 << 0,
    ImGuiWindowFlags_NoResize = 1 << 1,
    ImGuiWindowFlags_NoMove = 1 << 2,
    ImGuiWindowFlags_NoCollapse = 1 << 5,
    ImGuiWindowFlags_NoBringToFrontOnFocus = 1 << 13,
    ImGuiWindowFlags_NoNavFocus = 1 << 15,
    ImGuiWindowFlags_NoScrollbar = 1 << 16
};

enum ImGuiStyleVar_ {
    ImGuiStyleVar_WindowRounding,
    ImGuiStyleVar_WindowBorderSize,
    ImGuiStyleVar_WindowPadding,
    ImGuiStyleVar_ChildRounding
};

struct MockWidget {
    std::string type; // "Text", "Button", "InputText", "Selectable", "ProgressBar", "Window"
    std::string label_or_text;
    bool enabled = true;
    bool selected = false;
};

typedef int ImGuiCol;
enum ImGuiCol_ {
    ImGuiCol_Text,
    ImGuiCol_TextDisabled,
    ImGuiCol_WindowBg,
    ImGuiCol_ChildBg,
    ImGuiCol_PopupBg,
    ImGuiCol_Border,
    ImGuiCol_BorderShadow,
    ImGuiCol_FrameBg,
    ImGuiCol_FrameBgHovered,
    ImGuiCol_FrameBgActive,
    ImGuiCol_TitleBg,
    ImGuiCol_TitleBgActive,
    ImGuiCol_TitleBgCollapsed,
    ImGuiCol_MenuBarBg,
    ImGuiCol_ScrollbarBg,
    ImGuiCol_ScrollbarGrab,
    ImGuiCol_ScrollbarGrabHovered,
    ImGuiCol_ScrollbarGrabActive,
    ImGuiCol_CheckMark,
    ImGuiCol_SliderGrab,
    ImGuiCol_SliderGrabActive,
    ImGuiCol_Button,
    ImGuiCol_ButtonHovered,
    ImGuiCol_ButtonActive,
    ImGuiCol_Header,
    ImGuiCol_HeaderHovered,
    ImGuiCol_HeaderActive,
    ImGuiCol_PlotHistogram
};

// Simulation State Controller
class MockImGuiState {
public:
    static MockImGuiState& Get() {
        static MockImGuiState instance;
        return instance;
    }

    void Clear() {
        rendered_widgets.clear();
        clicked_buttons.clear();
        typed_texts.clear();
        selected_items.clear();
    }

    void AddWidget(const MockWidget& widget) {
        rendered_widgets.push_back(widget);
    }

    bool HasWidgetWithText(const std::string& type, const std::string& text) const {
        for (const auto& w : rendered_widgets) {
            if (w.type == type && w.label_or_text.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    bool HasButton(const std::string& label) const {
        return HasWidgetWithText("Button", label);
    }

    bool HasText(const std::string& text) const {
        for (const auto& w : rendered_widgets) {
            if (w.type == "Text" && w.label_or_text.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    bool HasSelectable(const std::string& label) const {
        return HasWidgetWithText("Selectable", label);
    }

    // Input simulation helpers
    void SimulateButtonClick(const std::string& label) {
        clicked_buttons[label] = true;
    }

    bool WasButtonClicked(const std::string& label) {
        auto it = clicked_buttons.find(label);
        if (it != clicked_buttons.end() && it->second) {
            clicked_buttons[label] = false; // consume
            return true;
        }
        return false;
    }

    void SimulateInputText(const std::string& label, const std::string& value) {
        typed_texts[label] = value;
    }

    bool HasSimulatedInput(const std::string& label, std::string& out_val) {
        auto it = typed_texts.find(label);
        if (it != typed_texts.end()) {
            out_val = it->second;
            return true;
        }
        return false;
    }

    void SimulateSelectableClick(const std::string& label) {
        selected_items[label] = true;
    }

    bool WasSelectableClicked(const std::string& label) {
        auto it = selected_items.find(label);
        if (it != selected_items.end() && it->second) {
            selected_items[label] = false; // consume
            return true;
        }
        return false;
    }

    std::vector<MockWidget> rendered_widgets;
    std::unordered_map<std::string, bool> clicked_buttons;
    std::unordered_map<std::string, std::string> typed_texts;
    std::unordered_map<std::string, bool> selected_items;
};

// ImGui namespace functions
namespace ImGui {
    typedef void* ImGuiContext;
    
    ImGuiContext CreateContext(void* shared_font_atlas = nullptr);
    void DestroyContext(ImGuiContext ctx = nullptr);
    ImGuiIO& GetIO();
    ImGuiStyle& GetStyle();
    
    void StyleColorsDark(ImGuiStyle* dst = nullptr);
    
    bool Begin(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0);
    void End();
    
    void Text(const char* fmt, ...);
    void TextV(const char* fmt, va_list args);
    void TextWrapped(const char* fmt, ...);
    void TextColored(const ImVec4& col, const char* fmt, ...);
    void TextUnformatted(const char* text, const char* text_end = nullptr);
    
    bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0, void* callback = nullptr, void* user_data = nullptr);
    bool Button(const char* label, const ImVec2& size = ImVec2(0, 0));
    
    bool BeginListBox(const char* label, const ImVec2& size = ImVec2(0, 0));
    void EndListBox();
    
    bool Selectable(const char* label, bool selected = false, ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0));
    
    void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f);
    void Separator();
    void ProgressBar(float fraction, const ImVec2& size_arg = ImVec2(-1.0f, 0.0f), const char* overlay = nullptr);
    void Dummy(const ImVec2& size);
    
    void PushStyleColor(ImGuiCol idx, const ImVec4& col);
    void PopStyleColor(int count = 1);
    
    void SetColumnWidth(int column_index, float width);
    void PushItemWidth(float item_width);
    void PopItemWidth();
    
    const ImGuiViewport* GetMainViewport();
    void SetNextWindowPos(const ImVec2& pos, int cond = 0, const ImVec2& pivot = ImVec2(0,0));
    void SetNextWindowSize(const ImVec2& size, int cond = 0);

    typedef int ImGuiStyleVar;
    void PushStyleVar(ImGuiStyleVar idx, float val);
    void PushStyleVar(ImGuiStyleVar idx, const ImVec2& val);
    void PopStyleVar(int count = 1);
    
    void SetScrollHereY(float center_y_ratio = 0.5f);
    
    bool BeginChild(const char* str_id, const ImVec2& size = ImVec2(0,0), bool border = false, ImGuiWindowFlags flags = 0);
    void EndChild();
    void Columns(int count = 1, const char* id = nullptr, bool border = true);
    void NextColumn();
    bool InputTextWithHint(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0, void* callback = nullptr, void* user_data = nullptr);
    void Spacing();
    double GetTime();

    void Render();
    void* GetDrawData();
    void NewFrame();
    void SetClipboardText(const char* text);
}
