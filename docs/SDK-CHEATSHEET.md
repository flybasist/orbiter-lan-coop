# Шпаргалка по SDK Orbiter для OLC

«Что вызвать, чтобы…» — только функции, нужные OLC. Почему именно так и что внутри — в
[ORBITER-INTERNALS.md](ORBITER-INTERNALS.md). Заголовки: `Orbitersdk/include/OrbiterAPI.h`
(функции `oapi*`), `VesselAPI.h` (класс `VESSEL`), `ModuleAPI.h` (класс `oapi::Module`).
Базовый коммит — `dabade26`. Всё получено чтением кода; поведение подтверждается на этапе 1.

## 1. Жизненный цикл плагина

| Задача | Вызов | Подвох |
|---|---|---|
| Начать сессию | `clbkSimulationStart` | Плагин живёт между сессиями: всё состояние сессии создавать здесь |
| Закончить сессию | `clbkSimulationEnd` | Мир уже уничтожен: корабли не трогать, только сеть и таблицы |
| Работа каждый кадр | `clbkPreStep` | Не вызывается на паузе |
| Работа на паузе и при неактивном окне | Своё скрытое окно (`HWND_MESSAGE`) с `SetTimer` или `PostMessage` | Покадровых колбэков на паузе нет; сообщения окну приходят всегда (`ORBITER-INTERNALS.md`, раздел 14) |
| Создать сеть и потоки | В `clbkSimulationStart`, не в `InitModule` | `InitModule`/`ExitModule` работают внутри `DllMain` |
| Появился или удалён корабль | `clbkNewVessel`, `clbkDeleteVessel` | `clbkNewVessel` — только для созданных во время работы, до конца их настройки |
| Ускорение или пауза изменились | `clbkTimeAccChanged`, `clbkPause` | В `clbkTimeAccChanged` оба аргумента — новое значение; `oapiSetPause` срабатывает только в следующем кадре |
| Прыжок во времени | `clbkTimeJump` | — |

## 2. Состояние кораблей

| Задача | Вызов | Подвох |
|---|---|---|
| Перебрать корабли | `oapiGetVesselCount`, `oapiGetVesselByIndex`, `oapiGetVesselInterface` | — |
| Найти корабль по имени | `oapiGetVesselByName` | Без учёта регистра, первый найденный; уникальность имён ядро не проверяет |
| Имя и класс | `VESSEL::GetName`, `VESSEL::GetClassName` | Класс может содержать подкаталог |
| Снять состояние (хост) | `VESSEL::GetStatusEx` с `VESSELSTATUS2`, `version = 2` | В `clbkPreStep`; время состояния — `oapiGetSimTime`. Списки с флагами `VS_*LIST` освобождает вызывающий |
| Навязать полёт (клиент) | `VESSEL::DefSetStateEx`, `status = 0` | **Только в `clbkPreStep`:** в `clbkPostStep` теряются ориентация и вращение. Не опускать ниже рельефа — поднимет сам |
| Навязать «стоит на поверхности» | `DefSetStateEx`, `status = 1`, `surf_lng/lat/hdg` | Один раз при посадке, не каждый кадр. Не внутри `clbkPostStep` |
| Только ориентация / вращение | `SetGlobalOrientation`, `SetRotationMatrix`, `SetAngularVel` | Лёгкие, учитывают суперкорабль; только в `clbkPreStep` |
| Отдельно положение или скорость | — | В SDK нет: только `DefSetStateEx` |
| Статус полёта | `VESSEL::GetFlightStatus` | Бит 0 — стоит, бит 1 — часть суперкорабля |
| Топливо и уровни двигателей | `VS_FUELLIST`, `VS_THRUSTLIST` в `VESSELSTATUS2`; `SetPropellantMass`, `SetThrusterLevel` | Топливо — до тяги: двигатель с пустым баком ядро обнуляет каждый шаг. Пустую массу (`SetEmptyMass`) передавать отдельно |
| Анимации, огни, маяки | `GetAnimPtr`/`SetAnimation`, `GetLightEmitter`, `GetBeacon` | Анимация — только картинка, внутреннее состояние модуля корабля не меняется (`ORBITER-INTERNALS.md`, раздел 17) |

