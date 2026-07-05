#include "gui/gui_app.h"
#include "imgui.h"
#include "ewr/usb.h"
#include "ewr/generator.h"
#include "ewr/parser.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <chrono>
#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#endif

namespace ewr {

// ============================================================================
// LogBuffer Implementation
// ============================================================================

LogBuffer::LogBuffer() : old_cout_(nullptr), old_cerr_(nullptr), is_captured_(false) {}

LogBuffer::~LogBuffer() {
    ReleaseCapture();
}

void LogBuffer::RegisterCapture() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_captured_) {
        old_cout_ = std::cout.rdbuf(this);
        old_cerr_ = std::cerr.rdbuf(this);
        is_captured_ = true;
    }
}

void LogBuffer::ReleaseCapture() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_captured_) {
        std::cout.rdbuf(old_cout_);
        std::cerr.rdbuf(old_cerr_);
        old_cout_ = nullptr;
        old_cerr_ = nullptr;
        is_captured_ = false;
    }
}

void LogBuffer::Append(const char* s, size_t n) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_.size() + n > 1000000) { // Limit log buffer size to 1MB
        buffer_.erase(0, 10000); // Discard oldest entries
    }
    buffer_.append(s, n);
}

int LogBuffer::overflow(int c) {
    if (c != EOF) {
        char ch = static_cast<char>(c);
        Append(&ch, 1);
    }
    return c;
}

std::streamsize LogBuffer::xsputn(const char* s, std::streamsize n) {
    Append(s, n);
    return n;
}

std::string LogBuffer::GetLogs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_;
}

void LogBuffer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
}

// ============================================================================
// EwrGuiApp Implementation
// ============================================================================

EwrGuiApp::EwrGuiApp() : 
    has_admin_privileges_(false),
    ota_sync_started_(false),
    ota_sync_running_(false),
    ota_sync_success_(false),
    ota_status_text_("Idle"),
    reset_running_(false),
    reset_success_(false),
    reset_progress_(0.0f),
    reset_status_text_("Idle"),
    shutdown_requested_(false),
    is_printer_connected_(false),
    connected_printer_pid_(0),
    last_scan_time_(0.0),
    read_running_(false),
    read_success_(false),
    read_status_text_("Idle")
{
    log_buffer_.RegisterCapture();
}

EwrGuiApp::~EwrGuiApp() {
    Shutdown();
    log_buffer_.ReleaseCapture();
}

void EwrGuiApp::Shutdown() {
    shutdown_requested_ = true;
    if (ota_thread_.joinable()) {
        ota_thread_.join();
    }
    if (reset_thread_.joinable()) {
        reset_thread_.join();
    }
    if (read_thread_.joinable()) {
        read_thread_.join();
    }
}

bool EwrGuiApp::CheckAdminPrivileges() const {
#ifdef _WIN32
    // Check if running as administrator on Windows
    // We declare IsUserAnAdmin as dynamic / external symbol to resolve with Windows SDK
    // Since Windows SDK headers might declare it, we just call it.
    typedef BOOL (WINAPI *PFN_IsUserAnAdmin)(VOID);
    HMODULE hShell = LoadLibraryA("shell32.dll");
    if (hShell) {
        PFN_IsUserAnAdmin pfn = (PFN_IsUserAnAdmin)GetProcAddress(hShell, "IsUserAnAdmin");
        if (pfn) {
            BOOL res = pfn();
            FreeLibrary(hShell);
            return res != FALSE;
        }
        FreeLibrary(hShell);
    }
    return false;
#else
    // Check effective UID on Linux
    return geteuid() == 0;
#endif
}

