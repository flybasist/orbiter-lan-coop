[English](../PACKAGE.md) · **Русский**

# Пакет испытаний

Готовые архивы для людей, которые проверяют OLC (задача 3.16, решения 43 и 50): ничего собирать не
нужно. Архивы собирает скрипт `tools/package/make-package.ps1` из готовой установки Orbiter
(см. раздел 3). Методика самих испытаний — [TEST-PROCEDURE.md](TEST-PROCEDURE.md).

## 1. Что в пакете

| Архив | Размер | Содержимое |
|---|---|---|
| `olc-1-orbiter.zip` | около 160 МБ | Orbiter базового коммита (x64) с D3D9Client, пустой каталог `Textures\Earth` |
| `olc-2-kernels.zip` | около 1,3 ГБ | Ядра SPICE (каталог `Kernels`) — без них Orbiter не стартует |
| `olc-3-plugin.zip` | около 0,1 МБ | Плагин `OrbiterLanCoop.dll`, шаблон `Config\OrbiterLanCoop.cfg`, сценарии `Scenarios\OLC-test`, начальный `Orbiter.cfg` (включены D3D9Client и плагин, столкновения кораблей выключены) |

Текстуры Земли в архив не входят (решение 50): лицензии пакетов не подтверждены, поэтому их ставят
по ссылке (раздел 2, шаг 5). Без них Земля серая, но стыковка проверяется.

## 2. Установка

Нужны Windows 10 или 11 x64 и 3 ГБ свободного места. Для картинки — видеокарта с DirectX 9
и ставятся «Распространяемый пакет Visual C++ 2015–2022 x64» и DirectX End-User Runtime
(June 2010) ([QUICKSTART.md](QUICKSTART.md), раздел 2).

1. Создайте пустой каталог без пробелов в пути, например `C:\OLC`.
2. Распакуйте в него `olc-1-orbiter.zip`, затем `olc-2-kernels.zip`, затем `olc-3-plugin.zip`
   (именно в таком порядке, в один и тот же каталог; подойдёт «Извлечь всё» проводника или
   `tar -xf <архив>` в этом каталоге). Файлы пересекаются только каталогами, ничего не
   перезаписывается.
3. Запустите `Orbiter.exe`. Графический клиент `D3D9Client` и плагин `OrbiterLanCoop` уже
   включены (`Orbiter.cfg` из `olc-3-plugin.zip`); проверить можно на вкладке Modules в Launchpad.
4. На вкладке «Extra» откройте пункт «OLC - LAN co-op»: роль (`host` или `client`), адрес хоста,
   порт и имя игрока. Те же настройки лежат в `Config\OrbiterLanCoop.cfg`; без файла плагин
   ничего не делает. Дальше — как в [QUICKSTART.md](QUICKSTART.md), раздел 6, и
   [PILOT-GUIDE.md](PILOT-GUIDE.md).
5. Текстуры Земли (по желанию): установите Orbiter 2016 (ссылка на форуме Orbiter, см.
   [TEXTURES.md](TEXTURES.md), раздел 2) и скопируйте из него каталог `Textures\Earth` в `Textures`
   своей установки. Так же ставятся текстуры Луны и других тел.
6. Брандмауэр: разрешите `Orbiter.exe` в частной сети; порт хоста по умолчанию 47800 (TCP и UDP).

Проверка: в `Orbiter.log` после запуска сценария с плагином есть строки `OLC: session started`
и `OLC: running as host` (или `client`).

## 3. Как собрать архивы

На Windows x64, где есть готовая установка Orbiter с D3D9Client (раздел 3 `QUICKSTART.md`,
`cmake --install`) и собранный плагин (раздел 5):

```bat
powershell -ExecutionPolicy Bypass -File tools\package\make-package.ps1 -OrbiterDir C:\olc\orbiter -PluginDll <путь>\OrbiterLanCoop.dll -OutDir C:\olc\package
```

Скрипт не берёт из установки то, что относится к одной машине: `Orbiter.cfg`, журналы, трассы,
рабочие сценарии `OLC` и `OLC-test`, плагин `OlcRigTest`, SDK; начальный `Orbiter.cfg` он пишет сам. Архивы лежат в `OutDir`.

## 4. Проверка 2026-11-18

Архивы собраны на Windows 11 x64 (Proxmox) из установки базового коммита `a7a3396e`, распакованы
по разделу 2 в пустой каталог на второй машине (Windows 10 x64, Proxmox) и запущены без графики:
`Orbiter.exe -s "OLC-test\Two DG 20 m" -p OrbiterLanCoop -T 15`. В журнале: `OLC host: listening
on port 47800`, `2 vessels`, `Fast process shutdown`, код выхода 0. С картинкой — на Windows 11 ARM
(Parallels, 3D): пакет распакован по разделу 2 в пустой каталог, `Orbiter.exe` запущен без ключей,
D3D9Client и плагин включились из начального `Orbiter.cfg`, «Connect» к хосту без графики со
сценарием `Two DG 20 m` — клиент в мире хоста, на экране два DeltaGlider, «you fly 'GL-01'».
