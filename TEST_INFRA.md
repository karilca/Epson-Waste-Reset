# EWR Testing Infrastructure & E2E Test Plan

This document outlines the testing architecture, mock configurations, and execution plan for the Epson Waste Reset (EWR) GUI application. The suite covers 82 test cases across four distinct tiers.

---

## 1. Testing Strategy & Architecture

To achieve fully automated, offline testing without modifying the production runtime logic, the EWR testing infrastructure leverages:

1. **Link-Time Dynamic Symbol Overriding**: 
   - Overriding standard dynamic symbols (e.g., `libcurl` calls on Linux, `URLDownloadToFileA` on Windows) at link-time. The linker resolves locally defined mock functions first, enabling seamless network interception.
2. **Preprocessor Macro Redirection**: 
   - Selectively replacing Win32 kernel calls (e.g., `CreateFile` -> `MockCreateFile`) specifically for the USB module to prevent interfering with standard test runner logging and I/O.
3. **Headless ImGui Context Simulation**: 
   - Initializing a naked Dear ImGui context without graphic backends (GLFW/OpenGL) in a virtual framebuffer (`1280x720`). Simulating GUI frames inside a programmatic loop, injecting search strings or button clicks, and verifying layout state updates.
4. **Isolated Test Library**: 
   - A dedicated static library containing mocked core routines, compiled specifically for tests to preserve production build integrity.

---

## 2. Test Case Catalog (82 Test Cases)

### Tier 1: Unit & Component Tests (25 Cases)
* **F5: Database Sync & Cache Loading**
  1. `SyncDatabaseOTA_Success`: Network mock success returns `true`.
  2. `SyncDatabaseOTA_Offline`: Network mock connection failure returns `false`.
  3. `SyncDatabaseOTA_Timeout`: Network mock timeout returns `false`.
  4. `LoadDatabase_ValidJson`: Valid database parsed, verifying size and records.
  5. `LoadDatabase_MissingAddresses`: Handles fallback parsing when "addresses" is absent.
  6. `LoadDatabase_MissingResetValues`: Validates reset array completion (defaults to 0x00).
  7. `LoadDatabase_CorruptJson`: Verifies graceful failure on bad JSON characters.
  8. `LoadDatabase_FileNotFound`: Graceful fallback if database file does not exist.
  9. `UniversalGenerator_IsEmpty`: Confirms empty state when no database is loaded.
* **F1/F4: Payload Sequence Generation**
  10. `GenerateWritePacket_Format`: Verifies IEEE 1284.4 D4 write packet structure.
  11. `GenerateSequence_SafeInit`: Confirms EJL and D4 channel initialization commands are present.
  12. `GenerateSequence_AddressValuesMatch`: Checks that packet count corresponds to target addresses.
  13. `GenerateSequence_KeysEncoded`: Validates that `rkey` and `wkey` are written to packets in correct formats.
* **F2: Wireshark Parser**
  14. `ScanModelsFolder_Empty`: Returns empty list when directory has no matches.
  15. `ScanModelsFolder_FiltersFiles`: Excludes files without `.txt` or `.c` extensions.
  16. `ScanModelsFolder_CreatesFolder`: Automatically creates directory if missing.
  17. `ParseWiresharkDump_ValidTxt`: Successfully extracts payload arrays from text formats.
  18. `ParseWiresharkDump_ValidC`: Successfully parses C files containing payload dumps.
  19. `ParseWiresharkDump_StripHeader`: Automatically strips 27-byte USB headers from Wireshark streams.
  20. `ParseWiresharkDump_EmptyOrInvalid`: Handles invalid syntax safely, returning empty list.
  21. `ParseWiresharkDump_FileNotFound`: Handles missing dump files without crashes.
* **F6: Privileges**
  22. `CheckPrivileges_Root_Linux`: Returns `true` when euid is mock-set to 0.
  23. `CheckPrivileges_User_Linux`: Returns `false` when euid is mock-set to 1000.
  24. `CheckPrivileges_Admin_Windows`: Returns `true` when admin privilege checks succeed.
  25. `CheckPrivileges_User_Windows`: Returns `false` when admin checks fail.

### Tier 2: Component Integration Tests (25 Cases)
* **F2: Real-time Search UI & Filtering**
  26. `Search_EmptyQuery`: Confirms all models (smart + custom) are listed when search is blank.
  27. `Search_SingleMatch`: Queries "L3150" and verifies exactly 1 model returns.
  28. `Search_MultipleMatches`: Queries "L" and checks that all L-series models list.
  29. `Search_CaseInsensitive`: Matches "l3150" to "L3150" successfully.
  30. `Search_NoMatch`: Verifies empty list and "No results found" layout.
  31. `Search_TrimSpaces`: Trims leading/trailing spaces before querying.
  32. `Search_SpecialCharacters`: Handles brackets and punctuation safely.
* **F3: Details Panel & Reset Button State**
  33. `SelectModel_DisplaysSmartDetails`: Renders name, addresses, and keys in detail window.
  34. `SelectModel_DisplaysReplayDetails`: Renders details for custom dump replays.
  35. `SelectModel_EnablesResetButton`: Ensures reset button is disabled by default and enabled on selection.
  36. `DeselectModel_DisablesResetButton`: Disables button if user clears the selected target.
* **F7: Log Interceptor**
  37. `LogCapture_StdoutRedirect`: Intercepts `std::cout` and verifies it lands in console buffer.
  38. `LogCapture_StderrRedirect`: Intercepts `std::cerr` and verifies it lands in console buffer.
  39. `LogCapture_ThreadSafety`: Verifies concurrent stdout prints do not garble logs.
  40. `LogCapture_OverflowControl`: Verifies log buffer discards old entries above limit (e.g. 1000 lines).
