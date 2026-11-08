cmake_minimum_required(VERSION 3.22)

project(dehli_musikk_plugin_core VERSION 0.1.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# JUCE, for the GUI module's test only
#
# A plugin brings its own JUCE, and the library adds none of its own when it is
# part of one. Built on its own, the library needs JUCE only to test the GUI
# module, which is off unless asked for. JUCE 9.0.5 is fetched as the archive
# of a commit and checked against its hash. Point -DJUCE_PATH at a checkout to
# skip the download.
# ------------------------------------------------------------------------------
option(DM_PLUGIN_CORE_GUI_TESTS "Build the GUI module's test, which needs JUCE" OFF)

if(PROJECT_IS_TOP_LEVEL AND DM_PLUGIN_CORE_GUI_TESTS)
    # JUCE compiles some C of its own.
    enable_language(C)

    set(JUCE_PATH "" CACHE PATH "Path to an existing JUCE checkout")

    if(JUCE_PATH)
        add_subdirectory("${JUCE_PATH}" JUCE)
    else()
        if(POLICY CMP0135)
            cmake_policy(SET CMP0135 NEW)
        endif()

        include(FetchContent)
        FetchContent_Declare(JUCE
            URL      https://github.com/juce-framework/JUCE/archive/d60b7a78030661574a615b586d9325ba7a7fa7dd.tar.gz
            URL_HASH SHA256=8237b2976871982eb9107fd67b576d95bf02355d99540a0127c4e7c001ecd9f5)
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

# ------------------------------------------------------------------------------
# The library
#
# Plain C++17 with no dependencies, so it builds anywhere and every plugin that
# uses it compiles the same code. Position independent, since a plugin is a
# shared library and links this into itself.
#
# Standard C++17 rather than the GNU dialect. With the dialect, gcc may fuse a
# multiply and an add into one instruction where the processor has one, which
# rounds differently, so the same input could give different samples on
# different machines.
#
# Symbols hidden, as JUCE hides a plugin's own. Two plugins can be loaded into
# one host with different versions of this library, and on Linux and macOS a
# visible symbol in one can be bound to the same symbol in the other.
# ------------------------------------------------------------------------------
add_library(dm_plugin_core STATIC
    src/dsp/Cassette.cpp
    src/dsp/Console.cpp
    src/dsp/OutputStage.cpp)

add_library(dm::plugin_core ALIAS dm_plugin_core)

target_include_directories(dm_plugin_core PUBLIC include)
target_compile_features(dm_plugin_core PUBLIC cxx_std_17)
set_target_properties(dm_plugin_core PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

# ------------------------------------------------------------------------------
# Warnings
#
# The library's own code is held to the same warnings wherever it is built, so
# a plugin that uses it never meets a warning the library's CI did not. They
# are errors when this is the top-level project, and only warnings when a
# plugin builds it, so a new compiler's new warning cannot stop a plugin
# building.
# ------------------------------------------------------------------------------
option(DM_PLUGIN_CORE_WARNINGS_AS_ERRORS
       "Treat warnings in the library and its tests as errors" ${PROJECT_IS_TOP_LEVEL})

function(dm_plugin_core_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wfloat-equal -Wsign-conversion
            -Wsign-compare -Wunused-parameter -Wuninitialized -Wcast-align
            -Wswitch-enum -Wredundant-decls -Woverloaded-virtual -Wreorder
            -Wzero-as-null-pointer-constant -Wmissing-field-initializers)
    endif()

    # A function defined with no declaration before it, which almost always
    # belongs in an anonymous namespace.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE -Wmissing-declarations)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE -Wmissing-prototypes
                                                 -Wshadow-uncaptured-local)
    endif()

    if(DM_PLUGIN_CORE_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

dm_plugin_core_set_warnings(dm_plugin_core)

# ------------------------------------------------------------------------------
# The GUI module
#
# The look and controls the plugins share, as a JUCE module, so each plugin
# compiles it with its own JUCE settings. Registered only when the project
# using the library has added JUCE first. Link dm::dm_gui to use it.
# ------------------------------------------------------------------------------
if(COMMAND juce_add_module)
    juce_add_module(modules/dm_gui ALIAS_NAMESPACE dm)
endif()

# ------------------------------------------------------------------------------
# Tests
#
# Only when this is the top-level project, so a plugin that uses the library
# does not build its tests.
# ------------------------------------------------------------------------------
option(DM_PLUGIN_CORE_SANITIZE "Build with the address and undefined behaviour sanitizers" OFF)

if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_executable(dm_output_stage_test tests/output_stage_test.cpp)
    target_link_libraries(dm_output_stage_test PRIVATE dm::plugin_core)
    set_target_properties(dm_output_stage_test PROPERTIES CXX_EXTENSIONS OFF)
    dm_plugin_core_set_warnings(dm_output_stage_test)

    if(DM_PLUGIN_CORE_SANITIZE AND NOT MSVC)
        foreach(target dm_plugin_core dm_output_stage_test)
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer -g)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endforeach()
    endif()

    add_test(NAME output_stage COMMAND dm_output_stage_test)
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING AND DM_PLUGIN_CORE_GUI_TESTS)
    juce_add_console_app(dm_gui_test PRODUCT_NAME "dm_gui_test")
    target_sources(dm_gui_test PRIVATE tests/gui_test.cpp)

    target_compile_definitions(dm_gui_test PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1)

    target_link_libraries(dm_gui_test
        PRIVATE
            dm::dm_gui
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    if(DM_PLUGIN_CORE_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(dm_gui_test PRIVATE /WX)
        else()
            target_compile_options(dm_gui_test PRIVATE -Werror)
        endif()
    endif()

    # JUCE 9.0.5's alert window code takes a reference through a temporary
    # that GCC 13 and newer warn about, though it is sound. Silenced for that
    # one JUCE file on GCC, as Overtonium does, and our code keeps the check.
    if(DM_PLUGIN_CORE_WARNINGS_AS_ERRORS AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
       AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
        set_source_files_properties(
            "${JUCE_MODULES_DIR}/juce_gui_basics/juce_gui_basics_2.cpp"
            PROPERTIES COMPILE_OPTIONS "-Wno-dangling-reference")
    endif()

    add_test(NAME gui COMMAND dm_gui_test)
endif()