void EwrGuiApp::Initialize() {
    has_admin_privileges_ = CheckAdminPrivileges();
    ApplyPremiumTheme();

    // Check if printer is connected on startup
    is_printer_connected_ = IsEpsonPrinterConnected(connected_printer_pid_);

    // Load local database immediately if exists as fallback
    std::cout << "[INFO] Loading local cached database..." << std::endl;
    generator_.LoadDatabase("database.json");
    smart_models_ = generator_.GetAvailableModels();
    
    // Scan custom models
    custom_models_ = ScanModelsFolder("models");
    
    // Start OTA Sync asynchronously
    if (ota_sync_running_) return;
    if (ota_thread_.joinable()) {
        ota_thread_.join();
    }
    ota_sync_running_ = true;
    ota_status_text_ = "Syncing...";
    ota_thread_ = std::thread(&EwrGuiApp::RunOtaSyncThread, this);
}

void EwrGuiApp::RunOtaSyncThread() {
    std::cout << "[INFO] Starting OTA database sync..." << std::endl;
    bool success = generator_.SyncDatabaseOTA();
    
    if (success) {
        std::cout << "[INFO] OTA sync successful. Loading updated database..." << std::endl;
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (generator_.LoadDatabase("database.json")) {
            smart_models_ = generator_.GetAvailableModels();
            ota_status_text_ = "Synced";
            ota_sync_success_ = true;
        } else {
            std::cout << "[WARNING] Failed to load downloaded database. Falling back..." << std::endl;
            generator_.LoadDatabase("database.json");
            smart_models_ = generator_.GetAvailableModels();
            ota_status_text_ = "Failed (Using cache)";
            ota_sync_success_ = false;
        }
    } else {
        std::cout << "[WARNING] OTA sync failed. Falling back to local database.json..." << std::endl;
        std::lock_guard<std::mutex> lock(data_mutex_);
        generator_.LoadDatabase("database.json");
        smart_models_ = generator_.GetAvailableModels();
        ota_status_text_ = "Failed (Using cache)";
        ota_sync_success_ = false;
    }
    ota_sync_running_ = false;
}

