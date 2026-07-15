cmake_minimum_required(VERSION 3.24)

project(TankSandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(External/RtPbrSurvey)

add_executable(TankSandbox
    src/main.cpp
    src/stdafx.h
    src/TankSandboxApp.h
    src/TankSandboxApp.cpp
)

set_target_properties(TankSandbox PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(TankSandbox
    PRIVATE
        RtPbrSurvey
)

target_compile_definitions(TankSandbox
    PRIVATE
        WIN32
        _WINDOWS
        _UNICODE
        UNICODE
)

set(D3D12_AGILITY_REDIST_DIR
    "$ENV{USERPROFILE}/.nuget/packages/microsoft.direct3d.d3d12/1.618.3/build/native/bin/x64"
    CACHE PATH
    "Directory containing D3D12Core.dll and D3D12SDKLayers.dll for the D3D12 Agility SDK."
)

if(EXISTS "${D3D12_AGILITY_REDIST_DIR}/D3D12Core.dll")
    add_custom_command(TARGET TankSandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TankSandbox>/D3D12"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${D3D12_AGILITY_REDIST_DIR}/D3D12Core.dll"
            "$<TARGET_FILE_DIR:TankSandbox>/D3D12/D3D12Core.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${D3D12_AGILITY_REDIST_DIR}/D3D12SDKLayers.dll"
            "$<TARGET_FILE_DIR:TankSandbox>/D3D12/D3D12SDKLayers.dll"
        COMMENT "Copying D3D12 Agility SDK runtime next to TankSandbox."
    )
else()
    message(WARNING "D3D12 Agility SDK runtime not found at ${D3D12_AGILITY_REDIST_DIR}; hardware D3D12 device creation may fail with D3D12_ERROR_INVALID_REDIST.")
endif()
