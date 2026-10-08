#pragma once

#include "CombatStack.h"
#include <vector>
#include <algorithm>
#include <optional>

namespace BattleAI
{

/**
 * @file TurnQueue.h
 * @brief Менеджер очереди инициативы ходов HoMM3 / VCMI.
 * 
 * Правила:
 * 1. Инициатива определяется эффективной скоростью отряда (Speed).
 * 2. При равной скорости преимущество первого хода у Атакующего (Ally).
 * 3. Поддержка фазы ожидания (Wait): отряд откладывает ход до конца раунда.
 * 4. Автоматический переход к следующему раунду со сбросом ответных ударов.
 */

class TurnQueue
{
public:
    int currentRound{1};

    /**
     * @brief Получить список ID отрядов в порядке их очереди на текущий раунд.
     * @param stacks Список всех отрядов на поле боя
     * @return Вектор ID отрядов в порядке хода
     */
    static std::vector<int> buildQueue(const std::vector<CombatStack>& stacks)
    {
        std::vector<const CombatStack*> normalPhase;
        std::vector<const CombatStack*> waitPhase;

        for (const auto& s : stacks)
        {
            if (!s.isAlive || s.hasActed || s.isDisabled) continue;

            if (s.hasWaited)
            {
                waitPhase.push_back(&s);
            }
            else
            {
                normalPhase.push_back(&s);
            }
        }

        // Предикат сортировки: по скорости (убывание), при равенстве - Атакующий (ALLY) первым
        auto speedComparator = [](const CombatStack* a, const CombatStack* b)
        {
            int spdA = a->getEffectiveSpeed();
            int spdB = b->getEffectiveSpeed();
            if (spdA != spdB) return spdA > spdB;
            if (a->side != b->side) return a->side == CombatSide::ALLY;
            return a->id < b->id;
        };

        std::sort(normalPhase.begin(), normalPhase.end(), speedComparator);
        std::sort(waitPhase.begin(), waitPhase.end(), speedComparator);

        std::vector<int> result;
        result.reserve(normalPhase.size() + waitPhase.size());
        for (const auto* s : normalPhase) result.push_back(s->id);
        for (const auto* s : waitPhase) result.push_back(s->id);

        return result;
    }

    /**
     * @brief Найти следующий отряд, который должен совершить ход.
     * Если все отряды походили, переходит к следующему раунду.
     * @param stacks Список всех отрядов
     * @return ID активного отряда или -1, если бой завершен (одна из сторон погибла)
     */
    int getNextActiveStackId(std::vector<CombatStack>& stacks)
    {
        // Проверка завершения боя
        bool hasAliveAlly = false;
        bool hasAliveEnemy = false;
        for (const auto& s : stacks)
        {
            if (s.isAlive)
            {
                if (s.side == CombatSide::ALLY) hasAliveAlly = true;
                if (s.side == CombatSide::ENEMY) hasAliveEnemy = true;
            }
        }
        if (!hasAliveAlly || !hasAliveEnemy) return -1;

        auto queue = buildQueue(stacks);
        if (queue.empty())
        {
            // Все оставшиеся в живых отряды походили -> Начинаем новый раунд!
            advanceRound(stacks);
            queue = buildQueue(stacks);
            if (queue.empty()) return -1;
        }

        return queue.front();
    }

    /**
     * @brief Переход на новый раунд боя: сброс флагов ходов и ответных ударов
     */
    void advanceRound(std::vector<CombatStack>& stacks)
    {
        currentRound++;
        for (auto& s : stacks)
        {
            if (s.isAlive)
            {
                s.resetRound();
            }
        }
    }
};

} // namespace BattleAI
