[English](../QUICKSTART.md) · **Русский**

# Быстрый старт

Подготовка окружения разработчика (Windows и сборка Orbiter из исходников на базовом
коммите), сборка плагина и запуск хоста и клиента. Команды взяты из `COMPILE.md` и
`CMakePresets.json` репозитория Orbiter. На стенде Windows 11 ARM (этап 0,
[PLAN.md](PLAN.md)) проверены полная сборка с D3D9Client, ядра SPICE, загрузка плагина,
запуск без графики и две копии с картинкой.

## 1. Требования

- Windows 10/11 x64 (или Windows 11 ARM: Orbiter x64 работает через эмуляцию).
- Около 10 ГБ на диске для исходников, сборки и текстур.
- Доступ в интернет на время установки зависимостей.

## 2. Подготовка окружения

1. **Visual Studio** 2022 или 2026 (подойдёт бесплатная Community) с набором
   «Desktop development with C++» и компонентом «C++ MFC» во вкладке Individual Components.
2. **DirectX SDK (June 2010):** https://www.microsoft.com/en-us/download/details.aspx?id=6812.
   Если уже установлен Visual C++ 2010 Redistributable, установщик падает с ошибкой S1023 и
   SDK **не ставит** (в каталоге остаётся одна папка `system`). Проверяйте по файлу
   `Include\d3dx9.h`. Обход от Microsoft: удалить Visual C++ 2010 Redistributable, поставить
   SDK, вернуть Redistributable. Так делает скрипт `tools/stands/setup-windows.ps1`.
3. **Git для Windows:** https://git-scm.com/install/windows.
4. **Python 3.13+** — только если собирать скриптом `compileOrbiter.py` (см. ниже).

## 3. Сборка Orbiter

Вариант А — скрипт из репозитория Orbiter (сам ставит зависимости, собирает и создаёт
каталог `install` с готовым к запуску Orbiter). Скачайте репозиторий, перейдите на базовый
коммит и запустите `compileOrbiter.py`, в меню выберите пункт 3 (сборка локального репозитория).

Вариант Б — вручную, из «x64 Native Tools Command Prompt for VS»
(на Windows ARM — среда кросс-сборки под x64):

```bat
git clone https://github.com/orbitersim/orbiter.git
cd orbiter
git checkout a7a3396e
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
```

Сборка появится в `out\build\windows-x64-release`, готовая к запуску установка — после
`cmake --install out\build\windows-x64-release` в `out\install\windows-x64-release`.

Что выяснилось на стенде:

- **Windows 11 ARM.** Сборка x64 идёт из обычной среды `vcvarsall.bat x64`: компилятор x64
  работает через эмуляцию. Настройка CMake (с загрузкой сторонних библиотек) — около 4 минут,
  сборка — около 6 минут на 4 ядрах.
- **Без DirectX SDK собрать полный Orbiter нельзя.** С `-DORBITER_BUILD_D3D9CLIENT=OFF` падает
  LuaInterpreter: ему нужен `gcCoreAPI.h`, который появляется только при сборке D3D9Client.
  Цели `Orbiter` и `Orbitersdk` (так собирает CI) при этом собираются. Каталог сборки не
  заменяет установку: часть модулей (атмосферы планет) Orbiter ищет там, куда их кладёт
  только `cmake --install`.

## 4. Первый запуск

1. Текстуры планет в репозиторий не входят. Укажите каталог с ними переменной
   `ORBITER_PLANET_TEXTURE_INSTALL_DIR` до сборки или параметром `PlanetTexDir` в `Orbiter.cfg`
   после (например, из установленного Orbiter 2016 или 2024).
2. **Ядра SPICE обязательны.** Без `Kernels\orbiter.bsp` (1,6 ГБ) Orbiter закрывает сессию
   сразу после запуска (`SUN - Couldn't load kernel`). Архивы — по ссылкам из `COMPILE.md`
   Orbiter, раздел SPICE: `Kernels.7z` (1,2 ГБ) распаковывается в `Kernels`, `Textures.7z` — в
   `Textures`. Для второй копии Orbiter на той же машине `orbiter.bsp` можно не копировать, а
   сделать жёсткую ссылку (`mklink /H`).
3. Запустите Orbiter, во вкладке Modules включите графический клиент D3D9Client, откройте
   сценарий с DeltaGlider и убедитесь, что всё летает. Без лаунчера: `Orbiter.exe -s
   "Checklists\Quickstart" -p D3D9Client`.
