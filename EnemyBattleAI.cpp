#include "EnemyBattleAI.h"
#include "DamageCalculator.h"
#include <limits>
#include <cmath>

namespace BattleAI
{

int EnemyBattleAI::selectTarget(
    const CombatStack& activeStack,
    const std::vector<CombatStack>& allStacks,
    const Pathfinder::ObstacleMask& dynamicObstacles)
{
    int bestTargetId = -1;
    float bestScore = -1e9f;

    for (const auto& candidate : allStacks)
    {
        if (!candidate.isAlive || candidate.side == activeStack.side) continue;

        // Расстояние пути до цели с огибанием препятствий
        int pathDist = Pathfinder::getPathDistance(
            activeStack.positionHex,
            candidate.positionHex,
            activeStack.isFlying,
            dynamicObstacles,
            true
        );

        if (pathDist >= 999) continue; // Недостижима

        // Базовая ценность цели
        float unitValue = (candidate.role == StackRole::CORE) ? 150.0f : 80.0f;
        float totalValue = candidate.count * unitValue;

        // HoMM3 AI фича: огромный бонус за полное уничтожение отряда (вайп стека)
        int estimatedDmg = DamageCalculator::estimateDamage(activeStack, candidate, false);
        if (estimatedDmg >= candidate.getTotalHp())
        {
            totalValue += 800.0f; // Бонус за гарантированное устранение отряда из очереди ходов
        }

        // Если цель в зоне непосредственного удара на этом ходу - колоссальный приоритет
        float reachBonus = 0.0f;
        if (pathDist <= activeStack.getEffectiveSpeed() || 
            HexGrid::isAdjacent(BattleHex(activeStack.positionHex), BattleHex(candidate.positionHex)))
        {
            reachBonus = 5000.0f;
        }

        // Формула VCMI / HoMM3: штраф за количество ходов до цели (медленный стек преследует ближайших)
        int spd = std::max(1, activeStack.getEffectiveSpeed());
        int turnsToReach = (pathDist + spd - 1) / spd;
        float distFactor = std::pow(3.5f, static_cast<float>(turnsToReach));
        float score = reachBonus + (totalValue / distFactor);

        if (score > bestScore)
        {
            bestScore = score;
            bestTargetId = candidate.id;
        }
    }

    return bestTargetId;
}

AIAction EnemyBattleAI::planAction(
    const CombatStack& activeStack,
    const std::vector<CombatStack>& allStacks,
    const Pathfinder::ObstacleMask& staticObstacles)
{
    AIAction action;
    action.fromHex = activeStack.positionHex;
    action.toHex = activeStack.positionHex;

    // Сборка динамической маски препятствий
    Pathfinder::ObstacleMask dynamicObstacles = staticObstacles;
    for (const auto& s : allStacks)
    {
        if (s.isAlive && s.id != activeStack.id && s.positionHex >= 0)
        {
            dynamicObstacles.set(s.positionHex);
        }
    }

    int targetId = selectTarget(activeStack, allStacks, dynamicObstacles);
    if (targetId == -1)
    {
        action.type = AIActionType::DEFEND;
        action.reason = "No accessible targets found on battlefield";
        return action;
    }

    const CombatStack* target = nullptr;
    for (const auto& s : allStacks)
    {
        if (s.id == targetId) { target = &s; break; }
    }
    if (!target)
    {
        action.type = AIActionType::DEFEND;
        return action;
    }

    action.targetStackId = target->id;
    action.targetHex = target->positionHex;

    // 1. Проверка: уже стоим вплотную к цели?
    if (HexGrid::isAdjacent(BattleHex(activeStack.positionHex), BattleHex(target->positionHex)))
    {
        action.type = AIActionType::MELEE_ATTACK;
        action.toHex = activeStack.positionHex;
        action.reason = "Immediate melee attack on adjacent target: " + target->name;
        return action;
    }

    // 2. Проверка: можем ли дойти и ударить на этом ходу?
    int attackHex = Pathfinder::getBestMeleeAttackHex(
        activeStack.positionHex,
        target->positionHex,
        activeStack.getEffectiveSpeed(),
        activeStack.isFlying,
        dynamicObstacles
    );

    if (attackHex != -1 && attackHex != activeStack.positionHex)
    {
        action.type = AIActionType::MOVE_AND_ATTACK;
        action.toHex = attackHex;
        action.reason = "Moving to adjacent hex " + std::to_string(attackHex) + " and attacking " + target->name;
        return action;
    }

    // 3. Цель дальше одного хода -> идем вперед максимальным шагом
    int stepHex = Pathfinder::getStepTowards(
        activeStack.positionHex,
        target->positionHex,
        activeStack.getEffectiveSpeed(),
        activeStack.isFlying,
        dynamicObstacles
    );

    if (stepHex != activeStack.positionHex)
    {
        action.type = AIActionType::MOVE;
        action.toHex = stepHex;
        action.reason = "Advancing " + std::to_string(activeStack.getEffectiveSpeed()) + " hexes towards " + target->name;
        return action;
    }

    // Если даже шаг сделать нельзя (зажат препятствиями)
    action.type = AIActionType::DEFEND;
    action.reason = "Movement blocked, defending";
    return action;
}

} // namespace BattleAI
