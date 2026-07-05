# Original User Request

## Initial Request — 2026-07-05T10:55:26+02:00

Convert the EWR (Epson Waste Reset) C++ application from a Command Line Interface (CLI) tool to a Graphical User Interface (GUI) tool using **Dear ImGui (with GLFW and OpenGL3 backends)**, ensuring it maintains full support for both Windows and Linux, and preserves all existing functionality.

Working directory: /home/kolak/Radna površina/Epson Ink Pad Resetter/Code
Integrity mode: development

## Requirements

### R1. GUI Integration via Dear ImGui
- Integrate Dear ImGui (v1.89+ or latest stable) using the GLFW and OpenGL3 backends.
- Use CMake `FetchContent` to download and build GLFW and Dear ImGui directly as part of the build process to avoid requiring system-installed package dependencies.
- Update `CMakeLists.txt` to support building the GUI executable (`ewr`) on both Linux and Windows.

### R2. User Interface Layout & Styling
- Implement a clean, modern single-window desktop application with a cohesive dark-themed aesthetic.
- **Header Section**: Display the application title, version, and indicators for Database sync status and user privilege levels (e.g., warning if not running with root/sudo on Linux).
- **Search & Selection Section**: A text input field allowing the user to search/filter printer models in real time. Matching models (both Smart Protocol from `database.json` and Replay models from the `models/` folder) must be listed in a filterable table or scrollable list. Selecting a model should display details about it.
- **Action Section**: A prominent "Reset Waste Ink Pad" button. This button should be enabled only when a printer model is selected.
- **Log / Output Console**: A dedicated scrollable text area at the bottom of the window displaying log outputs, operations, and success/error status messages in real time.

### R3. Non-Blocking Operation Execution
- Integrate core reset routines (`SyncDatabaseOTA`, USB scanning, and payload execution) asynchronously or in a non-blocking manner so that the GUI window remains responsive (does not freeze, stop rendering, or become unresponsive) during long network or USB operations.

### R4. Core Logic Integration
- Utilize the existing codebase routines:
  - `ewr::UniversalGenerator` for OTA database syncing and loading.
  - `ewr::ScanModelsFolder` for scanning custom Wireshark replay models.
  - `ewr::AutoConnectEpsonPrinter` for USB scanning and connection.
  - `ewr::ExecutePayloadSequence` for executing the payload sequence.

### R5. Privilege & Permission Handling
- Show a warning banner in the GUI if the tool is not launched with administrative/root privileges (on Linux, check if effective UID is 0; on Windows, check if running as Administrator).

## Verification Plan

### Automated/Compilation Verification
- Build the project using:
  ```bash
  cmake -B build
  cmake --build build --config Release
  ```
- Verify that the executable is generated at the root or standard target directory.
- Verify that the executable links against GLFW and OpenGL libraries.

### Manual / Behavioral Verification
- Launch the GUI.
- Verify that the UI displays database sync status (updates successfully if connected to the internet).
- Search for a printer model (e.g., "L3150"). Select it from the list.
- Click the "Reset Waste Ink Pad" button without a printer connected. Verify that:
  - The UI does not freeze.
  - The log console prints `[ERROR] Could not find an Epson printer` or equivalent.
  - The application remains responsive and allows trying again or exiting.

## Acceptance Criteria

### Compilation & Structure
- [ ] Code builds on Linux without compile errors.
- [ ] Dependencies (GLFW, ImGui) are managed automatically via CMake `FetchContent` (no manual pre-installation required).

### Graphical Interface & Functionality
- [ ] The app starts in a graphical window, not in CLI mode.
- [ ] Has a text box to search for printer models.
- [ ] Displays a list of available/matching models.
- [ ] Selecting a printer model and clicking "Reset Waste Ink Pad" triggers the reset process.
- [ ] A scrollable log area dynamically updates with logs.
- [ ] The UI does not freeze during USB device scanning or database sync.

## Follow-up — 2026-07-05T09:09:53Z

Please resume your work on the EWR GUI migration. Provide a status update of your current progress.
