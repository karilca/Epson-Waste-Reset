#include "imgui.h"
#include <cstdio>
#include <cstring>

static ImGuiIO g_io;
static ImGuiStyle g_style;
static ImGui::ImGuiContext g_context = (ImGui::ImGuiContext)0xBAADF00D;

namespace ImGui {

ImGuiContext CreateContext(void* shared_font_atlas) {
    return g_context;
}

void DestroyContext(ImGuiContext ctx) {
}

ImGuiIO& GetIO() {
    return g_io;
}

ImGuiStyle& GetStyle() {
    return g_style;
}

void StyleColorsDark(ImGuiStyle* dst) {
}

bool Begin(const char* name, bool* p_open, ImGuiWindowFlags flags) {
    MockWidget w;
    w.type = "Window";
    w.label_or_text = name;
    MockImGuiState::Get().AddWidget(w);
    return true;
}

void End() {
}

void TextV(const char* fmt, va_list args) {
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    MockWidget w;
    w.type = "Text";
    w.label_or_text = buf;
    MockImGuiState::Get().AddWidget(w);
}

void Text(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    TextV(fmt, args);
    va_end(args);
}

void TextWrapped(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    TextV(fmt, args);
    va_end(args);
}

void TextColored(const ImVec4& col, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    TextV(fmt, args);
    va_end(args);
}

void TextUnformatted(const char* text, const char* text_end) {
    MockWidget w;
    w.type = "Text";
    w.label_or_text = text_end ? std::string(text, text_end) : std::string(text);
    MockImGuiState::Get().AddWidget(w);
}

bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags, void* callback, void* user_data) {
    MockWidget w;
    w.type = "InputText";
    w.label_or_text = label;
    MockImGuiState::Get().AddWidget(w);

    std::string simulated;
    if (MockImGuiState::Get().HasSimulatedInput(label, simulated)) {
        strncpy(buf, simulated.c_str(), buf_size - 1);
        buf[buf_size - 1] = '\0';
        return true;
    }
    return false;
}

bool Button(const char* label, const ImVec2& size) {
    MockWidget w;
    w.type = "Button";
    w.label_or_text = label;
    MockImGuiState::Get().AddWidget(w);

    return MockImGuiState::Get().WasButtonClicked(label);
}

bool BeginListBox(const char* label, const ImVec2& size) {
    MockWidget w;
    w.type = "ListBox";
    w.label_or_text = label;
    MockImGuiState::Get().AddWidget(w);
    return true;
}

void EndListBox() {
}

bool Selectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size) {
    MockWidget w;
    w.type = "Selectable";
    w.label_or_text = label;
    w.selected = selected;
    MockImGuiState::Get().AddWidget(w);

    return MockImGuiState::Get().WasSelectableClicked(label);
}

void SameLine(float offset_from_start_x, float spacing) {}
void Separator() {}
void ProgressBar(float fraction, const ImVec2& size_arg, const char* overlay) {
    MockWidget w;
    w.type = "ProgressBar";
    char buf[128];
    snprintf(buf, sizeof(buf), "%.2f", fraction);
    w.label_or_text = buf;
    MockImGuiState::Get().AddWidget(w);
}

void Dummy(const ImVec2& size) {}

void PushStyleColor(ImGuiCol idx, const ImVec4& col) {}
void PopStyleColor(int count) {}

static ImGuiViewport g_viewport;

const ImGuiViewport* GetMainViewport() {
    return &g_viewport;
}

void SetNextWindowPos(const ImVec2& pos, int cond, const ImVec2& pivot) {}
void SetNextWindowSize(const ImVec2& size, int cond) {}

void PushStyleVar(ImGuiStyleVar idx, float val) {}
void PushStyleVar(ImGuiStyleVar idx, const ImVec2& val) {}
void PopStyleVar(int count) {}

void SetScrollHereY(float center_y_ratio) {}

void Render() {}
void* GetDrawData() { return nullptr; }
void NewFrame() {}

} // namespace ImGui
