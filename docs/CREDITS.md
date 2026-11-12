# Сторонние компоненты

orbiter-lan-coop распространяется под лицензией GPLv3. Ниже перечислено всё, что используется
из чужого и на каких условиях.

## Orbiter SDK

- **Что это:** заголовки и библиотеки для дополнений Orbiter (`Orbitersdk/include`,
  `orbiter.lib`, `Orbitersdk.lib`).
- **Откуда:** https://github.com/orbitersim/orbiter, базовый коммит `dabade26`.
- **Условия:** MIT License, © Martin Schweiger. `Orbitersdk.lib` статическая, её код входит
  в DLL плагина, поэтому в релиз кладётся текст MIT-лицензии Orbiter с уведомлением об
  авторских правах. В `Orbitersdk.lib` собраны также Dear ImGui (MIT, © Omar Cornut) и
  ImPlot (MIT, © Evan Pezent) — их уведомления кладутся в релиз так же.
- **Входит в репозиторий:** нет; сборка в GitHub Actions собирает его из базового коммита.

## Orbiter

- **Что это:** сам космосимулятор, в котором работает плагин.
- **Откуда:** https://github.com/orbitersim/orbiter.
- **Условия:** MIT License (графический клиент D3D9Client — GPLv3/LGPLv3). OLC с ним не
  линкуется, только запускается внутри.
- **Входит в репозиторий:** нет.