static bool CaseInsensitiveContains(const std::string& str, const std::string& query) {
    if (query.empty()) return true;
    auto it = std::search(
        str.begin(), str.end(),
        query.begin(), query.end(),
        [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return it != str.end();
}

static std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

void EwrGuiApp::RenderUI() {
    // Apply styling (Dark Theme)
    ImGui::StyleColorsDark();
    
    // Periodic printer connection scan (every 2 seconds)
    double now = ImGui::GetTime();
    if (now - last_scan_time_ > 2.0) {
        is_printer_connected_ = IsEpsonPrinterConnected(connected_printer_pid_);
        last_scan_time_ = now;
    }

    // Make the ImGui window cover the entire GLFW window viewport (making it feel native)
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | 
                                    ImGuiWindowFlags_NoCollapse | 
                                    ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoMove | 
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | 
                                    ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 15.0f));
    
    ImGui::Begin("Epson Waste Ink Pad Resetter", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    // F6: Administrative Privilege Warning Banner
    if (!has_admin_privileges_) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.35f, 0.10f, 0.10f, 1.00f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::BeginChild("AdminWarning", ImVec2(0, 32), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.8f, 1.0f), "WARNING: Administrative privileges are required! Please run the application with root/administrator privileges.");
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    
    // Header section: version, sync status, connection status
    ImGui::Text("EWR Utility v1.1.0");
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Connection status indicator
    if (is_printer_connected_) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "●");
        ImGui::SameLine();
        ImGui::Text("Printer: Connected (PID: %04X)", connected_printer_pid_);
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "●");
        ImGui::SameLine();
        ImGui::Text("Printer: Disconnected");
    }
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // OTA sync status indicator
    if (ota_status_text_ == "Synced") {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "●");
    } else if (ota_status_text_ == "Syncing...") {
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), "●");
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "●");
    }
    ImGui::SameLine();
    ImGui::Text("DB Sync: %s", ota_status_text_.c_str());
    
    ImGui::Separator();
    ImGui::Spacing();

    // Split Layout
    ImGui::Columns(2, "MainLayout", false);
    
    // Set left column width to 40% of the viewport width
    ImGui::SetColumnWidth(0, viewport->WorkSize.x * 0.38f);

    // ==========================================
    // LEFT COLUMN: Pretraga i popis modela
    // ==========================================
    ImGui::Text("Model Selection");
    ImGui::Spacing();
    
    // F2: Real-time search
    char buf[256];
    std::strncpy(buf, search_query_.c_str(), sizeof(buf));
    ImGui::PushItemWidth(-1.0f); // Fill column width
    if (ImGui::InputTextWithHint("##SearchModels", "Search printer models...", buf, sizeof(buf))) {
        search_query_ = buf;
    }
    ImGui::PopItemWidth();
    
    ImGui::Spacing();
    
    // Perform search filtering
    std::string query = Trim(search_query_);
    std::vector<std::string> matches;
    
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        for (const auto& m : smart_models_) {
            if (CaseInsensitiveContains(m.name, query)) {
                matches.push_back(m.name);
            }
        }
        for (const auto& m : custom_models_) {
            if (CaseInsensitiveContains(m.name, query)) {
                if (std::find(matches.begin(), matches.end(), m.name) == matches.end()) {
                    matches.push_back(m.name);
                }
            }
        }
    }
    
    // Available Models ListBox
    ImGui::Text("Available Models (%zu matches):", matches.size());
    if (ImGui::BeginListBox("##ModelsList", ImVec2(-1.0f, 270.0f))) {
        if (matches.empty()) {
            ImGui::Text("No results found");
        } else {
            std::sort(matches.begin(), matches.end());
            for (const auto& name : matches) {
                bool is_selected = (name == selected_model_name_);
                if (ImGui::Selectable(name.c_str(), is_selected)) {
                    selected_model_name_ = name;
                }
            }
        }
        ImGui::EndListBox();
    }
    
    // ==========================================
    // RIGHT COLUMN: Detalji, reset, read
    // ==========================================
    ImGui::NextColumn();
    
    ImGui::Text("Model Details");
    ImGui::Spacing();
    
    DbPrinterModel selected_smart_model;
    bool selected_is_smart = false;
    PrinterModel selected_custom_model;
    
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        for (const auto& m : smart_models_) {
            if (m.name == selected_model_name_) {
                selected_smart_model = m;
                selected_is_smart = true;
                break;
            }
        }
        if (!selected_is_smart) {
            for (const auto& m : custom_models_) {
                if (m.name == selected_model_name_) {
                    selected_custom_model = m;
                    break;
                }
            }
        }
    }

    ImGui::BeginChild("DetailsContainer", ImVec2(-1.0f, 110.0f), true);
    if (selected_model_name_.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Please select a printer model from the list.");
    } else {
        ImGui::Text("Selected Model: %s", selected_model_name_.c_str());
        if (selected_is_smart) {
            ImGui::Text("Type: Smart Protocol");
            ImGui::Text("Read Key: %u", selected_smart_model.rkey);
            ImGui::Text("Write Key: %s", selected_smart_model.wkey.c_str());
            ImGui::Text("Reset Target Addresses: %zu", selected_smart_model.addresses.size());
        } else {
            ImGui::Text("Type: Replay Model");
            ImGui::Text("Wireshark Dump: %s", selected_custom_model.filepath.c_str());
        }
    }
    ImGui::EndChild();
    
    ImGui::Spacing();
    
    // Diagnostics (Counter Reading)
    ImGui::Text("Counter Diagnostics");
    ImGui::Spacing();
    ImGui::BeginChild("DiagnosticsContainer", ImVec2(-1.0f, 95.0f), true);
    
    bool can_read = selected_is_smart && !read_running_ && !reset_running_ && !selected_model_name_.empty();
    if (!can_read) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Button("Read Waste Pad Counter");
        ImGui::PopStyleColor(2);
    } else {
        if (ImGui::Button("Read Waste Pad Counter")) {
            TriggerReadCounters();
        }
    }
    
    ImGui::SameLine();
    if (read_running_) {
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), "Reading EEPROM...");
    } else if (read_status_text_ != "Idle") {
        if (read_success_) {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Read Success!");
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Read Failed: %s", read_status_text_.c_str());
        }
    } else {
        ImGui::Text("Status: Idle");
    }
    
    if (read_success_ && !read_values_.empty()) {
        ImGui::Spacing();
        if (read_values_.size() == 1 && selected_smart_model.addresses.size() >= 1) {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Counter Value (Address %u): %u", selected_smart_model.addresses[0], read_values_[0]);
        } else if (read_values_.size() >= 2 && selected_smart_model.addresses.size() >= 2) {
            uint16_t combined = read_values_[0] | (read_values_[1] << 8);
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Counters: Address %u = %u, Address %u = %u", 
                               selected_smart_model.addresses[0], read_values_[0],
                               selected_smart_model.addresses[1], read_values_[1]);
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Combined Value: %u", combined);
        }
    }
    ImGui::EndChild();
    
    ImGui::Spacing();
    
    // Action panel
    ImGui::Text("Maintenance Actions");
    ImGui::Spacing();
    ImGui::BeginChild("ResetContainer", ImVec2(-1.0f, 95.0f), true);
    
    bool can_reset = !selected_model_name_.empty() && !reset_running_ && !read_running_;
    if (!can_reset) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Button("Reset Waste Ink Pad");
        ImGui::PopStyleColor(2);
    } else {
        if (ImGui::Button("Reset Waste Ink Pad")) {
            TriggerReset();
        }
    }
    
    if (reset_running_) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            shutdown_requested_ = true;
        }
        ImGui::Text("Status: %s", reset_status_text_.c_str());
        ImGui::ProgressBar(reset_progress_);
    } else if (reset_status_text_ != "Idle") {
        if (reset_success_) {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Status: %s", reset_status_text_.c_str());
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Please turn the printer OFF and ON using the physical power button.");
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Status: %s", reset_status_text_.c_str());
        }
    } else {
        ImGui::Text("Status: Idle");
    }
    ImGui::EndChild();
    
    // Restore layout columns
    ImGui::Columns(1);
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // F7: Log Console Output
    ImGui::Text("Log Console Output");
    ImGui::SameLine();
    if (ImGui::Button("Clear Logs")) {
        ClearLogs();
    }
    
    std::string logs = log_buffer_.GetLogs();
    if (ImGui::BeginListBox("##LogsList", ImVec2(-1.0f, 120.0f))) {
        std::stringstream ss(logs);
        std::string line;
        while (std::getline(ss, line)) {
            ImVec4 col = ImVec4(0.92f, 0.92f, 0.95f, 1.00f); // default white/grey
            if (line.rfind("[ERROR]", 0) == 0 || line.rfind("[!]", 0) == 0) {
                col = ImVec4(0.9f, 0.3f, 0.3f, 1.00f); // Red
            } else if (line.rfind("[WARNING]", 0) == 0) {
                col = ImVec4(0.9f, 0.6f, 0.2f, 1.00f); // Orange
            } else if (line.rfind("[SUCCESS]", 0) == 0) {
                col = ImVec4(0.3f, 0.9f, 0.3f, 1.00f); // Green
            } else if (line.rfind("[INFO]", 0) == 0) {
                col = ImVec4(0.3f, 0.7f, 0.9f, 1.00f); // Cyan
            }
            ImGui::TextColored(col, "%s", line.c_str());
        }
        ImGui::SetScrollHereY(1.0f);
        ImGui::EndListBox();
    }
    
    ImGui::End();
}

