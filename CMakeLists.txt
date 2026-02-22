cmake_minimum_required(VERSION 3.20)

# Force Windows SDK detection
set(CMAKE_SYSTEM_VERSION 10.0)

project(PrismeASA LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/imgui ${CMAKE_SOURCE_DIR}/external/minhook/include)

# ============== CHEAT DLL (ASA-Internal.dll) ==============
# Requires MSVC toolchain (SEH __try/__except + MASM)

if(MSVC)
    enable_language(ASM_MASM)
endif()

add_library(ASA-Internal SHARED
        dllmain.cpp
        src/imgui/imgui.cpp
        src/imgui/imgui_draw.cpp
        src/imgui/imgui_tables.cpp
        src/imgui/imgui_widgets.cpp
        src/imgui/backends/imgui_impl_dx11.cpp
        src/imgui/backends/imgui_impl_dx12.cpp
        src/imgui/backends/imgui_impl_win32.cpp
        external/minhook/src/buffer.c
        external/minhook/src/hook.c
        external/minhook/src/trampoline.c
        external/minhook/src/hde/hde64.c
        external/minhook/src/hde/hde32.c
)

# MinHook C files need C language
set_source_files_properties(
    external/minhook/src/buffer.c
    external/minhook/src/hook.c
    external/minhook/src/trampoline.c
    external/minhook/src/hde/hde64.c
    external/minhook/src/hde/hde32.c
    PROPERTIES LANGUAGE C
)

if(MSVC)
    # Add the MASM assembly file
    target_sources(ASA-Internal PRIVATE NtUserSendInput.asm)
    set_source_files_properties(NtUserSendInput.asm PROPERTIES LANGUAGE ASM_MASM)

    target_compile_definitions(ASA-Internal PRIVATE
            _WINDOWS _USRDLL NOMINMAX WIN32_LEAN_AND_MEAN
    )
    target_compile_options(ASA-Internal PRIVATE
            /EHa          # Async exceptions (SEH + C++)
            /MP           # Multi-processor compilation
            /utf-8        # Source charset
            /W3           # Warning level 3
            /wd4244 /wd4267 /wd4302 /wd4311 /wd4312 /wd4996
    )
    # Static runtime so no vcruntime DLL needed
    set_property(TARGET ASA-Internal PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(ASA-Internal PRIVATE
        kernel32 user32 gdi32 advapi32 shell32 ole32
        d3d11 d3d12 d3dcompiler dxgi dwmapi dcomp
)

# Output DLL next to launcher
set_target_properties(ASA-Internal PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ============== EMBED TOOL (embed_dll.exe) ==============
# Encrypts ASA-Internal.dll into embedded_dll.h for the launcher

add_executable(embed_dll tools/embed_dll.cpp)
if(MSVC)
    set_property(TARGET embed_dll PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set_target_properties(embed_dll PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# After DLL is built, run embed_dll to generate embedded_dll.h
# Using a custom target + BYPRODUCTS so ninja knows the file dependency
add_custom_target(embed_step
        COMMAND $<TARGET_FILE:embed_dll>
                "$<TARGET_FILE:ASA-Internal>"
                "${CMAKE_SOURCE_DIR}/embedded_dll.h"
        COMMENT "Encrypting ASA-Internal.dll into embedded_dll.h"
        BYPRODUCTS "${CMAKE_SOURCE_DIR}/embedded_dll.h"
        DEPENDS embed_dll ASA-Internal
)

# ============== LAUNCHER APP (PrismeASA.exe) ==============
# Subscription system + DLL injector

add_executable(PrismeASA WIN32
        launcher.cpp
        app.rc
)

if(MSVC)
    target_compile_options(PrismeASA PRIVATE /utf-8 /MP)
    set_property(TARGET PrismeASA PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    # MinGW fallback
    target_link_options(PrismeASA PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
endif()

target_link_libraries(PrismeASA PRIVATE
        winhttp dwmapi advapi32 crypt32
)

set_target_properties(PrismeASA PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Build order: embed_dll.exe + ASA-Internal.dll -> embed_step -> PrismeASA.exe
add_dependencies(ASA-Internal embed_dll)
add_dependencies(PrismeASA embed_step)