По сети не передавать `OBJHANDLE`: опорное тело, база, корабли — по имени
(`oapiGetObjectName`, `oapiGetGbodyByName`, `oapiGetBaseByName`).

## 3. Ввод: клавиша на клиенте → вызов на хосте

Стандартные удерживаемые клавиши ядро применяет только к фокусному кораблю. Для корабля
клиента хост повторяет их сам. `dt` — `oapiGetSimStep()`.

| Клавиша по умолчанию | Вызов на хосте для корабля клиента |
|---|---|
| Numpad2/8/1/3/4/6 — RCS вращение (режим RCS 1) | `IncThrusterGroupLevel_SingleStep (THGROUP_ATT_PITCHUP…BANKRIGHT, 1.0)` каждый кадр, пока нажата |
| Numpad2/8/1/3/6/9 — RCS перемещение (режим RCS 2) | `IncThrusterGroupLevel_SingleStep (THGROUP_ATT_UP…BACK, 1.0)` |
| Ctrl + те же — малая мощность RCS | То же с `0.1` |
| Numpad+ / Numpad- — полный главный / тормозной, пока нажата | `IncThrusterGroupLevel_SingleStep (THGROUP_MAIN, 1.0)` и `(THGROUP_RETRO, -1.0)`; для тормозного наоборот |
| Ctrl+Numpad+ / Ctrl+Numpad- — газ плавно | Своя логика по `Get/SetThrusterGroupLevel` (ниже), шаг `0.2 * dt` |
| Numpad* — выключить главный и тормозной | `SetThrusterGroupLevel (THGROUP_MAIN, 0)`, `(THGROUP_RETRO, 0)` |
| Numpad0 / Numpad. — зависание больше / меньше | `IncThrusterGroupLevel (THGROUP_HOVER, ±0.2 * dt)` |
| Delete / Insert — триммер | Свой накопитель `trim ± 0.25 * dt` в пределах ±1, затем `SetControlSurfaceLevel` |
| Comma / Period — тормоз левого / правого колеса | `SetWheelbrakeLevel (1.0, 1 или 2, false)` каждый кадр, пока нажата |
| Разовые клавиши: автопилоты, RCS вкл/режим, расстыковка, клавиши дополнений | `VESSEL::SendBufferedKey (key, true, kstate)` |
| Клавиши HUD | Не пересылать: действуют на HUD фокусного корабля хоста |

Режим RCS: `GetAttitudeMode`, `SetAttitudeMode` (0 — выкл, 1 — вращение, 2 — перемещение).
Режим аэродинамических органов: `GetADCtrlMode`, `SetADCtrlMode`.

Плавный газ (как `Vessel::IncMainRetroLevel`, `Vessel.cpp:2101`), `d = ±0.2 * dt`:

- `d > 0`: если тормозной > 0 — уменьшить тормозной на `d` (не ниже 0) и главный = 0;
  иначе главный = min(главный + `d`, 1);
- `d < 0`: если главный > 0 — уменьшить главный на `|d|` (не ниже 0) и тормозной = 0;
  иначе тормозной = min(тормозной + `|d|`, 1).

Чего сделать нельзя: доставить удерживаемые клавиши в `clbkConsumeDirectKey` корабля, который
не в фокусе. Собственные удерживаемые клавиши дополнений на хосте работать не будут.

### Перехват ввода на клиенте