void EwrGuiApp::TriggerReset() {
    if (reset_running_) return;
    
    reset_running_ = true;
    reset_success_ = false;
    reset_progress_ = 0.0f;
    reset_status_text_ = "Initiating...";
    shutdown_requested_ = false;
    
    if (reset_thread_.joinable()) {
        reset_thread_.join();
    }
    
    reset_thread_ = std::thread(&EwrGuiApp::RunResetThread, this);
}

void EwrGuiApp::RunResetThread() {
    std::cout << "[INFO] Initiating waste ink pad reset sequence..." << std::endl;
    reset_progress_ = 0.1f;
    reset_status_text_ = "Connecting to printer...";
    
    EwrDeviceHandle hPrinter = AutoConnectEpsonPrinter();
    if (!hPrinter) {
        std::cerr << "[ERROR] Could not find an Epson printer" << std::endl;
        std::lock_guard<std::mutex> lock(data_mutex_);
        reset_status_text_ = "Error: Printer not found";
        reset_running_ = false;
        return;
    }
    
    if (shutdown_requested_) {
        DisconnectPrinter(hPrinter);
        std::lock_guard<std::mutex> lock(data_mutex_);
        reset_status_text_ = "Cancelled";
        reset_running_ = false;
        return;
    }
    
    reset_progress_ = 0.3f;
    reset_status_text_ = "Generating payload sequence...";
    
    bool is_smart = false;
    DbPrinterModel smart_model;
    PrinterModel custom_model;
    
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        for (const auto& m : smart_models_) {
            if (m.name == selected_model_name_) {
                is_smart = true;
                smart_model = m;
                break;
            }
        }
        if (!is_smart) {
            for (const auto& m : custom_models_) {
                if (m.name == selected_model_name_) {
                    custom_model = m;
                    break;
                }
            }
        }
    }
    
    std::vector<std::vector<unsigned char>> sequence;
    if (is_smart) {
        sequence = generator_.GenerateSequence(smart_model);
    } else {
        sequence = ParseWiresharkDump(custom_model.filepath);
    }
    
    if (sequence.empty()) {
        std::cerr << "[ERROR] Generated reset sequence is empty!" << std::endl;
        DisconnectPrinter(hPrinter);
        std::lock_guard<std::mutex> lock(data_mutex_);
        reset_status_text_ = "Error: Empty payload";
        reset_running_ = false;
        return;
    }
    
    if (shutdown_requested_) {
        DisconnectPrinter(hPrinter);
        std::lock_guard<std::mutex> lock(data_mutex_);
        reset_status_text_ = "Cancelled";
        reset_running_ = false;
        return;
    }
    
    reset_progress_ = 0.5f;
    reset_status_text_ = "Executing sequence...";
    
    bool execute_success = true;
    execute_success = ExecutePayloadSequence(hPrinter, sequence, &shutdown_requested_, &reset_progress_);
    
    DisconnectPrinter(hPrinter);
    
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (shutdown_requested_) {
        reset_status_text_ = "Cancelled";
    } else if (execute_success) {
        reset_status_text_ = "Success! Ink pad reset completed.";
        reset_success_ = true;
    } else {
        reset_status_text_ = "Error: Reset sequence failed";
    }
    
    reset_progress_ = 1.0f;
    reset_running_ = false;
}

