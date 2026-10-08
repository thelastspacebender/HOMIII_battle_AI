#pragma once

#include "HexGrid.h"
#include "CombatStack.h"
#include "TurnQueue.h"
#include "Pathfinder.h"
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace BattleAI
{

// Константы тензора поля боя: 11 рядов x 15 колонок = 165 гексов
constexpr size_t BFIELD_ROWS = 11;
constexpr size_t BFIELD_COLS = 15;
constexpr size_t TOTAL_HEXES = BFIELD_ROWS * BFIELD_COLS; // 165

constexpr size_t NUM_HEX_CHANNELS = 28;
constexpr size_t NUM_GLOBAL_CHANNELS = 4;
constexpr size_t TENSOR_FLAT_SIZE = (TOTAL_HEXES * NUM_HEX_CHANNELS) + NUM_GLOBAL_CHANNELS; // 4624

/**
 * @brief Полный список 28 каналов пространственных признаков на каждый гекс.
 */
enum HexChannel : size_t
{
    CH_OBSTACLE = 0,         ///< 1.0 = камень/дерево/забор, 0.0 = чисто
    CH_HAZARD = 1,           ///< 0.5 = ров, 1.0 = огненная стена/мина
    CH_CORPSE_HP = 2,        ///< HP погибшего отряда / 100.0 (цель для поднятия мертвецов)
    CH_GATE_WALL = 3,        ///< 1.0 = закрытые ворота / стена
    CH_UNIT_SIDE = 4,        ///< +1.0 = союзник, -1.0 = враг, 0.0 = пусто
    CH_TWO_HEX = 5,          ///< 1.0 = двухгексовый юнит
    CH_IS_ACTIVE = 6,        ///< 1.0 на гексе отряда, который ходит прямо сейчас
    CH_QUEUE_PRIORITY = 7,   ///< +1.0 = союзник ходит следующим, -1.0 = враг
    CH_COUNT_NORM = 8,       ///< count / 50.0
    CH_TOTAL_HP_NORM = 9,    ///< totalHp / maxTotalHp
    CH_TOP_UNIT_HP = 10,     ///< currentHpTopUnit / hpPerUnit
    CH_ATTACK = 11,          ///< attack / 40.0
    CH_DEFENSE = 12,         ///< defense / 40.0
    CH_SPEED = 13,           ///< speed / 20.0
    CH_DMG_MIN = 14,         ///< minDmg / 50.0
    CH_DMG_MAX = 15,         ///< maxDmg / 50.0
    CH_IS_FLYING = 16,       ///< 1.0 если летающий
    CH_IS_SHOOTER = 17,      ///< 1.0 если стрелок
    CH_SHOTS_LEFT = 18,      ///< shots / 24.0
    CH_CAN_RETALIATE = 19,   ///< 1.0 если может ответить на удар
    CH_INF_RETALIATE = 20,   ///< 1.0 если бесконечная ответка
    CH_CAN_CAST = 21,        ///< 1.0 если кастер
    CH_NO_MELEE_PEN = 22,    ///< 1.0 если стрелок без штрафа в рукопашной
    CH_NO_RETAL_ATK = 23,    ///< 1.0 если бьет без ответа
    CH_MORALE = 24,          ///< morale / 3.0 [-1.0 .. +1.0]
    CH_LUCK = 25,            ///< luck / 3.0 [0.0 .. 1.0]
    CH_IS_DISABLED = 26,     ///< 1.0 если слепота/паралич
    CH_SPEED_BUFF = 27       ///< -1.0 Slow, 0 Normal, +1.0 Haste
};

/**
 * @brief 4 глобальных канала контекста боя.
 */
enum GlobalChannel : size_t
{
    GL_IS_BATTLE_AI = 0,     ///< 1.0 против жадного BattleAI, 0.0 против Человека
    GL_ARMY_RATIO = 1,       ///< Ally army value / Enemy army value
    GL_ROUND_NUM = 2,        ///< Номер раунда боя / 30.0
    GL_CORE_THREAT = 3       ///< Дистанция от ближайшего вражеского милишника до союзного ядра / 17.0
};

/**
 * @brief Полный тензор поля боя (5240 значений).
 */
struct BattlefieldTensor
{
    std::array<std::array<float, NUM_HEX_CHANNELS>, TOTAL_HEXES> hexGrid{};
    std::array<float, NUM_GLOBAL_CHANNELS> globalContext{};

    BattlefieldTensor()
    {
        clear();
    }

    void clear()
    {
        for (auto& hex : hexGrid)
        {
            hex.fill(0.0f);
        }
        globalContext.fill(0.0f);
    }

    /**
     * @brief Извлечение полного 28-канального тензора из текущего состояния игрового движка
     * @param allStacks Список всех отрядов
     * @param activeStackId ID активного отряда
     * @param staticObstacles Битовая маска препятствий
     * @param queue Менеджер очереди ходов
     * @param againstBattleAI Режим: true = игра против BattleAI, false = против Человека
     */
    void extractFromState(
        const std::vector<CombatStack>& allStacks,
        int activeStackId,
        const Pathfinder::ObstacleMask& staticObstacles,
        const TurnQueue& queue,
        bool againstBattleAI = true
    )
    {
        clear();

        // 1. Статические препятствия
        for (size_t h = 0; h < TOTAL_HEXES; ++h)
        {
            if (staticObstacles.test(h))
            {
                hexGrid[h][CH_OBSTACLE] = 1.0f;
            }
        }

        // 2. Отряды на поле боя
        float allyValue = 0.0f;
        float enemyValue = 0.0f;
        const CombatStack* coreStack = nullptr;
        std::vector<int> enemyMeleeHexes;

        for (const auto& s : allStacks)
        {
            if (!s.isAlive || s.positionHex < 0 || s.positionHex >= static_cast<int>(TOTAL_HEXES))
            {
                // Если погиб, фиксируем труп
                if (!s.isAlive && s.positionHex >= 0 && s.positionHex < static_cast<int>(TOTAL_HEXES))
                {
                    hexGrid[s.positionHex][CH_CORPSE_HP] = std::min(1.0f, (s.initialCount * s.hpPerUnit) / 200.0f);
                }
                continue;
            }

            int h = s.positionHex;

            // Расчет ценности армии
            float val = s.count * (s.role == StackRole::CORE ? 150.0f : 80.0f);
            if (s.side == CombatSide::ALLY)
            {
                allyValue += val;
                if (s.role == StackRole::CORE) coreStack = &s;
            }
            else
            {
                enemyValue += val;
                if (!s.isShooter) enemyMeleeHexes.push_back(h);
            }

            // Заполнение каналов гекса
            hexGrid[h][CH_UNIT_SIDE]       = (s.side == CombatSide::ALLY) ? 1.0f : -1.0f;
            hexGrid[h][CH_TWO_HEX]         = s.isTwoHex ? 1.0f : 0.0f;
            hexGrid[h][CH_IS_ACTIVE]       = (s.id == activeStackId) ? 1.0f : 0.0f;
            hexGrid[h][CH_COUNT_NORM]      = std::min(1.0f, s.count / 50.0f);
            hexGrid[h][CH_TOTAL_HP_NORM]   = s.getHealthRatio();
            hexGrid[h][CH_TOP_UNIT_HP]     = std::min(1.0f, static_cast<float>(s.currentHpTopUnit) / s.hpPerUnit);
            hexGrid[h][CH_ATTACK]          = std::min(1.0f, s.attack / 40.0f);
            hexGrid[h][CH_DEFENSE]         = std::min(1.0f, s.defense / 40.0f);
            hexGrid[h][CH_SPEED]           = std::min(1.0f, s.getEffectiveSpeed() / 20.0f);
            hexGrid[h][CH_DMG_MIN]         = std::min(1.0f, s.minDamage / 50.0f);
            hexGrid[h][CH_DMG_MAX]         = std::min(1.0f, s.maxDamage / 50.0f);
            hexGrid[h][CH_IS_FLYING]       = s.isFlying ? 1.0f : 0.0f;
            hexGrid[h][CH_IS_SHOOTER]      = s.isShooter ? 1.0f : 0.0f;
            hexGrid[h][CH_SHOTS_LEFT]      = std::min(1.0f, s.shotsLeft / 24.0f);
            hexGrid[h][CH_CAN_RETALIATE]   = s.hasRetaliation ? 1.0f : 0.0f;
            hexGrid[h][CH_INF_RETALIATE]   = s.infiniteRetaliation ? 1.0f : 0.0f;
            hexGrid[h][CH_CAN_CAST]        = s.canCast ? 1.0f : 0.0f;
            hexGrid[h][CH_NO_MELEE_PEN]    = s.noMeleePenalty ? 1.0f : 0.0f;
            hexGrid[h][CH_NO_RETAL_ATK]    = s.noRetaliationAttack ? 1.0f : 0.0f;
            hexGrid[h][CH_MORALE]          = std::clamp(s.morale / 3.0f, -1.0f, 1.0f);
            hexGrid[h][CH_LUCK]            = std::clamp(s.luck / 3.0f, 0.0f, 1.0f);
            hexGrid[h][CH_IS_DISABLED]     = s.isDisabled ? 1.0f : 0.0f;
            hexGrid[h][CH_SPEED_BUFF]      = static_cast<float>(s.speedBuff);
        }

        // 3. Канал очереди инициативы (CH_QUEUE_PRIORITY)
        auto queueOrder = TurnQueue::buildQueue(allStacks);
        for (size_t rank = 0; rank < queueOrder.size(); ++rank)
        {
            int stackId = queueOrder[rank];
            for (const auto& s : allStacks)
            {
                if (s.id == stackId && s.isAlive && s.positionHex >= 0)
                {
                    float priority = 1.0f - (static_cast<float>(rank) / std::max(1.0f, static_cast<float>(queueOrder.size())));
                    if (s.side == CombatSide::ENEMY) priority = -priority;
                    hexGrid[s.positionHex][CH_QUEUE_PRIORITY] = priority;
                    break;
                }
            }
        }

        // 4. Глобальные каналы контекста
        globalContext[GL_IS_BATTLE_AI] = againstBattleAI ? 1.0f : 0.0f;
        globalContext[GL_ARMY_RATIO]   = (enemyValue > 0.0f) ? std::min(5.0f, allyValue / enemyValue) : 5.0f;
        globalContext[GL_ROUND_NUM]    = std::min(1.0f, queue.currentRound / 30.0f);

        // Расчет дистанции угрозы для ядра (CORE THREAT)
        float minCoreDist = 17.0f;
        if (coreStack && !enemyMeleeHexes.empty())
        {
            for (int eHex : enemyMeleeHexes)
            {
                int d = HexGrid::distance(coreStack->positionHex, eHex);
                if (d < minCoreDist) minCoreDist = static_cast<float>(d);
            }
        }
        globalContext[GL_CORE_THREAT] = minCoreDist / 17.0f;
    }

    /**
     * @brief Развернуть тензор в плоский одномерный вектор размером 5240
     */
    [[nodiscard]] std::vector<float> flatten() const
    {
        std::vector<float> flat;
        flat.reserve(TENSOR_FLAT_SIZE);

        for (size_t h = 0; h < TOTAL_HEXES; ++h)
        {
            for (size_t c = 0; c < NUM_HEX_CHANNELS; ++c)
            {
                flat.push_back(hexGrid[h][c]);
            }
        }

        for (size_t g = 0; g < NUM_GLOBAL_CHANNELS; ++g)
        {
            flat.push_back(globalContext[g]);
        }

        return flat;
    }
};

} // namespace BattleAI
