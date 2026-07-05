#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <sstream>
#include <iostream>
#include "ewr/generator.h"
#include "ewr/parser.h"

namespace ewr {

// A thread-safe streambuf for capturing stdout/stderr
class LogBuffer : public std::streambuf {
public:
    LogBuffer();
    ~LogBuffer();
    
    void RegisterCapture();
    void ReleaseCapture();
    std::string GetLogs() const;
    void Clear();

protected:
    int overflow(int c) override;
    std::streamsize xsputn(const char* s, std::streamsize n) override;

private:
    void Append(const char* s, size_t n);

    mutable std::mutex mutex_;
    std::string buffer_;
    std::streambuf* old_cout_;
    std::streambuf* old_cerr_;
    bool is_captured_;
};

class EwrGuiApp {
public:
    EwrGuiApp();
    ~EwrGuiApp();

    void Initialize();
    void RenderUI();
    void Shutdown();

    // Input simulation / state query for tests
    void SetSearchQuery(const std::string& query);
    std::string GetSearchQuery() const;
    std::vector<std::string> GetFilteredModelNames() const;
    void SelectModel(const std::string& modelName);
    std::string GetSelectedModelName() const;
    void TriggerReset();
    bool IsResetButtonEnabled() const;
    bool IsWarningBannerVisible() const;
    std::string GetWarningBannerText() const;
    std::string GetConsoleOutput() const;
    float GetResetProgress() const;
    std::string GetResetStatusText() const;
    std::string GetOtaStatusText() const;
    bool IsResetRunning() const;
    bool IsOtaSyncRunning() const;
    void ClearLogs();

private:
    void RunOtaSyncThread();
    void RunResetThread();
    bool CheckAdminPrivileges() const;

    // Search query and filters
    std::string search_query_;
    std::string selected_model_name_;
    
    // Loaded data
    UniversalGenerator generator_;
    std::vector<DbPrinterModel> smart_models_;
    std::vector<PrinterModel> custom_models_; // from ScanModelsFolder
    
    // GUI states
    bool has_admin_privileges_;
    bool ota_sync_started_;
    std::atomic<bool> ota_sync_running_;
    std::atomic<bool> ota_sync_success_;
    std::string ota_status_text_;

    std::atomic<bool> reset_running_;
    std::atomic<bool> reset_success_;
    std::atomic<float> reset_progress_;
    std::string reset_status_text_;
    
    std::atomic<bool> shutdown_requested_;

    std::thread ota_thread_;
    std::thread reset_thread_;

    mutable std::mutex data_mutex_;
    LogBuffer log_buffer_;
};

} // namespace ewr
