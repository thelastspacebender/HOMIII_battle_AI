#pragma once

#include "CombatStack.h"
#include "HexGrid.h"
#include <vector>
#include <random>

namespace BattleAI
{

/**
 * @file DamageCalculator.h
 * @brief Полный калькулятор урона HoMM3 / VCMI:
 * - Формула атаки/защиты с коэффициентами +5% и -2.5%
 * - Разброс урона (мин/макс) и матожидание
 * - Прямой и дальний выстрел (штраф за расстояние > 10 гексов)
 * - Штраф стрелка в рукопашной (Melee penalty 50%)
 * - Механика ответного удара (Retaliation) с учетом гибели и способностей
 */

struct AttackOutcome
{
    bool isRanged{false};
    bool isBlocked{false};
    bool isDistancePenalty{false};
    bool isMeleePenalty{false};

    int damageDealt{0};
    int unitsKilled{0};

    bool retaliationOccurred{false};
    int retaliationDamage{0};
    int retaliationKilled{0};
};

class DamageCalculator
{
public:
    /**
     * @brief Проверка, заблокирован ли стрелок вражеским отрядом в соседнем гексе
     */
    static bool isShooterBlocked(const CombatStack& shooter, const std::vector<CombatStack>& allStacks)
    {
        if (!shooter.isShooter || !shooter.isAlive) return false;
        BattleHex shooterHex(shooter.positionHex);

        for (const auto& other : allStacks)
        {
            if (!other.isAlive || other.side == shooter.side) continue;
            if (HexGrid::isAdjacent(shooterHex, BattleHex(other.positionHex)))
            {
                return true; // Враг стоит в соседнем гексе!
            }
        }
        return false;
    }

    /**
     * @brief Расчет множителя урона от разницы атаки и защиты (HoMM3 стандарт)
     * Если Atk >= Def: +5% за каждую единицу разницы (макс +300% при разнице 60)
     * Если Atk < Def:  -2.5% за каждую единицу разницы (макс -70% при разнице 28)
     */
    static float getAttackDefenseMultiplier(int attack, int defense)
    {
        if (attack >= defense)
        {
            int diff = std::min(60, attack - defense);
            return 1.0f + 0.05f * static_cast<float>(diff);
        }
        else
        {
            int diff = std::min(28, defense - attack);
            return std::max(0.30f, 1.0f - 0.025f * static_cast<float>(diff));
        }
    }

    /**
     * @brief Расчет базового урона (детерминированное матожидание для поиска и оценки)
     */
    static float getExpectedBaseDamage(const CombatStack& attacker)
    {
        if (attacker.count <= 0 || !attacker.isAlive) return 0.0f;
        float avgDmgPerUnit = (attacker.minDamage + attacker.maxDamage) * 0.5f;
        return attacker.count * avgDmgPerUnit;
    }

    /**
     * @brief Расчет базового урона с генератором случайных чисел (для реальной симуляции)
     */
    template <typename Rng>
    static int getRandomBaseDamage(const CombatStack& attacker, Rng& rng)
    {
        if (attacker.count <= 0 || !attacker.isAlive) return 0;
        std::uniform_int_distribution<int> dist(attacker.minDamage, attacker.maxDamage);
        int total = 0;
        for (int i = 0; i < attacker.count; ++i)
        {
            total += dist(rng);
        }
        return total;
    }

    /**
     * @brief Расчет ожидаемого урона (без изменения состояния)
     */
    static int estimateDamage(
        const CombatStack& attacker,
        const CombatStack& defender,
        bool isRangedAttack
    )
    {
        if (attacker.count <= 0 || !attacker.isAlive || defender.count <= 0 || !defender.isAlive) return 0;

        float base = getExpectedBaseDamage(attacker);
        float mult = getAttackDefenseMultiplier(attacker.attack, defender.defense);

        if (isRangedAttack)
        {
            int dist = HexGrid::distance(attacker.positionHex, defender.positionHex);
            if (dist > 10)
            {
                mult *= 0.5f; // Штраф за расстояние
            }
        }
        else
        {
            if (attacker.isShooter && !attacker.noMeleePenalty)
            {
                mult *= 0.5f; // Штраф стрелка в ближнем бою
            }
        }

        return std::max(1, static_cast<int>(std::round(base * mult)));
    }

    /**
     * @brief Выполнение полноценной атаки (дальней или ближней) с нанесением урона и ответкой
     */
    template <typename Rng>
    static AttackOutcome executeAttack(
        CombatStack& attacker,
        CombatStack& defender,
        bool isRanged,
        Rng& rng
    )
    {
        AttackOutcome outcome;
        outcome.isRanged = isRanged;

        if (attacker.count <= 0 || !attacker.isAlive || defender.count <= 0 || !defender.isAlive)
        {
            return outcome;
        }

        float mult = getAttackDefenseMultiplier(attacker.attack, defender.defense);

        if (isRanged)
        {
            if (attacker.shotsLeft > 0)
            {
                attacker.shotsLeft--;
            }
            int dist = HexGrid::distance(attacker.positionHex, defender.positionHex);
            if (dist > 10)
            {
                outcome.isDistancePenalty = true;
                mult *= 0.5f;
            }
        }
        else
        {
            if (attacker.isShooter && !attacker.noMeleePenalty)
            {
                outcome.isMeleePenalty = true;
                mult *= 0.5f;
            }
        }

        // Расчет урона атакующего
        int baseDmg = getRandomBaseDamage(attacker, rng);
        outcome.damageDealt = std::max(1, static_cast<int>(std::round(baseDmg * mult)));
        outcome.unitsKilled = defender.applyDamage(outcome.damageDealt);

        // Проверка ответного удара
        // Ответка наносится ТОЛЬКО в ближнем бою (если цель выжила, имеет ответку и атакующий не отменяет ответку)
        if (!isRanged && defender.isAlive && defender.hasRetaliation && !attacker.noRetaliationAttack)
        {
            outcome.retaliationOccurred = true;

            float retalMult = getAttackDefenseMultiplier(defender.attack, attacker.defense);
            if (defender.isShooter && !defender.noMeleePenalty)
            {
                retalMult *= 0.5f;
            }

            int retalBaseDmg = getRandomBaseDamage(defender, rng);
            outcome.retaliationDamage = std::max(1, static_cast<int>(std::round(retalBaseDmg * retalMult)));
            outcome.retaliationKilled = attacker.applyDamage(outcome.retaliationDamage);

            if (!defender.infiniteRetaliation)
            {
                defender.hasRetaliation = false; // Снятие ответки!
            }
        }

        return outcome;
    }
};

} // namespace BattleAI
