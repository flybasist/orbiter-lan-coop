# OlcRigTest.dll: creates and deletes vessels on a schedule for checks on test rigs
# (docs/STANDS.md). Built only together with the plugin (Windows, OLC_ORBITER_SDK_DIR).
add_library(OlcRigTest SHARED OlcRigTest.cpp)
target_include_directories(OlcRigTest SYSTEM PRIVATE ${OLC_ORBITER_SDK_DIR}/include)
target_link_libraries(OlcRigTest PRIVATE
    ${OLC_ORBITER_SDK_DIR}/lib/Orbiter.lib
    ${OLC_ORBITER_SDK_DIR}/lib/Orbitersdk.lib
)