// Getters and input simulation helpers for test runner
void EwrGuiApp::SetSearchQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    search_query_ = query;
}

std::string EwrGuiApp::GetSearchQuery() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return search_query_;
}

std::vector<std::string> EwrGuiApp::GetFilteredModelNames() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::string query = Trim(search_query_);
    std::vector<std::string> matches;
    
    for (const auto& m : smart_models_) {
        if (CaseInsensitiveContains(m.name, query)) {
            matches.push_back(m.name);
        }
    }
    for (const auto& m : custom_models_) {
        if (CaseInsensitiveContains(m.name, query)) {
            if (std::find(matches.begin(), matches.end(), m.name) == matches.end()) {
                matches.push_back(m.name);
            }
        }
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

void EwrGuiApp::SelectModel(const std::string& modelName) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    selected_model_name_ = modelName;
}

std::string EwrGuiApp::GetSelectedModelName() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return selected_model_name_;
}

bool EwrGuiApp::IsResetButtonEnabled() const {
    return !selected_model_name_.empty() && !reset_running_;
}

bool EwrGuiApp::IsWarningBannerVisible() const {
    return !has_admin_privileges_;
}

std::string EwrGuiApp::GetWarningBannerText() const {
    if (!has_admin_privileges_) {
        return "WARNING: Administrative privileges are required! Please run the application with root/administrator privileges.";
    }
    return "";
}

