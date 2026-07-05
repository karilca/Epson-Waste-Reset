# Project: EWR GUI Conversion

## Architecture
The application is a GUI wrapper around the core `ewr_core` library (which handles USB communication, packet parsing, and OTA database sync).
The GUI is built using Dear ImGui, GLFW, and OpenGL3 backend.

### Data Flow
1. On Startup:
   - Check user privileges (root on Linux, Admin on Windows) -> Set flag and show warning banner if needed.
   - Start asynchronous OTA database sync via `ewr::UniversalGenerator::SyncDatabaseOTA` in a background thread.
   - Load local database from `database.json` and scan custom models from `models/` directory.
2. Real-time Search:
   - User inputs search query in GUI.
   - GUI filters the list of available models (smart + replay) and displays matching items.
3. Selection & Reset:
   - User selects a model. Detail panel displays selected model information. "Reset Waste Ink Pad" button is enabled.
   - User clicks the reset button.
   - Start asynchronous reset routine in a background thread (USB auto-connect, payload generation, sequence execution).
   - Core logs are redirected/captured and displayed in the GUI log console in real-time.
   - The GUI remains responsive throughout, preventing freezing.

### Shared Interfaces / Contracts
- GUI to Core:
  - `ewr::UniversalGenerator`
  - `ewr::ScanModelsFolder`
  - `ewr::AutoConnectEpsonPrinter`
  - `ewr::ExecutePayloadSequence`
- Asynchronous Coordination:
  - Background threads report status (idle, syncing, scanning, resetting, done, error) and logs to the GUI main thread thread-safely.
  - The UI uses atomic/mutex-guarded structures to query thread progress without blocking.

## Milestones

| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | E2E Test Suite and Infrastructure | Design E2E test runner, mock environments (USB, OTA), and write test cases for Tiers 1-4 | None | IN_PROGRESS (Conv ID: 3b123f36-13cf-4874-8cb2-ae79bc843b89) |
| 2 | GLFW & Dear ImGui Integration | Update CMakeLists.txt to fetch GLFW/ImGui, set up window loop, and compile executable | None | IN_PROGRESS (Conv ID: 6267e549-a751-4f8f-b955-7463fdf0aff3) |
| 3 | GUI Layout & Styling | Implement UI header, search, printer model list table, action panel, and scrollable log console | M2 | PLANNED |
| 4 | Asynchronous Execution | Move OTA database sync and USB reset routine to background threads, maintaining UI responsiveness | M3 | PLANNED |
| 5 | Privilege Handling & Logging | Integrate privilege check banners and redirect core print/std::cout/std::cerr logs to GUI console | M3 | PLANNED |
| 6 | E2E Test Pass (Tiers 1-4) | Run E2E test suite against the built GUI and fix bugs until 100% pass rate is achieved | M1, M4, M5 | PLANNED |
| 7 | Adversarial Hardening (Tier 5) | Perform white-box analysis, generate adversarial tests, and ensure structural/logical coverage | M6 | PLANNED |

## Interface Contracts
### GUI Asynchronous Task Interface
- `struct TaskStatus`
  - `std::atomic<bool> is_running`
  - `std::atomic<bool> success`
  - `std::vector<std::string> log_messages` (protected by `std::mutex`)
  - `std::atomic<float> progress`

### Log Interceptor
- Intercept stdout/stderr or redirect logging calls to GUI's internal console buffer thread-safely.

## Code Layout
- `cli/` - Original CLI main entry point (deprecated / replaced).
- `src/` - Core library sources (`generator.cpp`, `parser.cpp`, `usb_*.cpp`).
- `src/gui/` - GUI application sources (main application loop, window layout).
- `include/ewr/` - Core library headers.
- `include/gui/` - GUI application headers.
- `tests/` - E2E tests and mocks.
- `CMakeLists.txt` - Build configuration (FetchContent GLFW & ImGui, compile GUI target).
