[English](../CREDITS.md) · **Русский**

# Сторонние компоненты

orbiter-lan-coop распространяется под лицензией GPLv3. Ниже перечислено всё, что используется
из чужого и на каких условиях.

## Orbiter SDK

- **Что это:** заголовки и библиотеки для дополнений Orbiter (`Orbitersdk/include`,
  `orbiter.lib`, `Orbitersdk.lib`).
- **Откуда:** https://github.com/orbitersim/orbiter, базовый коммит `a7a3396e`.
- **Условия:** MIT License, © Martin Schweiger. `Orbitersdk.lib` статическая, её код входит
  в DLL плагина. В `Orbitersdk.lib` собраны также Dear ImGui (MIT, © Omar Cornut) и ImPlot
  (MIT, © Evan Pezent). Тексты всех трёх лицензий лежат в `tools/package/THIRD-PARTY-NOTICES.txt`
  и кладутся рядом с DLL в архив плагина (`OLC-THIRD-PARTY-NOTICES.txt`).
- **Входит в репозиторий:** нет; сборка в GitHub Actions собирает его из базового коммита.

## Orbiter

- **Что это:** сам космосимулятор, в котором работает плагин.
- **Откуда:** https://github.com/orbitersim/orbiter.
- **Условия:** MIT License (графический клиент D3D9Client — GPLv3/LGPLv3). OLC с ним не
  линкуется, только запускается внутри.
- **Входит в репозиторий:** нет. **В пакете испытаний:** да — `olc-1-orbiter.zip` (с файлом
  `LICENSE` Orbiter).

## Ядра SPICE

- **Что это:** данные о движении тел Солнечной системы (`Kernels\orbiter.bsp` и др.), без них
  Orbiter не запускается.
- **Откуда:** ссылки из `COMPILE.md` Orbiter (`cdn.openorbiter.space/spice/`), данные NASA/JPL.
- **Условия:** на сайте Orbiter не оговорены; данные NASA/JPL обычно свободны
  ([TEXTURES.md](TEXTURES.md), раздел 3). В пакет положены по решению владельца (решение 50).
- **Входит в репозиторий:** нет. **В пакете испытаний:** да — `olc-2-kernels.zip`.
