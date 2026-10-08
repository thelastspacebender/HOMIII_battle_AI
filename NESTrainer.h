#pragma once

#include "NeuralWeightModulator.h"
#include "AdamOptimizer.h"
#include "CombatStack.h"
#include "TurnQueue.h"
#include "DamageCalculator.h"
#include "TacticalSearch.h"
#include "EnemyBattleAI.h"
#include <vector>
#include <random>

namespace BattleAI
{

/**
 * @file NESTrainer.h
 * @brief Обучение тактического ИИ методом Natural Evolution Strategies (NES) + Adam Optimizer.
 */

struct BattleStats
{
    bool victory{false};
    bool zeroLossArchers{false};
    float reward{0.0f};
    int archersRemaining{0};
    int pikemenRemaining{0};
    int zombiesKilled{0};
    int roundsPlayed{0};
};

struct TrainingEpochStats
{
    int epoch{0};
    float meanReward{0.0f};
    float winRate{0.0f};
    float zeroLossRate{0.0f};
    float learningRate{0.0f};
    DynamicWeights currentWeights;
};

class NESTrainer
{
public:
    NESTrainer(NeuralWeightModulator& modulator, float learningRate = 0.05f, float sigma = 0.1f, int populationSize = 16);

    /**
     * @brief Запустить симуляцию одного полного матча
     */
    static BattleStats simulateSingleBattle(
        NeuralWeightModulator& brain,
        std::mt19937& rng,
        bool againstBattleAI = true,
        int maxRounds = 35
    );

    /**
     * @brief Выполнить одну эпоху обучения NES + Adam
     */
    TrainingEpochStats trainEpoch(int epochIndex);

    /**
     * @brief Инициализация базового сценария Погоста (15 Лучников + 2 Пикинера vs 35 Зомби)
     */
    static std::vector<CombatStack> createCryptScenario();
    static Pathfinder::ObstacleMask createCryptObstacles();

    void setLearningRate(float lr) { adam.setLearningRate(lr); }
    [[nodiscard]] float getSigma() const { return sigma; }

private:
    NeuralWeightModulator& brain;
    AdamOptimizer adam;
    float sigma{0.1f};
    int popSize{16};
    std::mt19937 rng;
};

} // namespace BattleAI
