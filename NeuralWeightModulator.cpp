#include "NeuralWeightModulator.h"
#include <random>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace BattleAI
{

NeuralWeightModulator::NeuralWeightModulator()
{
    W1.resize(HIDDEN_DIM * INPUT_DIM, 0.0f);
    b1.resize(HIDDEN_DIM, 0.0f);
    W2_dual.resize(MATRIX_TOTAL_ROWS * HIDDEN_DIM, 0.0f);
    b2_dual.resize(MATRIX_TOTAL_ROWS, 0.0f);

    initializeDefaultWeights();
}

void NeuralWeightModulator::initializeDefaultWeights()
{
    // Инициализация Kaiming / He для весов
    std::mt19937 rng(1337);
    float stddev1 = std::sqrt(2.0f / static_cast<float>(INPUT_DIM));
    std::normal_distribution<float> dist1(0.0f, stddev1);

    for (auto& w : W1)
    {
        w = dist1(rng) * 0.1f;
    }
    std::fill(b1.begin(), b1.end(), 0.01f);

    float stddev2 = std::sqrt(2.0f / static_cast<float>(HIDDEN_DIM));
    std::normal_distribution<float> dist2(0.0f, stddev2);

    for (auto& w : W2_dual)
    {
        w = dist2(rng) * 0.1f;
    }

    // Калибровочные базовые смещения:
    // Верхняя половина (0..7) - BattleAI:
    // 0: strip_retal, 1: block_shooter, 2: stand_on_corpse, 3: screen_core,
    // 4: kite_distance, 5: bait_diversion, 6: dmg_dealt, 7: dmg_penalty
    b2_dual[0] = 3.0f;  // strip_retal
    b2_dual[1] = 1.0f;  // block_shooter
    b2_dual[2] = 0.5f;  // stand_on_corpse
    b2_dual[3] = 1.5f;  // screen_core
    b2_dual[4] = 4.0f;  // kite_distance (Высокий для кайта зомби!)
    b2_dual[5] = 5.0f;  // bait_diversion (Высокий для байта на пикинера!)
    b2_dual[6] = 2.0f;  // dmg_dealt
    b2_dual[7] = 3.0f;  // dmg_penalty

    // Нижняя половина (8..15) - Human:
    b2_dual[8]  = 1.0f; // strip_retal
    b2_dual[9]  = 2.0f; // block_shooter
    b2_dual[10] = 0.5f; // stand_on_corpse
    b2_dual[11] = 4.5f; // screen_core (Высокий: плотная защита стрелка коробкой!)
    b2_dual[12] = 2.0f; // kite_distance
    b2_dual[13] = 1.0f; // bait_diversion (Низкий: человек не ведется на глупый байт!)
    b2_dual[14] = 2.0f; // dmg_dealt
    b2_dual[15] = 4.0f; // dmg_penalty
}

DynamicWeights NeuralWeightModulator::predictWeights(const BattlefieldTensor& tensor, bool againstBattleAI)
{
    std::vector<float> input = tensor.flatten();
    std::array<float, HIDDEN_DIM> hidden{};

    // Слой 1: 5240 -> 64 с функцией активации ReLU
    for (size_t j = 0; j < HIDDEN_DIM; ++j)
    {
        float sum = b1[j];
        const float* rowW = &W1[j * INPUT_DIM];
        
        // Векторное накопление суммы
        for (size_t k = 0; k < INPUT_DIM; ++k)
        {
            sum += rowW[k] * input[k];
        }

        hidden[j] = std::max(0.0f, sum); // ReLU
    }

    // Слой 2 (Conditioning Split Head): 64 -> 8 весов
    size_t rowOffset = againstBattleAI ? 0 : 8;
    std::array<float, WEIGHT_OUTPUTS> rawOut{};

    for (size_t i = 0; i < WEIGHT_OUTPUTS; ++i)
    {
        size_t rowIdx = rowOffset + i;
        float sum = b2_dual[rowIdx];
        const float* rowW = &W2_dual[rowIdx * HIDDEN_DIM];

        for (size_t j = 0; j < HIDDEN_DIM; ++j)
        {
            sum += rowW[j] * hidden[j];
        }

        // Softplus / ReLU для гарантированной неотрицательности тактических весов
        rawOut[i] = (sum > 20.0f) ? sum : std::log1p(std::exp(sum));
    }

    DynamicWeights dw;
    dw.w_strip_retaliation     = rawOut[0];
    dw.w_block_shooter         = rawOut[1];
    dw.w_stand_on_corpse       = rawOut[2];
    dw.w_screen_core           = rawOut[3];
    dw.w_kite_distance         = rawOut[4];
    dw.w_bait_diversion        = rawOut[5];
    dw.w_dmg_dealt             = rawOut[6];
    dw.w_dmg_received_penalty  = rawOut[7];

    return dw;
}

std::vector<float> NeuralWeightModulator::getFlatWeights() const
{
    std::vector<float> flat;
    flat.reserve(TOTAL_PARAMS);

    flat.insert(flat.end(), W1.begin(), W1.end());
    flat.insert(flat.end(), b1.begin(), b1.end());
    flat.insert(flat.end(), W2_dual.begin(), W2_dual.end());
    flat.insert(flat.end(), b2_dual.begin(), b2_dual.end());

    return flat;
}

void NeuralWeightModulator::setFlatWeights(const std::vector<float>& params)
{
    if (params.size() != TOTAL_PARAMS) return;

    size_t offset = 0;
    std::copy_n(params.data() + offset, W1.size(), W1.data());
    offset += W1.size();

    std::copy_n(params.data() + offset, b1.size(), b1.data());
    offset += b1.size();

    std::copy_n(params.data() + offset, W2_dual.size(), W2_dual.data());
    offset += W2_dual.size();

    std::copy_n(params.data() + offset, b2_dual.size(), b2_dual.data());
}

bool NeuralWeightModulator::saveWeights(const std::string& filepath) const
{
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    uint32_t magic = 0x484F4D4D; // "HOMM"
    uint32_t count = static_cast<uint32_t>(TOTAL_PARAMS);
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    auto flat = getFlatWeights();
    out.write(reinterpret_cast<const char*>(flat.data()), flat.size() * sizeof(float));
    return true;
}

bool NeuralWeightModulator::loadWeights(const std::string& filepath)
{
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) return false;

    uint32_t magic = 0;
    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));

    if (magic != 0x484F4D4D || count != TOTAL_PARAMS) return false;

    std::vector<float> flat(TOTAL_PARAMS);
    in.read(reinterpret_cast<char*>(flat.data()), flat.size() * sizeof(float));

    setFlatWeights(flat);
    return true;
}

} // namespace BattleAI
