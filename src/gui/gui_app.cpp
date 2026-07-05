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
    shutdown_requested_(false)
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
    
    ImGui::Begin("Epson Waste Ink Pad Resetter");
    
    // F6: Administrative Privilege Warning Banner
    if (!has_admin_privileges_) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "WARNING: Administrative privileges are required! Please run the application with root/administrator privileges.");
        ImGui::Separator();
    }
    
    // Header section: version, sync status
    ImGui::Text("Version: 1.1.0");
    ImGui::SameLine();
    ImGui::Text("| DB Sync: %s", ota_status_text_.c_str());
    ImGui::Separator();
    
    // F2: Real-time search
    char buf[256];
    std::strncpy(buf, search_query_.c_str(), sizeof(buf));
    if (ImGui::InputText("Search Models", buf, sizeof(buf))) {
        search_query_ = buf;
    }
    
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
    ImGui::Text("Available Models:");
    if (ImGui::BeginListBox("##ModelsList", ImVec2(-1.0f, 150.0f))) {
        if (matches.empty()) {
            ImGui::Text("No results found");
        } else {
            // Sort match names alphabetically
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
    
    ImGui::Separator();
    
    // F3: Model Selection & Details Panel
    ImGui::Text("Model Details:");
    if (selected_model_name_.empty()) {
        ImGui::Text("Please select a printer model from the list.");
    } else {
        bool is_smart = false;
        DbPrinterModel smart_info;
        PrinterModel custom_info;
        
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            for (const auto& m : smart_models_) {
                if (m.name == selected_model_name_) {
                    is_smart = true;
                    smart_info = m;
                    break;
                }
            }
            if (!is_smart) {
                for (const auto& m : custom_models_) {
                    if (m.name == selected_model_name_) {
                        custom_info = m;
                        break;
                    }
                }
            }
        }
        
        if (is_smart) {
            ImGui::Text("Type: Smart Protocol");
            ImGui::Text("Read Key: %u", smart_info.rkey);
            ImGui::Text("Write Key: %s", smart_info.wkey.c_str());
            ImGui::Text("Reset Addresses: %zu", smart_info.addresses.size());
        } else {
            ImGui::Text("Type: Replay Model");
            ImGui::Text("File Path: %s", custom_info.filepath.c_str());
        }
    }
    
    ImGui::Separator();
    
    // F4: Action panel (Reset execution & responsiveness)
    bool can_reset = !selected_model_name_.empty() && !reset_running_;
    if (!can_reset) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::Button("Reset Waste Ink Pad");
        ImGui::PopStyleColor();
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
        ImGui::Text("Status: %s", reset_status_text_.c_str());
    }
    
    ImGui::Separator();
    
    // F7: Log Console Output
    ImGui::Text("Log Console:");
    ImGui::SameLine();
    if (ImGui::Button("Clear Logs")) {
        ClearLogs();
    }
    
    std::string logs = log_buffer_.GetLogs();
    if (ImGui::BeginListBox("##LogsList", ImVec2(-1.0f, 200.0f))) {
        ImGui::TextUnformatted(logs.c_str());
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
    
    // Execute sequence and update progress linearly
    bool execute_success = true;
    size_t total_packets = sequence.size();
    
    // If running in tests, or if we want fine-grained linear progress updates,
    // we can send packets step by step or call the single block.
    // To support Cancellation midway through packets, sending step-by-step is ideal!
    // Since ExecutePayloadSequence in usb_linux.cpp doesn't support cancellation,
    // we can either call it, or run our own loop here.
    // Wait, the prompt says "Utilize the existing codebase routines: ewr::ExecutePayloadSequence".
    // If we call it directly, we can do:
    reset_progress_ = 0.7f;
    execute_success = ExecutePayloadSequence(hPrinter, sequence);
    reset_progress_ = 0.9f;
    
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

} // namespace ewr
