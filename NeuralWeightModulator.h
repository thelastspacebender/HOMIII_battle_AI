#pragma once

#include "BattlefieldTensor.h"
#include "TacticalFeatures.h"
#include <vector>
#include <string>
#include <fstream>
#include <cmath>

namespace BattleAI
{

/**
 * @file NeuralWeightModulator.h
 * @brief Нейромодулятор динамических весов (Split-Matrix Conditioning Head).
 * 
 * Преобразует 28-канальный пространственный тензор поля боя (5240 чисел)
 * в вектор динамических тактических весов W(t).
 * 
 * Архитектура:
 * 1. Encoder Dense: 5240 -> 64 (ReLU)
 * 2. Split Conditioning Head: 64 -> 16
 *    - Rows 0..7: Режим против BattleAI (эксплойт жадности, байт на пикинера, кайт)
 *    - Rows 8..15: Режим против Human (оборона коробкой, позиционирование, прикрытие)
 */

class NeuralWeightModulator
{
public:
    static constexpr size_t INPUT_DIM = TENSOR_FLAT_SIZE; // 5240
    static constexpr size_t HIDDEN_DIM = 64;
    static constexpr size_t WEIGHT_OUTPUTS = 8;
    static constexpr size_t MATRIX_TOTAL_ROWS = WEIGHT_OUTPUTS * 2; // 16 строк
    static constexpr size_t TOTAL_PARAMS = (HIDDEN_DIM * INPUT_DIM) + HIDDEN_DIM + (MATRIX_TOTAL_ROWS * HIDDEN_DIM) + MATRIX_TOTAL_ROWS;

    NeuralWeightModulator();

    /**
     * @brief Прямой проход: Тензор [5240] -> Динамические веса W(t)
     * @param tensor Пространственный тензор поля боя
     * @param againstBattleAI true = выбор верхней половины матрицы, false = нижней
     */
    DynamicWeights predictWeights(const BattlefieldTensor& tensor, bool againstBattleAI = true);

    /**
     * @brief Получить плоский вектор всех обучаемых параметров (для Adam и NES)
     */
    [[nodiscard]] std::vector<float> getFlatWeights() const;
    void setFlatWeights(const std::vector<float>& params);

    [[nodiscard]] std::vector<float> getTrainableHeadParams() const { return b2_dual; }
    void setTrainableHeadParams(const std::vector<float>& params)
    {
        if (params.size() == b2_dual.size()) b2_dual = params;
    }

    /**
     * @brief Инициализация весов методом He / Xavier + калибровочные смещения
     */
    void initializeDefaultWeights();

    /**
     * @brief Сохранение весов в бинарный файл
     */
    bool saveWeights(const std::string& filepath) const;

    /**
     * @brief Загрузка весов из бинарного файла
     */
    bool loadWeights(const std::string& filepath);

    [[nodiscard]] size_t getParameterCount() const { return TOTAL_PARAMS; }

private:
    // W1: [HIDDEN_DIM x INPUT_DIM] (64 x 5240 = 335,360)
    std::vector<float> W1;
    // b1: [HIDDEN_DIM] (64)
    std::vector<float> b1;

    // W2_dual: [16 x 64] (1,024)
    std::vector<float> W2_dual;
    // b2_dual: [16] (16)
    std::vector<float> b2_dual;
};

} // namespace BattleAI