4. **Текстур Земли нет ни в сборке, ни в `Textures.7z`, ни в релизе Orbiter на GitHub** — они
   ставятся отдельным пакетом. Без них D3D9Client при старте показывает окно «Surface textures
   are missing for Earth» и ждёт нажатия OK. Для проверок, где поверхность не важна, хватит
   пустого каталога `Textures\Earth`: окна не будет, Земля будет без текстуры.

## 5. Сборка плагина OLC

Плагину нужны заголовки SDK и две библиотеки из сборки Orbiter (раздел 3). Соберите
каталог SDK, например `C:\olc\orbiter-sdk`:

- `include` — копия `Orbitersdk\include` из репозитория Orbiter;
- `lib\Orbiter.lib` и `lib\Orbitersdk.lib` — из каталога сборки Orbiter
  (`out\build\windows-x64-release`, поиском по имени файла).

Затем в той же командной строке x64 из каталога OLC:

```bat
cmake -S . -B orbiter-lan-coop.out -G Ninja -DCMAKE_BUILD_TYPE=Release -DOLC_ORBITER_SDK_DIR=C:\olc\orbiter-sdk
cmake --build orbiter-lan-coop.out
ctest --test-dir orbiter-lan-coop.out
```

Готовый плагин — `orbiter-lan-coop.out\plugin\OrbiterLanCoop.dll`. Скопируйте его в
`Modules\Plugin` Orbiter и включите во вкладке Modules. Как запустить хост и клиент —
раздел 6.

То же самое делает сборка в GitHub Actions (`.github/workflows/build.yml`): она собирает
ядро Orbiter из базового коммита и выкладывает DLL как артефакт `OrbiterLanCoop-x64`.

## 6. Запуск хоста и клиента (этап 1)

На этапе 1 настройки задаются файлом, окна в Launchpad пока нет.

1. Скопируйте `OrbiterLanCoop.dll` в `Modules\Plugin` обеих копий Orbiter (хоста и клиента)
   и включите плагин во вкладке Modules или ключом `-p OrbiterLanCoop`.
2. Положите в `Config` каждой копии файл `OrbiterLanCoop.cfg` (пример с пояснениями —
   `plugin/OrbiterLanCoop.cfg`). У хоста `Role = host`, у клиента `Role = client` и
   `HostAddress` — адрес хоста.
3. Оба запускают **один и тот же сценарий**: корабли сопоставляются по имени. Сначала хост,
   потом клиент — клиент подключается при старте сессии и сам не переподключается.
4. В `Orbiter.log` обоих — строки `OLC ...`: подключение, раз в 10 с статистика
   (задержка, принятые пакеты, расхождение сим-времени).

На клиенте лучше выключить столкновения кораблей (`VesselCollision = FALSE` в
`Orbiter.cfg`): иначе локальный расчёт спорит с состояниями хоста. Управлять кораблём на
клиенте пока нельзя: клиент только показывает мир хоста.

Для замера расхождения положений у обоих задайте `TraceVessel` (имя летящего корабля): в
`olc-trace.csv` каждой копии окажутся положения, их можно сравнить по MJD.

Две копии с картинкой на одном экране:

- размер окна — `WindowWidth` и `WindowHeight` в `Orbiter.cfg` каждой копии, в логических
  пикселях (при масштабе экрана 200% окно 640 пикселей займёт 1280 точек). Orbiter сохраняет
  эти поля при выходе, только если в файле задан и `DeviceIndex`, иначе размер сбросится;
- записанный полёт Orbiter ищет в `Flights\<имя сценария>`: если сценарий скопирован под
  другим именем (например, чтобы сменить камеру), каталог записи тоже копируется под новым
  именем;
- на хосте во время проигрывания записи камеру ведёт запись; у клиента своя камера из
  сценария.

## 7. Обновление

Базовый коммит Orbiter меняется только осознанно (см. [README.md](README.md), условие 3):

```bat
git fetch
git checkout <новый коммит или тег релиза>
```

После этого Orbiter пересобирается, а карта ядра ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md))
сверяется с новым кодом.

## 8. Частые проблемы

| Сообщение или симптом | Что делать |
|---|---|
| Ошибки при сборке документации (LaTeX) | Пресеты Windows собирают без документации (`ORBITER_MAKE_DOC=OFF`); если включали — соберите в один поток |
| CMake не находит системные заголовки с генератором Ninja | Запускайте из «Native Tools Command Prompt», а не из обычной консоли |
| Планеты без текстур | Не указан каталог текстур (см. раздел 4) |

## 9. Удаление

Удалите каталог клона Orbiter и каталог `orbiter-lan-coop.out`. Visual Studio и DirectX SDK
удаляются через «Установка и удаление программ».
