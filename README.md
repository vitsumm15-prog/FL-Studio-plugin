# Zarya — VST3-синтезатор (wavetable / sample / granular)

Полифонический синтезатор на C++17 + JUCE 8. DSP-ядро не зависит от JUCE и покрыто офлайн-тестом.

## Возможности

- 3 осциллятора, у каждого режимы: Wavetable (8 банков по 16 кадров, band-limited mip-уровни), Sample (старт, петля с кроссфейдом), Granular (размер, плотность, разброс, скорость сканирования).
- Unison до 7 голосов на осциллятор, detune, стерео-ширина, октава/полутон/fine, уровень, панорама.
- Фильтр: LP12, LP24, HP12, HP24, BP, Notch; резонанс, drive, keytrack.
- 4 ADSR-огибающие (первая — громкость), 4 LFO (sine, triangle, saw, square, S&H, smooth random, Lorenz, Rossler), синхронизация с темпом хоста.
- Матрица модуляции на 8 слотов (огибающие, LFO, velocity, mod wheel, нота, random → высота, позиция, уровень, панорама, cutoff, резонанс, drive, amp).
- Режимы голосов: Poly (до 16), Mono, Legato, glide, pitch bend, sustain-педаль.
- Арпеджиатор: Up, Down, Up-Down, Random, As Played; 1–4 октавы; gate; синхронизация с хостом.
- Эффекты: дисторшн → хорус → стерео-дилей (ping-pong, синхро) → ревербератор; мягкий лимитер на выходе.
- Загрузка семплов WAV/AIFF/FLAC/OGG/MP3 кнопкой или перетаскиванием, отображение формы волны; пути к семплам сохраняются в проекте.
- Standalone-версия с экранной клавиатурой.

## Структура

```
CMakeLists.txt
Source/
  PluginProcessor.h/.cpp   — процессор, MIDI, транспорт, состояние, загрузка семплов
  Parameters.cpp           — все параметры (APVTS) и их привязка к движку
  PluginEditor.h/.cpp      — интерфейс: вкладки, группы регуляторов, вид семплов, клавиатура
  dsp/                     — независимое DSP-ядро (namespace zarya)
Tests/EngineTest.cpp       — офлайн-рендер 8 сценариев в WAV + проверка NaN/пиков/скорости
```

## Сборка

**Готовая сборка для Windows:** GitHub Actions (`.github/workflows/windows.yml`) при каждом push собирает артефакт `Zarya-Windows-x64` — установщик `Zarya-*-Setup.exe` (VST3 в `C:\Program Files\Common Files\VST3`, Standalone в `C:\Program Files\Zarya`) и портативный zip. Установщик не подписан — Windows SmartScreen попросит нажать «Подробнее → Выполнить в любом случае».

Нужны CMake 3.22+, Git и компилятор C++17. JUCE 8 скачается автоматически; можно положить его копию в папку `JUCE/` рядом с `CMakeLists.txt`.

**Windows (Visual Studio 2022):**

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Плагин: `build/Zarya_artefacts/Release/VST3/Zarya.vst3` → скопировать в `C:\Program Files\Common Files\VST3`.

**macOS (Xcode):**

```
cmake -B build -G Xcode
cmake --build build --config Release
```

Плагин: `build/Zarya_artefacts/Release/VST3/Zarya.vst3` → скопировать в `~/Library/Audio/Plug-Ins/VST3`.

**Linux:** установить зависимости JUCE (libasound2-dev, libjack-jackd2-dev, libfreetype-dev, libfontconfig1-dev, libx11-dev, libxrandr-dev, libxinerama-dev, libxcursor-dev, libxcomposite-dev, libxext-dev, libgl1-mesa-dev), затем `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`.

Плагин: `build/Zarya_artefacts/Release/VST3/Zarya.vst3` → скопировать в `~/.vst3`. Standalone (`build/Zarya_artefacts/Release/Standalone/Zarya`) работает через ALSA или JACK/PipeWire — выбор в Options → Audio/MIDI Settings.

## Тест DSP-ядра

Собирается вместе с проектом (`ZaryaEngineTest`) или отдельно без JUCE:

```
g++ -std=c++17 -O2 Tests/EngineTest.cpp Source/dsp/*.cpp -o enginetest && ./enginetest
```

Ожидаемый результат: `ALL TESTS PASSED` и файлы `zarya_test_*.wav`.

## Статус и ограничения

- DSP-ядро собрано и проверено (g++ -Wall -Wextra без предупреждений, все тесты проходят, стресс-тест 16 голосов + все эффекты ≈ 20× быстрее реального времени).
- Обёртка JUCE (процессор, параметры, интерфейс, CMake) собрана с JUCE 8.0.4 на Linux (GCC 13) без ошибок; VST3 и Standalone запускаются, звук проверен через JACK. Сборка под Windows/macOS пока не проверялась.
- Интерфейс функциональный (собирается автоматически из параметров), без кастомной графики и пресетов.
- Семплы хранятся по абсолютному пути; если файл перемещён, используется встроенный семпл. Максимальная длина семпла — 60 с (сводится в моно).
