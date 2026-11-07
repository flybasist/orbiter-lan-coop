# Плагин OrbiterLanCoop.dll для Orbiter (только Windows x64).
# Нужен собранный Orbiter: каталог OLC_ORBITER_SDK_DIR с подкаталогами
#   include - заголовки SDK (Orbitersdk/include из репозитория Orbiter);
#   lib     - Orbiter.lib (библиотека импорта orbiter.exe) и Orbitersdk.lib.
# Как его получить - docs/QUICKSTART.md и .github/workflows/build.yml.

add_library(OrbiterLanCoop SHARED
    OrbiterLanCoop.cpp
)

# Заголовки SDK - системные: предупреждения в них не наши.
target_include_directories(OrbiterLanCoop SYSTEM PRIVATE ${OLC_ORBITER_SDK_DIR}/include)

target_link_libraries(OrbiterLanCoop PRIVATE
    olc-core
    ${OLC_ORBITER_SDK_DIR}/lib/Orbiter.lib
    ${OLC_ORBITER_SDK_DIR}/lib/Orbitersdk.lib
)
