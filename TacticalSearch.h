#pragma once

#include "TacticalFeatures.h"
#include "CombatStack.h"
#include "HexGrid.h"
#include "Pathfinder.h"
#include "DamageCalculator.h"
#include <vector>

namespace BattleAI
{

/**
 * @file TacticalSearch.h
 * @brief Тактический микро-поиск легальных действий (Level 2 Micro Search).
 * 
 * Генерирует 100% физически легальных действий:
 * - Прямой / штрафной выстрел
 * - Перемещение на любой проходимый гекс в пределах очков хода
 * - Удар в ближнем бою с любого доступного направления
 * - Выжидание (Wait) или Защита (Defend)
 * 
 * Оценивает полезность каждого хода через динамические веса W(t).
 */

class TacticalSearch
{
public:
    /**
     * @brief Генерация всех 100% легальных ходов для активного отряда
     */
    static std::vector<MoveEvaluation> generateCandidates(
        const CombatStack& activeStack,
        const std::vector<CombatStack>& allStacks,
        const Pathfinder::ObstacleMask& staticObstacles
    );

    /**
     * @brief Найти наилучшее легальное действие с максимальной полезностью Utility
     */
    static MoveEvaluation findBestAction(
        const CombatStack& activeStack,
        const std::vector<CombatStack>& allStacks,
        const Pathfinder::ObstacleMask& staticObstacles,
        const DynamicWeights& weights
    );

private:
    /**
     * @brief Вычисление признаков тактического хода (features)
     */
    static void evaluateFeatures(
        MoveEvaluation& eval,
        const CombatStack& activeStack,
        const std::vector<CombatStack>& allStacks,
        const Pathfinder::ObstacleMask& staticObstacles
    );
};

} // namespace BattleAI
