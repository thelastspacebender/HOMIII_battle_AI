#pragma once

#include "Role.h"
#include "TacticalFeatures.h"
#include "BattlefieldTensor.h"
#include "NeuralWeightModulator.h"
#include <vector>
#include <memory>
#include <unordered_map>

namespace BattleAI
{

struct StackSnapshot
{
    int id = -1;
    int side = 0;
    int hex = -1;
    int count = 0;
    int hpPerUnit = 0;
    int totalHp = 0;
    int speed = 0;
    int attack = 0;
    int defense = 0;
    int minDmg = 0;
    int maxDmg = 0;
    bool isShooter = false;
    bool isFlying = false;
    bool hasRetaliation = true;
    bool waitedThisTurn = false;
    float combatValue = 0.0f;
};

/**
 * @brief Level 1 Macro Director.
 * Вычисляет роли и НА КАЖДЫЙ ХОД ГЕНЕРИРУЕТ ДИНАМИЧЕСКИЕ ВЕСА W(t).
 */
class Director
{
public:
    Director() = default;

    void analyze(const std::vector<StackSnapshot>& allyStacks, 
                 const std::vector<StackSnapshot>& enemyStacks);

    /**
     * @brief Генератор динамических весов под конкретную секунду боя.
     * @param activeStack Чей сейчас ход.
     * @param nextStackInQueue Кто ходит следующим по шкале инициативы.
     * @param enemyHasResurrection Есть ли у противника заклинание поднятия мертвецов/воскрешения.
     */
    DynamicWeights generateWeightsForTurn(
        const StackSnapshot& activeStack,
        int nextStackInQueue,
        bool enemyHasResurrection
    ) const;

    StackRole getRole(int stackId) const;
    BattlePhase getPhase() const { return currentPhase; }
    int getAllyCoreId() const { return allyCoreId; }
    int getEnemyCoreId() const { return enemyCoreId; }

    /**
     * @brief Сформировать полный тензор поля боя (187 гексов x 12 каналов + контекст).
     */
    BattlefieldTensor extractTensor(
        const std::vector<StackSnapshot>& allyStacks,
        const std::vector<StackSnapshot>& enemyStacks,
        const StackSnapshot& activeStack,
        int nextStackInQueue,
        bool isAgainstBattleAI,
        int roundNumber,
        const std::vector<int>& obstacleHexes = {},
        const std::vector<int>& corpseHexes = {}
    ) const;

    /**
     * @brief Генерация динамических весов через нейросеть / матрицу поля боя.
     */
    DynamicWeights generateWeightsNeural(const BattlefieldTensor& tensor)
    {
        return modulator.predictWeights(tensor);
    }

private:
    BattlePhase currentPhase = BattlePhase::CONTAINMENT;
    std::unordered_map<int, StackRole> roleAssignments;
    int allyCoreId = -1;
    int enemyCoreId = -1;
    float strengthRatio = 1.0f;
    NeuralWeightModulator modulator;
};

} // namespace BattleAI
