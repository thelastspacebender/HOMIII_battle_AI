#pragma once

#include "HexGrid.h"
#include "Role.h"
#include <string>
#include <algorithm>
#include <cmath>

namespace BattleAI
{

/**
 * @file CombatStack.h
 * @brief Структура боевого отряда HoMM3 / VCMI со всеми 28 характеристиками.
 */

enum class CombatSide : int8_t
{
    ALLY = 1,
    ENEMY = -1
};

struct CombatStack
{
    // Идентификация
    int id{0};
    CombatSide side{CombatSide::ALLY};
    std::string typeId{"core:archer"};
    std::string name{"Archers"};
    StackRole role{StackRole::CORE};

    // Численность и здоровье
    int count{1};
    int initialCount{1};
    int hpPerUnit{10};
    int currentHpTopUnit{10}; // HP верхнего юнита (от 1 до hpPerUnit)

    // Боевые характеристики (HoMM3 стандарт)
    int attack{6};
    int defense{3};
    int speed{4};
    int minDamage{2};
    int maxDamage{3};
    
    // Стрельба
    bool isShooter{true};
    int shotsLeft{12};
    int maxShots{12};
    bool noMeleePenalty{false}; // Стрелок без штрафа в рукопашной

    // Способности и модификаторы
    bool isFlying{false};
    bool isTwoHex{false};
    bool hasRetaliation{true};     // Ответный удар на текущий раунд
    bool infiniteRetaliation{false}; // Бесконечная ответка (Грифоны)
    bool noRetaliationAttack{false}; // Атака без ответа врага (Церберы, Вампиры)
    bool canCast{false};            // Заклинатель
    int morale{0};                  // [-3 .. +3]
    int luck{0};                    // [0 .. 3]
    bool isDisabled{false};         // Слепота / Паралич
    int speedBuff{0};               // -1 Slow, 0 Normal, +1 Haste

    // Позиционирование и раунд
    int positionHex{-1};
    bool isAlive{true};
    bool hasWaited{false};
    bool hasActed{false};

    // Вычисляемые свойства
    [[nodiscard]] int getTotalHp() const
    {
        if (count <= 0 || !isAlive) return 0;
        return (count - 1) * hpPerUnit + currentHpTopUnit;
    }

    [[nodiscard]] int getMaxTotalHp() const
    {
        return initialCount * hpPerUnit;
    }

    [[nodiscard]] float getHealthRatio() const
    {
        int maxHp = getMaxTotalHp();
        return maxHp > 0 ? static_cast<float>(getTotalHp()) / static_cast<float>(maxHp) : 0.0f;
    }

    [[nodiscard]] int getEffectiveSpeed() const
    {
        int s = speed;
        if (speedBuff > 0) s += 2; // Haste
        if (speedBuff < 0) s = std::max(1, s - 2); // Slow
        return s;
    }

    /**
     * @brief Нанесение урона отряду и пересчет оставшихся юнитов
     * @param damage Наносимый урон
     * @return Количество погибших юнитов от этого удара
     */
    int applyDamage(int damage)
    {
        if (damage <= 0 || !isAlive || count <= 0) return 0;

        int killed = 0;
        if (damage < currentHpTopUnit)
        {
            currentHpTopUnit -= damage;
            return 0;
        }

        // Верхний юнит погибает
        damage -= currentHpTopUnit;
        killed = 1;

        // Погибают следующие юниты
        int additionalKilled = damage / hpPerUnit;
        killed += additionalKilled;
        damage %= hpPerUnit;

        if (killed >= count)
        {
            killed = count;
            count = 0;
            currentHpTopUnit = 0;
            isAlive = false;
        }
        else
        {
            count -= killed;
            currentHpTopUnit = hpPerUnit - damage;
        }

        return killed;
    }

    /**
     * @brief Сброс состояния перед началом нового раунда
     */
    void resetRound()
    {
        hasWaited = false;
        hasActed = false;
        hasRetaliation = true;
    }

    // ==========================================
    // ФАБРИЧНЫЕ МЕТОДЫ ДЛЯ СЦЕНАРИЯ ПОГОСТА
    // ==========================================

    /**
     * @brief 15 Лучников (Замок, Т2) - Наше ядро (CORE), которое нужно беречь без потерь!
     */
    static CombatStack createArchers(int id, int count = 15, int hex = 15) // (x=0, y=1)
    {
        CombatStack s;
        s.id = id;
        s.side = CombatSide::ALLY;
        s.typeId = "core:archer";
        s.name = "Archers";
        s.role = StackRole::CORE;
        s.count = count;
        s.initialCount = count;
        s.hpPerUnit = 10;
        s.currentHpTopUnit = 10;
        s.attack = 6;
        s.defense = 3;
        s.speed = 4;
        s.minDamage = 2;
        s.maxDamage = 3;
        s.isShooter = true;
        s.shotsLeft = 24; // Тележка с боеприпасами (HoMM3 стандарт для зачистки Погостов)
        s.maxShots = 24;
        s.noMeleePenalty = false;
        s.isFlying = false;
        s.isTwoHex = false;
        s.positionHex = hex;
        s.isAlive = true;
        return s;
    }

    /**
     * @brief 1 Пикинер (Замок, Т1) - Разменная единичка для кайта/байта (BAIT)
     */
    static CombatStack createPikemanBait(int id, int hex)
    {
        CombatStack s;
        s.id = id;
        s.side = CombatSide::ALLY;
        s.typeId = "core:pikeman";
        s.name = "Pikeman";
        s.role = StackRole::BAIT;
        s.count = 1;
        s.initialCount = 1;
        s.hpPerUnit = 10;
        s.currentHpTopUnit = 10;
        s.attack = 4;
        s.defense = 5;
        s.speed = 4;
        s.minDamage = 1;
        s.maxDamage = 3;
        s.isShooter = false;
        s.shotsLeft = 0;
        s.maxShots = 0;
        s.noMeleePenalty = false;
        s.isFlying = false;
        s.isTwoHex = false;
        s.positionHex = hex;
        s.isAlive = true;
        return s;
    }

    /**
     * @brief 30-35 Зомби / Ходячие мертвецы (Некрополис, Т2) - Охрана Погоста
     */
    static CombatStack createWalkingDead(int id, int count = 35, int hex = 89) // (x=14, y=5)
    {
        CombatStack s;
        s.id = id;
        s.side = CombatSide::ENEMY;
        s.typeId = "core:walkingDead";
        s.name = "Walking Dead";
        s.role = StackRole::UNKNOWN;
        s.count = count;
        s.initialCount = count;
        s.hpPerUnit = 15;
        s.currentHpTopUnit = 15;
        s.attack = 5;
        s.defense = 5;
        s.speed = 3; // Медленный милишник
        s.minDamage = 2;
        s.maxDamage = 3;
        s.isShooter = false;
        s.shotsLeft = 0;
        s.maxShots = 0;
        s.noMeleePenalty = false;
        s.isFlying = false;
        s.isTwoHex = false;
        s.positionHex = hex;
        s.isAlive = true;
        return s;
    }
};

} // namespace BattleAI
