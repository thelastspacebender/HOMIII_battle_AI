#pragma once

#include "CombatStack.h"
#include "HexGrid.h"
#include "Pathfinder.h"
#include <vector>

namespace BattleAI
{

/**
 * @file EnemyBattleAI.h
 * @brief Точная реплика жадного алгоритма VCMI BattleAI для Зомби.
 * 
 * Логика поведения:
 * 1. Оценка доступных целей союзников (Archers, Pikemen).
 * 2. Оценка расстояния пути (Path Distance с учетом обхода препятствий).
 * 3. Выбор ближайшей цели (минимизация пути, приоритет добивания/атаки).
 * 4. Движение к цели через Pathfinder::getStepTowards().
 * 5. Нанесение удара в ближнем бою при достижении.
 */

enum class AIActionType
{
    MOVE,
    MELEE_ATTACK,
    MOVE_AND_ATTACK,
    WAIT,
    DEFEND
};

struct AIAction
{
    AIActionType type{AIActionType::DEFEND};
    int fromHex{-1};
    int toHex{-1};
    int targetStackId{-1};
    int targetHex{-1};
    std::string reason;
};

class EnemyBattleAI
{
public:
    /**
     * @brief Запланировать ход для отряда врага (Зомби)
     * @param activeStack Активный отряд врага
     * @param allStacks Все отряды на поле боя
     * @param staticObstacles Маска статических препятствий (камни, бревна)
     * @return Решение ИИ (действие)
     */
    static AIAction planAction(
        const CombatStack& activeStack,
        const std::vector<CombatStack>& allStacks,
        const Pathfinder::ObstacleMask& staticObstacles
    );

    /**
     * @brief Выбор лучшей цели для атаки среди союзников
     * @return ID целевого отряда или -1, если целей нет
     */
    static int selectTarget(
        const CombatStack& activeStack,
        const std::vector<CombatStack>& allStacks,
        const Pathfinder::ObstacleMask& dynamicObstacles
    );
};

} // namespace BattleAI
