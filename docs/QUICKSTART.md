# Быстрый старт

Плагина пока нет, поэтому здесь — подготовка окружения разработчика: Windows и сборка
Orbiter из исходников на базовом коммите. Команды взяты из `COMPILE.md` и
`CMakePresets.json` репозитория Orbiter и на наших стендах ещё не проверены — это часть
этапа 0 ([PLAN.md](PLAN.md)). После проверки файл уточняется.

## 1. Требования

- Windows 10/11 x64 (или Windows 11 ARM: Orbiter x64 работает через эмуляцию).
- Около 10 ГБ на диске для исходников, сборки и текстур.
- Доступ в интернет на время установки зависимостей.

## 2. Подготовка окружения

1. **Visual Studio** 2022 или 2026 (подойдёт бесплатная Community) с набором
   «Desktop development with C++» и компонентом «C++ MFC» во вкладке Individual Components.
2. **DirectX SDK (June 2010):** https://www.microsoft.com/en-us/download/details.aspx?id=6812.
   Ошибка установщика S1023 известна: файлы обычно всё равно распаковываются в
   `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)`.
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
git checkout b9bd0785
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
```

Сборка появится в `out\build\windows-x64-release`.

## 4. Первый запуск

1. Текстуры планет в репозиторий не входят. Укажите каталог с ними переменной
   `ORBITER_PLANET_TEXTURE_INSTALL_DIR` до сборки или параметром `PlanetTexDir` в `Orbiter.cfg`
   после (например, из установленного Orbiter 2016 или 2024).
2. Ядра SPICE и их текстуры берутся отдельно (ссылки в `COMPILE.md` Orbiter, раздел SPICE).
3. Запустите Orbiter, во вкладке Modules включите графический клиент D3D9Client, откройте
   сценарий с DeltaGlider и убедитесь, что всё летает.

## 5. Повседневное использование

Появится вместе с плагином: сборка OLC в `orbiter-lan-coop.out`, установка в
`Modules/Plugin`, запуск хоста и клиента.

## 6. Обновление

Базовый коммит Orbiter меняется только осознанно (см. [README.md](README.md), условие 3):

```bat
git fetch
git checkout <новый коммит или тег релиза>
```

После этого Orbiter пересобирается, а карта ядра ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md))
сверяется с новым кодом.

## 7. Частые проблемы

| Сообщение или симптом | Что делать |
|---|---|
| Ошибки при сборке документации (LaTeX) | Пресеты Windows собирают без документации (`ORBITER_MAKE_DOC=OFF`); если включали — соберите в один поток |
| CMake не находит системные заголовки с генератором Ninja | Запускайте из «Native Tools Command Prompt», а не из обычной консоли |
| Планеты без текстур | Не указан каталог текстур (см. раздел 4) |

## 8. Удаление

Удалите каталог клона Orbiter и каталог `orbiter-lan-coop.out`. Visual Studio и DirectX SDK
удаляются через «Установка и удаление программ».
