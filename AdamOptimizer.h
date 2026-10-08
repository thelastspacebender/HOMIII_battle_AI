#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace BattleAI
{

/**
 * @file AdamOptimizer.h
 * @brief Полноценный оптимизатор Adam на C++20 для максимизации функции вознаграждения.
 * 
 * Формула:
 * m_t = beta1 * m_{t-1} + (1 - beta1) * g
 * v_t = beta2 * v_{t-1} + (1 - beta2) * (g^2)
 * m_hat = m_t / (1 - beta1^t)
 * v_hat = v_t / (1 - beta2^t)
 * theta_{t+1} = theta_t + lr * m_hat / (sqrt(v_hat) + eps)
 */

class AdamOptimizer
{
public:
    AdamOptimizer(size_t numParams, float learningRate = 0.05f, float beta1 = 0.9f, float beta2 = 0.999f, float eps = 1e-8f)
        : lr(learningRate), b1(beta1), b2(beta2), epsilon(eps), t(0)
    {
        m.resize(numParams, 0.0f);
        v.resize(numParams, 0.0f);
    }

    void reset()
    {
        std::fill(m.begin(), m.end(), 0.0f);
        std::fill(v.begin(), v.end(), 0.0f);
        t = 0;
    }

    /**
     * @brief Шаг оптимизатора (градиентный подъем для максимизации награды)
     * @param params Вектор параметров theta (изменяется на месте)
     * @param grad Вектор градиента вознаграждения
     */
    void step(std::vector<float>& params, const std::vector<float>& grad)
    {
        if (params.size() != m.size() || grad.size() != m.size()) return;

        t++;
        float biasCorrection1 = 1.0f - std::pow(b1, static_cast<float>(t));
        float biasCorrection2 = 1.0f - std::pow(b2, static_cast<float>(t));

        for (size_t i = 0; i < params.size(); ++i)
        {
            float g = grad[i];

            // 1-й момент
            m[i] = b1 * m[i] + (1.0f - b1) * g;
            // 2-й момент
            v[i] = b2 * v[i] + (1.0f - b2) * (g * g);

            // Скорректированные моменты
            float mHat = m[i] / biasCorrection1;
            float vHat = v[i] / biasCorrection2;

            // Обновление весов (плюс, т.к. максимизируем награду)
            params[i] += lr * (mHat / (std::sqrt(vHat) + epsilon));
        }
    }

    void setLearningRate(float newLr) { lr = newLr; }
    [[nodiscard]] float getLearningRate() const { return lr; }
    [[nodiscard]] uint32_t getStepCount() const { return t; }

private:
    float lr{0.05f};
    float b1{0.9f};
    float b2{0.999f};
    float epsilon{1e-8f};
    uint32_t t{0};

    std::vector<float> m;
    std::vector<float> v;
};

} // namespace BattleAI
