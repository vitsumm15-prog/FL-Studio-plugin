// Zarya Synth — банк процедурных вейвтаблов с mip-уровнями (антиалиасинг).
#pragma once

#include "Common.h"
#include <string>
#include <vector>

namespace zarya
{
class WavetableBank
{
public:
    static constexpr int kTableSize = 2048;           // сэмплов на один период
    static constexpr int kStride    = kTableSize + 1;  // +1 защитный сэмпл для интерполяции
    static constexpr int kFrames    = 16;              // кадров морфинга на таблицу
    static constexpr int kLevels    = 11;              // mip-уровни: 1024, 512, ... 1 гармоника
    static constexpr int kNumTables = 8;

    struct Table
    {
        std::string name;
        std::vector<float> data; // [frame][level][kStride]

        const float* get (int frame, int level) const noexcept
        {
            return data.data() + ((size_t) frame * kLevels + (size_t) level) * kStride;
        }
    };

    // Единый на процесс банк (строится один раз, потокобезопасная инициализация).
    // Вызовите заранее в конструкторе плагина, чтобы не строить его в аудиопотоке.
    static const WavetableBank& instance();

    int numTables() const noexcept { return (int) tables.size(); }
    const Table& table (int index) const noexcept { return tables[(size_t) clamp (index, 0, numTables() - 1)]; }

    // Выбор mip-уровня: гармоники допускаются до max(sr/2, sr - 18 кГц),
    // т.е. отражённые частоты попадают только в неслышимую область выше 18 кГц.
    static int levelForFrequency (float hz, float sampleRate) noexcept;

    static const char* const* tableNames() noexcept; // английские идентификаторы

    WavetableBank (const WavetableBank&) = delete;
    WavetableBank& operator= (const WavetableBank&) = delete;

private:
    WavetableBank();
    std::vector<Table> tables;
};
} // namespace zarya