std::string EwrGuiApp::GetConsoleOutput() const {
    return log_buffer_.GetLogs();
}

float EwrGuiApp::GetResetProgress() const {
    return reset_progress_;
}

std::string EwrGuiApp::GetResetStatusText() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return reset_status_text_;
}

std::string EwrGuiApp::GetOtaStatusText() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return ota_status_text_;
}

bool EwrGuiApp::IsResetRunning() const {
    return reset_running_;
}

bool EwrGuiApp::IsOtaSyncRunning() const {
    return ota_sync_running_;
}

void EwrGuiApp::ClearLogs() {
    log_buffer_.Clear();
}

bool EwrGuiApp::IsPrinterConnected() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return is_printer_connected_;
}

uint16_t EwrGuiApp::GetConnectedPrinterPid() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return connected_printer_pid_;
}

bool EwrGuiApp::IsReadRunning() const {
    return read_running_;
}

bool EwrGuiApp::IsReadSuccess() const {
    return read_success_;
}

std::vector<uint8_t> EwrGuiApp::GetReadValues() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return read_values_;
}

std::string EwrGuiApp::GetReadStatusText() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return read_status_text_;
}

void EwrGuiApp::TriggerReadCounters() {
    if (read_running_ || selected_model_name_.empty()) return;

    read_running_ = true;
    read_success_ = false;
    read_status_text_ = "Initiating read...";
    read_values_.clear();

    if (read_thread_.joinable()) {
        read_thread_.join();
    }

    read_thread_ = std::thread(&EwrGuiApp::RunReadThread, this);
}

void EwrGuiApp::RunReadThread() {
    std::cout << "[INFO] Initiating EEPROM read sequence..." << std::endl;
    
    EwrDeviceHandle hPrinter = AutoConnectEpsonPrinter();
    if (!hPrinter) {
        std::cerr << "[ERROR] Could not find an Epson printer to read counter values" << std::endl;
        std::lock_guard<std::mutex> lock(data_mutex_);
        read_status_text_ = "Error: Printer not found";
        read_running_ = false;
        return;
    }

    bool is_smart = false;
    DbPrinterModel smart_model;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        for (const auto& m : smart_models_) {
            if (m.name == selected_model_name_) {
                is_smart = true;
                smart_model = m;
                break;
            }
        }
    }

    if (!is_smart) {
        std::cerr << "[ERROR] Counter reading is only supported for Smart Protocol models." << std::endl;
        DisconnectPrinter(hPrinter);
        std::lock_guard<std::mutex> lock(data_mutex_);
        read_status_text_ = "Error: Model not supported";
        read_running_ = false;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        read_status_text_ = "Reading EEPROM...";
    }

    bool success = true;
    std::vector<uint8_t> values;
    for (uint16_t addr : smart_model.addresses) {
        uint8_t val = 0;
        if (ReadEEPROMAddress(hPrinter, smart_model.rkey, addr, val)) {
            values.push_back(val);
            std::cout << "[SUCCESS] Read EEPROM Address " << addr << ": " << (int)val << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to read EEPROM Address " << addr << std::endl;
            success = false;
            break;
        }
    }

    DisconnectPrinter(hPrinter);

    std::lock_guard<std::mutex> lock(data_mutex_);
    if (success) {
        read_values_ = values;
        read_success_ = true;
        read_status_text_ = "Success";
    } else {
        read_status_text_ = "Error: Read failed";
    }
    read_running_ = false;
}

void EwrGuiApp::ApplyPremiumTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.WindowBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_Border]                 = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.35f, 0.35f, 0.38f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.45f, 0.45f, 0.48f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]        = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.14f, 0.22f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.20f, 0.32f, 0.54f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.26f, 0.40f, 0.68f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.28f, 0.48f, 0.80f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.22f, 0.34f, 0.58f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.40f, 0.68f, 0.80f);
}

} // namespace ewr
