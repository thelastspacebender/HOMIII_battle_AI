#pragma once

#include "Role.h"
#include <vector>
#include <cmath>
#include <iostream>

namespace BattleAI
{

/**
 * @brief Вектор динамических весов W(t), генерируемый Директором НА КАЖДЫЙ ХОД боя.
 * Значения этих весов меняются каждую секунду в зависимости от того,
 * кто сейчас на поле, кто ходит следующим и в какой фазе находится бой.
 */
struct DynamicWeights
{
    float w_strip_retaliation = 0.0f;     ///< Важность срыва ответки прямо сейчас
    float w_block_shooter = 0.0f;        ///< Важность блокировки стрелка
    float w_stand_on_corpse = 0.0f;      ///< Важность закрытия трупа (анти-воскрешение)
    float w_screen_core = 0.0f;          ///< Важность прикрытия главного отряда
    float w_preserve_tempo = 0.0f;       ///< Важность выжидания (Wait)
    float w_dmg_dealt = 1.0f;            ///< Множитель нанесенного урона
    float w_dmg_received_penalty = 1.0f; ///< Штраф за полученный урон
    float w_kite_distance = 0.0f;        ///< Важность разрыва дистанции со стрелком/медленным врагом
    float w_bait_diversion = 0.0f;       ///< Важность отвлечения вражеских стеков на единичку

    void print() const
    {
        std::cout << "  [Динамические веса хода W(t)]:\n"
                  << "    • Срыв ответки (StripRetaliation): " << w_strip_retaliation << "\n"
                  << "    • Блокировка стрелка (BlockShooter): " << w_block_shooter << "\n"
                  << "    • Закрытие трупа (CorpseBlock):     " << w_stand_on_corpse << "\n"
                  << "    • Прикрытие ядра (ScreenCore):       " << w_screen_core << "\n"
                  << "    • Кайтинг (KiteDistance):           " << w_kite_distance << "\n"
                  << "    • Байт/Отвлечение (BaitDiversion):  " << w_bait_diversion << "\n"
                  << "    • Штраф за потери (DamagePenalty):   " << w_dmg_received_penalty << "\n";
    }
};

/**
 * @brief Оценка любого возможного хода.
 */
struct MoveEvaluation
{
    enum class ActionKind : uint8_t
    {
        MOVE = 0,
        MELEE_ATTACK = 1,
        RANGED_ATTACK = 2,
        WAIT = 3,
        DEFEND = 4,
        SPELLCAST = 5
    };

    ActionKind kind = ActionKind::MOVE;
    int sourceHex = -1;
    int targetHex = -1;
    int targetStackId = -1;

    // Признаки хода (Features)
    bool stripsRetaliation = false;
    bool blocksEnemyShooter = false;
    bool standsOnCorpse = false;
    bool screensAllyCore = false;
    bool preservesTempo = false;
    bool kitesEnemy = false;
    bool baitsEnemy = false;
    float baitScore = 0.0f;
    
    float expectedDmgDealt = 0.0f;
    float expectedDmgReceived = 0.0f;
    float killRatio = 0.0f;

    StackRole targetRole = StackRole::UNKNOWN;

    /**
     * @brief Оценка полезности хода: признаки хода умножаются на ДИНАМИЧЕСКИЕ веса W(t),
     * которые машина посчитала под текущую секунду боя.
     */
    float computeUtility(const DynamicWeights& w, StackRole myRole) const
    {
        float score = 0.0f;

        // 1. Размен уроном по динамическим коэффициентам
        score += (expectedDmgDealt * w.w_dmg_dealt);
        score -= (expectedDmgReceived * w.w_dmg_received_penalty);

        // 2. Динамический вес срыва ответки
        if (stripsRetaliation)
        {
            score += w.w_strip_retaliation;
        }

        // 3. Динамический вес блокировки стрелка
        if (blocksEnemyShooter)
        {
            score += w.w_block_shooter;
        }

        // 4. Динамический вес закрытия трупа
        if (standsOnCorpse)
        {
            score += w.w_stand_on_corpse;
        }

        // 5. Динамический вес прикрытия союзного ядра
        if (screensAllyCore)
        {
            // Для приманок (BAIT) толпиться возле лучника — смертельная ошибка!
            if (myRole == StackRole::BAIT)
            {
                score -= w.w_screen_core * 3.0f; // Жесткий штраф за приведение врага к стрелку
            }
            else
            {
                score += w.w_screen_core;
            }
        }

        // 6. Динамический вес темпа
        if (preservesTempo)
        {
            score += w.w_preserve_tempo;
        }

        // 7. Динамический вес кайтинга
        if (kitesEnemy)
        {
            score += w.w_kite_distance * (1.0f + baitScore);
        }

        // 8. Динамический вес байта / увода врага
        if (baitsEnemy)
        {
            score += w.w_bait_diversion * (1.0f + baitScore);
        }

        return score;
    }
};

} // namespace BattleAI
