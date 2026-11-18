# The OrbiterLanCoop.dll plugin for Orbiter (Windows x64 only).
# Needs a built Orbiter: the folder OLC_ORBITER_SDK_DIR with subfolders
#   include - SDK headers (Orbitersdk/include from the Orbiter repository);
#   lib     - Orbiter.lib (import library of orbiter.exe) and Orbitersdk.lib.
# How to get it - docs/QUICKSTART.md and .github/workflows/build.yml.

add_library(OrbiterLanCoop SHARED
    OrbiterLanCoop.cpp
    config.cpp
    host_side.cpp
    client_side.cpp
    launchpad_item.cpp
    scenario_launch.cpp
    trace.cpp
    vessel_control.cpp
    OrbiterLanCoop.rc
)

# SDK headers are system headers: warnings in them are not ours.
target_include_directories(OrbiterLanCoop SYSTEM PRIVATE ${OLC_ORBITER_SDK_DIR}/include)

target_link_libraries(OrbiterLanCoop PRIVATE
    olc-core
    ${OLC_ORBITER_SDK_DIR}/lib/Orbiter.lib
    ${OLC_ORBITER_SDK_DIR}/lib/Orbitersdk.lib
)
