cmake_minimum_required(VERSION 3.15)
project(EWR VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include FetchContent
include(FetchContent)

# Fetch GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
    GIT_SHALLOW    ON
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.8
    GIT_SHALLOW    ON
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# Create Dear ImGui target
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Find OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# Core library sources
set(LIB_SOURCES src/parser.cpp src/generator.cpp)

if(WIN32)
    list(APPEND LIB_SOURCES src/usb_windows.cpp)
else()
    list(APPEND LIB_SOURCES src/usb_linux.cpp)
endif()

add_library(ewr_core STATIC ${LIB_SOURCES})

target_include_directories(ewr_core PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor>
)

if(WIN32)
    target_link_libraries(ewr_core PRIVATE setupapi urlmon)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    pkg_check_modules(LIBCURL REQUIRED libcurl)
    
    target_include_directories(ewr_core PRIVATE ${LIBUSB_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
    target_link_libraries(ewr_core PRIVATE ${LIBUSB_LIBRARIES} ${LIBCURL_LIBRARIES})
endif()

# GUI executable target
add_executable(ewr src/gui/main.cpp src/gui/gui_app.cpp)
target_link_libraries(ewr PRIVATE ewr_core imgui)
set_target_properties(ewr PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

if(WIN32)
    set_target_properties(ewr PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(ewr PRIVATE "/ENTRY:mainCRTStartup")
    endif()
endif()

# Test target configuration
option(BUILD_TESTING "Build the test suite" ON)

if(BUILD_TESTING)
    set(MOCK_SOURCES
        tests/mocks/imgui/imgui.cpp
        tests/mocks/mock_network.cpp
        tests/mocks/mock_usb.cpp
    )
    if(WIN32)
        list(APPEND MOCK_SOURCES tests/mocks/mock_usb_windows.cpp)
    else()
        list(APPEND MOCK_SOURCES tests/mocks/mock_usb_linux.cpp)
    endif()

    add_library(ewr_core_mocked STATIC ${LIB_SOURCES})
    target_include_directories(ewr_core_mocked PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks>
    )

    if(WIN32)
        target_compile_definitions(ewr_core_mocked PRIVATE
            CreateFile=MockCreateFile
            WriteFile=MockWriteFile
            ReadFile=MockReadFile
            CloseHandle=MockCloseHandle
            GetOverlappedResult=MockGetOverlappedResult
            WaitForSingleObject=MockWaitForSingleObject
            CancelIo=MockCancelIo
        )
        target_link_libraries(ewr_core_mocked PRIVATE setupapi urlmon)
    else()
        target_include_directories(ewr_core_mocked PRIVATE ${LIBUSB_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
        target_link_libraries(ewr_core_mocked PRIVATE ${LIBUSB_LIBRARIES} ${LIBCURL_LIBRARIES})
    endif()

    add_executable(ewr_tests
        tests/test_main.cpp
        tests/test_generator.cpp
        tests/test_parser.cpp
        tests/test_usb.cpp
        tests/test_gui.cpp
        src/gui/gui_app.cpp
        ${MOCK_SOURCES}
    )

    target_include_directories(ewr_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    )

    target_link_libraries(ewr_tests PRIVATE ewr_core_mocked)
    
    if(NOT WIN32)
        target_include_directories(ewr_tests PRIVATE ${LIBUSB_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS})
        target_link_libraries(ewr_tests PRIVATE ${LIBUSB_LIBRARIES} ${LIBCURL_LIBRARIES} pthread)
    endif()

    set_target_properties(ewr_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()