| Задача | Вызов | Подвох |
|---|---|---|
| Удерживаемые клавиши | `clbkProcessKeyboardImmediate (kstate, simRunning)` | `true` отключает стандартную обработку на весь кадр, в том числе клавиши корабля |
| Разовые нажатия | `clbkProcessKeyboardBuffered (key, kstate, simRunning)` | Только нажатие; `true` убирает клавишу из очереди |
| Джойстик: RCS | `GetManualControlLevel (группа, MANCTRL_ANYMODE, MANCTRL_JOYSTICK)` | Значения прошлого кадра; работает, только если джойстик включён в Orbiter |
| Джойстик: газ | `GetThrusterGroupLevel (THGROUP_MAIN)` фокусного корабля | Ручка газа ставит постоянный уровень |
| Клавиатура вне колбэков | `oapiGetKeyState` | Состояние прошлого кадра; пусто, когда клавиатуру забрало окно ImGui |
| Мышь | `clbkProcessMouse` | `true` скрывает событие от ядра |

## 4. Время

| Задача | Вызов | Подвох |
|---|---|---|
| Время, шаг | `oapiGetSimTime`, `oapiGetSimStep`, `oapiGetSimMJD` | `oapiGetSimTime` — начало текущего шага |
| Ускорение | `oapiGetTimeAcceleration`, `oapiSetTimeAcceleration` | Меняется плавно, не мгновенно |
| Пауза | `oapiGetPause`, `oapiSetPause` | Применяется в начале следующего кадра |
| Большое расхождение времени | `oapiSetSimMJD (mjd, pmode)` | Тяжёлая операция, только при больших расхождениях |

## 5. Сценарии и корабли

| Задача | Вызов | Подвох |
|---|---|---|
| Сохранить мир | `oapiSaveScenario (fname, desc)` | Только в файл в `Scenarios`, подкаталоги должны существовать |
| Загрузить сценарий во время работы | — | Невозможно из SDK |
| Создать корабль | `oapiCreateVesselEx (name, classname, &vs2)` | **Нет файла класса — Orbiter завершается.** Сначала проверить `Config\Vessels\<класс>.cfg` |
| Удалить корабль | `oapiDeleteVessel (h)` | Удаление в конце кадра; последний корабль, который может быть в фокусе, удалять нельзя |
| Сменить фокус | `oapiSetFocusObject (h)` | — |

## 6. Стыковка

| Задача | Вызов | Подвох |
|---|---|---|
| Узнать стыковки | `GetStatusEx` с `VS_DOCKINFOLIST` → `dockinfo[].idx/ridx/rvessel` | `rvessel` — handle, по сети имя |
| Состыковать на клиенте | `VESSEL::Dock (target, n, tgtn, 0)` | Режим 0 — без телепортации, корабли уже должны быть совмещены. Коды: 0 — успех, 1/2 — порт занят, 3 — уже в связке |
| Расстыковать | `VESSEL::Undock (n)` | Расталкивает корабли и на 5 с выключает их столкновения |
| Вести связку | `DefSetStateEx` + `SetGlobalOrientation` + `SetAngularVel` на одном корабле | Не задавать состояние каждой части |
| `VS_DOCKINFOLIST` в `DefSetStateEx` | — | Во время работы не использовать: только указатели, без суперкорабля |

## 7. Интерфейс

| Задача | Вызов | Подвох |
|---|---|---|
| Пункт настроек в Launchpad | `LaunchpadItem` + `oapiRegisterLaunchpadItem` в `InitModule` | Окно — диалог Win32, не ImGui; ImGui в Launchpad нет |
| Сохранить настройки перед стартом | `LaunchpadItem::clbkWriteConfig` | Вызывается перед каждой сессией, отменить запуск нельзя |
| Запустить сценарий | Нет в SDK | Только перезапуск `orbiter.exe -s <сценарий>` |
| Окно в симуляции | `ImGuiDialog` + `oapiOpenDialog` | Вне сессии падает; рисуется и на паузе; без графического клиента окон нет |
| Открыть окно игроку | `oapiRegisterCustomCmd`, `oapiRegisterCustomMenuCmd` | Пока фокус в окне ImGui, клавиатура уходит ему |
| Короткое сообщение | `oapiAddNotification` | — |
| Постоянный текст на экране | `oapiCreateAnnotation` | Только в сессии и с графическим клиентом; рисуется через ImGui, скрывается вместе с окнами; текст в UTF-8 |

Подробности — `ORBITER-INTERNALS.md`, раздел 16.
