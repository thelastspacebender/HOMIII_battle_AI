#include "TacticalSearch.h"
#include <algorithm>
#include <limits>
#include <iostream>

namespace BattleAI
{

std::vector<MoveEvaluation> TacticalSearch::generateCandidates(
    const CombatStack& activeStack,
    const std::vector<CombatStack>& allStacks,
    const Pathfinder::ObstacleMask& staticObstacles)
{
    std::vector<MoveEvaluation> candidates;
    candidates.reserve(128);

    Pathfinder::ObstacleMask dynamicObstacles = staticObstacles;
    for (const auto& s : allStacks)
    {
        if (s.isAlive && s.id != activeStack.id && s.positionHex >= 0)
        {
            dynamicObstacles.set(s.positionHex);
        }
    }

    // 1. Выстрелы (Ranged Attacks)
    bool canShootWithoutPenalty = (activeStack.isShooter && activeStack.shotsLeft > 0 && 
                                   !DamageCalculator::isShooterBlocked(activeStack, allStacks));
    if (canShootWithoutPenalty)
    {
        for (const auto& target : allStacks)
        {
            if (!target.isAlive || target.side == activeStack.side) continue;

            MoveEvaluation eval;
            eval.kind = MoveEvaluation::ActionKind::RANGED_ATTACK;
            eval.sourceHex = activeStack.positionHex;
            eval.targetHex = target.positionHex;
            eval.targetStackId = target.id;
            eval.targetRole = target.role;

            evaluateFeatures(eval, activeStack, allStacks, staticObstacles);
            candidates.push_back(eval);
        }
    }

    // 2. Достижимые клетки перемещения (для стрелков - только если заблокированы или кончились стрелы)
    if (!canShootWithoutPenalty || activeStack.role != StackRole::CORE)
    {
        auto reachable = Pathfinder::getReachableHexes(
            activeStack.positionHex,
            activeStack.getEffectiveSpeed(),
            activeStack.isFlying,
            dynamicObstacles
        );

        for (int dstHex : reachable)
        {
            // 2a. Простое перемещение на гекс
            if (dstHex != activeStack.positionHex)
            {
                MoveEvaluation moveEval;
                moveEval.kind = MoveEvaluation::ActionKind::MOVE;
                moveEval.sourceHex = activeStack.positionHex;
                moveEval.targetHex = dstHex;

                evaluateFeatures(moveEval, activeStack, allStacks, staticObstacles);
                candidates.push_back(moveEval);
            }

            // 2b. Рукопашная атака с этого гекса по соседним врагам
            auto neighbors = HexGrid::getValidNeighbors(BattleHex(dstHex), true);
            for (const auto& n : neighbors)
            {
                for (const auto& target : allStacks)
                {
                    if (!target.isAlive || target.side == activeStack.side) continue;
                    if (target.positionHex != n.id) continue;

                    MoveEvaluation meleeEval;
                    meleeEval.kind = MoveEvaluation::ActionKind::MELEE_ATTACK;
                    meleeEval.sourceHex = dstHex;
                    meleeEval.targetHex = target.positionHex;
                    meleeEval.targetStackId = target.id;
                    meleeEval.targetRole = target.role;

                    evaluateFeatures(meleeEval, activeStack, allStacks, staticObstacles);
                    candidates.push_back(meleeEval);
                }
            }
        }
    }

    // 3. Выжидание (Wait)
    if (!activeStack.hasWaited)
    {
        MoveEvaluation waitEval;
        waitEval.kind = MoveEvaluation::ActionKind::WAIT;
        waitEval.sourceHex = activeStack.positionHex;
        waitEval.targetHex = activeStack.positionHex;
        waitEval.preservesTempo = true;
        evaluateFeatures(waitEval, activeStack, allStacks, staticObstacles);
        candidates.push_back(waitEval);
    }

    // 4. Защита (Defend)
    MoveEvaluation defEval;
    defEval.kind = MoveEvaluation::ActionKind::DEFEND;
    defEval.sourceHex = activeStack.positionHex;
    defEval.targetHex = activeStack.positionHex;
    evaluateFeatures(defEval, activeStack, allStacks, staticObstacles);
    candidates.push_back(defEval);

    return candidates;
}

void TacticalSearch::evaluateFeatures(
    MoveEvaluation& eval,
    const CombatStack& activeStack,
    const std::vector<CombatStack>& allStacks,
    const Pathfinder::ObstacleMask& staticObstacles)
{
    (void)staticObstacles;

    // Поиск ключевых отрядов на поле боя
    const CombatStack* allyCore = nullptr;
    const CombatStack* nearestEnemy = nullptr;
    int minEnemyDist = 999;

    for (const auto& s : allStacks)
    {
        if (!s.isAlive) continue;
        if (s.side == activeStack.side && s.role == StackRole::CORE)
        {
            allyCore = &s;
        }
        else if (s.side != activeStack.side)
        {
            int d = HexGrid::distance(activeStack.positionHex, s.positionHex);
            if (d < minEnemyDist)
            {
                minEnemyDist = d;
                nearestEnemy = &s;
            }
        }
    }

    // Оценка урона для атак
    if (eval.kind == MoveEvaluation::ActionKind::RANGED_ATTACK ||
        eval.kind == MoveEvaluation::ActionKind::MELEE_ATTACK)
    {
        const CombatStack* target = nullptr;
        for (const auto& s : allStacks)
        {
            if (s.id == eval.targetStackId) { target = &s; break; }
        }

        if (target)
        {
            bool isRanged = (eval.kind == MoveEvaluation::ActionKind::RANGED_ATTACK);
            eval.expectedDmgDealt = static_cast<float>(DamageCalculator::estimateDamage(activeStack, *target, isRanged));
            eval.killRatio = std::min(1.0f, eval.expectedDmgDealt / std::max(1, target->getTotalHp()));

            if (isRanged)
            {
                // Если стрелок собирается выстрелить, но враг в непосредственной близости:
                if (nearestEnemy)
                {
                    int distToEnemy = HexGrid::distance(activeStack.positionHex, nearestEnemy->positionHex);
                    // Если враг может дойти и ударить на этом/следующем ходу:
                    if (distToEnemy <= nearestEnemy->getEffectiveSpeed())
                    {
                        eval.expectedDmgReceived = static_cast<float>(DamageCalculator::estimateDamage(*nearestEnemy, activeStack, false));
                    }
                }
            }
            else
            {
                // Если рукопашная - проверяем ответный удар
                if (target->hasRetaliation && !activeStack.noRetaliationAttack)
                {
                    int remainingHp = target->getTotalHp() - static_cast<int>(eval.expectedDmgDealt);
                    if (remainingHp > 0)
                    {
                        eval.expectedDmgReceived = static_cast<float>(DamageCalculator::estimateDamage(*target, activeStack, false));
                    }
                }

                // Срыв ответки единичкой (Пикинером)
                if (target->hasRetaliation && activeStack.role == StackRole::BAIT)
                {
                    eval.stripsRetaliation = true;
                }
            }
        }
    }

    // Оценка пространственных перемещений
    if (eval.kind == MoveEvaluation::ActionKind::MOVE)
    {
        int dst = eval.targetHex;

        // 1. Кайтинг (для стрелка/ядра): увеличение дистанции от ближайшего врага
        if (activeStack.role == StackRole::CORE && nearestEnemy)
        {
            int currentDist = HexGrid::distance(activeStack.positionHex, nearestEnemy->positionHex);
            int newDist = HexGrid::distance(dst, nearestEnemy->positionHex);
            if (newDist > currentDist)
            {
                eval.kitesEnemy = true;
                eval.baitScore = static_cast<float>(newDist - currentDist) * 10.0f;
            }
        }

        // 2. Байт / Отвод (для пикинера)
        if (activeStack.role == StackRole::BAIT && nearestEnemy && allyCore)
        {
            int distToEnemy = HexGrid::distance(dst, nearestEnemy->positionHex);
            int coreDistToEnemy = HexGrid::distance(allyCore->positionHex, nearestEnemy->positionHex);
            int distToCore = HexGrid::distance(dst, allyCore->positionHex);
            int enemySpeed = nearestEnemy->getEffectiveSpeed(); // 3 для зомби

            // Свобода маневра на целевом гексе (проверка на тупики у стен поля 15x11)
            int freeNeighbors = 0;
            auto nbs = HexGrid::getValidNeighbors(BattleHex(dst), false);
            for (const auto& nb : nbs)
            {
                if (!staticObstacles.test(nb.id)) freeNeighbors++;
            }

            // Враг может ударить со смежного гекса, значит зона поражения врага: enemySpeed + 1 (4 гекса)!
            bool inEnemyAttackRange = (distToEnemy <= enemySpeed + 1);

            if (inEnemyAttackRange)
            {
                // ОПАСНАЯ ЗОНА: Враг может дойти и ударить на этом ходу!
                // Это допустимо ТОЛЬКО как ЖЕРТВЕННЫЙ БАЙТ (когда стрелок в опасности)!
                if (coreDistToEnemy <= 5 && distToCore >= coreDistToEnemy)
                {
                    // Зомби близко к стрелку (<= 5)! Пикинер жертвует собой, уводя зомби В СТОРОНУ/ОТ стрелка!
                    eval.baitsEnemy = true;
                    eval.baitScore = 80.0f + static_cast<float>(distToCore) * 5.0f;
                    eval.expectedDmgReceived = 5.0f; // Приемлемая жертва ради спасения стрелка
                }
                else
                {
                    // Бессмысленная гибель единички — строжайший штраф!
                    eval.expectedDmgReceived = 100.0f;
                }
            }
            else
            {
                // БЕЗОПАСНАЯ ЗОНА (Кайтинг без потерь):
                eval.baitsEnemy = true;

                // Байт эффективен, если пикинер ближе к зомби, чем стрелок (чтобы агро было на пикинере)
                float aggroAdvantage = static_cast<float>(coreDistToEnemy - distToEnemy);

                // Идеальная дистанция удержания на поводке:
                // distToEnemy == enemySpeed + 2 (дистанция 5 для зомби):
                // Зомби делает 3 шага, оказывается на дистанции 2, НЕ МОЖЕТ ударить!
                float leashScore = 0.0f;
                if (distToEnemy == enemySpeed + 2) // Дистанция 5
                {
                    leashScore = 60.0f;
                }
                else if (distToEnemy == enemySpeed + 3) // Дистанция 6
                {
                    leashScore = 40.0f;
                }
                else if (distToEnemy == enemySpeed + 4) // Дистанция 7
                {
                    leashScore = 20.0f;
                }

                // Бонус за открытое пространство (избегаем углов поля и тупиков)
                float maneuverBonus = (freeNeighbors >= 4) ? 15.0f : ((freeNeighbors <= 2) ? -35.0f : 0.0f);

                // Держимся на дистанции от стрелка (не приводим врага к стрелку)
                float coreSeparation = (distToCore >= 4) ? 20.0f : -40.0f;

                // ЗАПРЕТНАЯ ЗОНА СТРЕЛКОВ: Лучники в (0, 1).
                // Пикинеры должны уводить зомби строго в нижнюю и центральную часть (Y >= 4)!
                BattleHex targetCoord(dst);
                float sanctuaryPenalty = (targetCoord.y <= 3) ? -200.0f : 30.0f;

                // ШТРАФ ЗА ПРИЖИМАНИЕ К СТЕНАМ (X == 0, X == 14, Y >= 10):
                // Пикинер обязан кружить в открытом пространстве (X: 2..12, Y: 4..9), огибая (7, 7)!
                float borderPenalty = 0.0f;
                if (targetCoord.x == 0 || targetCoord.x >= 14 || targetCoord.y >= 10)
                {
                    borderPenalty = -100.0f;
                }
                else if (targetCoord.x >= 2 && targetCoord.x <= 11 && targetCoord.y >= 4 && targetCoord.y <= 9)
                {
                    borderPenalty = 40.0f;
                }

                eval.baitScore = (aggroAdvantage * 4.0f) + leashScore + maneuverBonus + coreSeparation + sanctuaryPenalty + borderPenalty;
            }
        }
    }

    // 3. Оценка выжидания (Wait) для пикинера
    if (eval.kind == MoveEvaluation::ActionKind::WAIT)
    {
        if (activeStack.role == StackRole::BAIT && nearestEnemy)
        {
            int currentDist = HexGrid::distance(activeStack.positionHex, nearestEnemy->positionHex);
            if (currentDist >= nearestEnemy->getEffectiveSpeed() + 2)
            {
                // Если мы уже на безопасной дистанции (>= 5), выжидание позволяет пропустить ход зомби вперед!
                eval.baitScore = 45.0f;
            }
        }
    }
}

MoveEvaluation TacticalSearch::findBestAction(
    const CombatStack& activeStack,
    const std::vector<CombatStack>& allStacks,
    const Pathfinder::ObstacleMask& staticObstacles,
    const DynamicWeights& weights)
{
    auto candidates = generateCandidates(activeStack, allStacks, staticObstacles);
    if (candidates.empty())
    {
        MoveEvaluation def;
        def.kind = MoveEvaluation::ActionKind::DEFEND;
        def.sourceHex = activeStack.positionHex;
        def.targetHex = activeStack.positionHex;
        return def;
    }

    float bestUtility = -1e9f;
    size_t bestIdx = 0;

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        float u = candidates[i].computeUtility(weights, activeStack.role);
        if (u > bestUtility)
        {
            bestUtility = u;
            bestIdx = i;
        }
    }

    return candidates[bestIdx];
}

} // namespace BattleAI