* **F4: Asynchronous Coordination & Task States**
  41. `AsyncTask_OTA_StatusRunning`: Verifies running status updates correctly during sync.
  42. `AsyncTask_OTA_StatusSuccess`: Status shows success upon successful download.
  43. `AsyncTask_Reset_StatusRunning`: GUI remains responsive and status is marked running during USB transfer.
  44. `AsyncTask_Reset_StatusSuccess`: Status updates to success when reset sequence finishes cleanly.
  45. `AsyncTask_Reset_StatusError`: Status reports errors when reset execution fails.
  46. `AsyncTask_LogForwarding`: Verifies async reset worker forwards logs to the GUI in real-time.
  47. `AsyncTask_ProgressUpdates`: Checks that progress ranges linearly from 0.0 to 1.0.
  48. `AsyncTask_NoGuiFreeze`: Frame-time stays low (<16ms) during background execution.
  49. `AsyncTask_CancelReset`: Safely interrupts reset thread via cancel flag.
  50. `AsyncTask_MultipleRestarts`: Sequentially re-runs reset tasks without dangling threads.

### Tier 3: E2E Scenario Tests (20 Cases)
* **F1 + F5 + F6: Startup, Sync, and Privilege Scenarios**
  51. `E2E_Startup_SyncSuccess_Root`: Starts app under root with success sync. Database is loaded, privileges banner hidden, status displays "Synced".
  52. `E2E_Startup_SyncSuccess_NoRoot`: Starts app under normal user. Displays warning banner.
  53. `E2E_Startup_SyncOffline_Root`: Network is offline. App falls back to local database file.
  54. `E2E_Startup_SyncTimeout_Root`: Sync times out. Logs message and uses local database file.
  55. `E2E_Startup_SyncCorrupt_Root`: Remote database returns invalid syntax. Falls back to local database cleanly.
* **F1 + F2 + F3 + F4 + F7: Reset Success Scenarios**
  56. `E2E_SmartReset_Success`: User searches, selects "L3150", clicks Reset, mock USB claims interface, writes payload, registers ACKs, and prints success messages to console.
  57. `E2E_ReplayReset_Success`: Same as above using a custom parsed Wireshark replay dump.
* **F1 + F4 + F7: Reset Failure Scenarios**
  58. `E2E_Reset_NoDevice`: User clicks reset without printer plugged in. Logs "[ERROR] Could not find an Epson printer".
  59. `E2E_Reset_DeviceBusy_Windows`: CreateFile fails with sharing violation error. Warnings suggest exiting Epson Status Monitor.
  60. `E2E_Reset_AccessDenied_Windows`: CreateFile fails with access denied. Prompts user to run as Admin.
  61. `E2E_Reset_ClaimInterfaceFailed_Linux`: USB interface cannot be claimed. Fails and releases descriptors.
  62. `E2E_Reset_WriteFailed`: Endpoint write error occurs. Cleans up handles and aborts.
  63. `E2E_Reset_NoAckReceived`: Sequence finishes but printer reports no ACKs. Logs failure.
  64. `E2E_Reset_DisconnectedMidway`: USB disconnected during packet write. Aborts transfer cleanly.
* **F1 + F2 + F3 + F4 + F7: Complex Action Scenarios**
  65. `E2E_SearchFilter_ChangeSelection`: Switches selection from L3150 to L3210 mid-process. Executes correct sequence.
  66. `E2E_RunReset_ThenSyncDatabase`: Interleaves reset actions and database sync actions.
  67. `E2E_DatabaseReload_UpdatesList`: Updates filtered UI options list when database loads asynchronously.
  68. `E2E_MultipleResets_Consecutive`: Simulates double reset flow with a simulated unplug cycle between runs.
  69. `E2E_Reset_ReplaysBothTxtAndC`: Success execution verifying custom model lists loaded with mixed `.txt` and `.c` files.
  70. `E2E_AppQuit_DuringActiveTask`: Closes application during active reset thread. Verifies orderly thread joining and zero memory leaks.

### Tier 4: Edge Case, Stress & Robustness Tests (12 Cases)
* **Data & Input Stress**
  71. `Database_ExtremeSize`: Loads 10,000 mock printer models. Ensures search stays fast and UI responsive.
  72. `Database_InvalidModelFields`: Verifies robustness when addresses are out-of-bounds or keys are empty.
  73. `Search_RapidInputStress`: Simulates typing at 100 Hz. Verifies zero crashes or race conditions.
  74. `Replay_MalformedWiresharkFiles`: Parses corrupted text streams (e.g. missing brackets). Does not crash.
  75. `Replay_HugeWiresharkFile`: Parses huge dumps (e.g. 50MB). Heap allocation remains stable.
* **Hardware Failure Stress**
  76. `USB_PowerCycleSimulator`: Connects/disconnects USB interface at 10 Hz. Ensures scanning handles it cleanly.
  77. `USB_ExtremelySlowPrinter`: Delays ACKs to 1.9 seconds per packet. Sequence finishes without timeout.
  78. `USB_PartialWrite`: Simulates writes sending partial byte buffers. Verifies retry logic.
  79. `LogConsole_StressBuffer`: Appends 100,000 log statements. Memory usage remains low and capped.
  80. `Thread_StackOverflowRisk`: Evaluates deep recursion inside payload compilation.
  81. `Resource_DescriptorLeak_Check`: Tracks descriptors and ensures zero leaks on failure paths.
  82. `Memory_Leak_ValgrindRun`: Run E2E test suite through Valgrind/ASan to verify leak-free ImGui/background thread operations.
