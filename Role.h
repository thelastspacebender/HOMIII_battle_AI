#pragma once

#include <cstdint>
#include <string>

namespace BattleAI
{

/**
 * @brief Macro Roles assigned by Level 1 Director to every stack on the battlefield.
 */
enum class StackRole : uint8_t
{
    UNKNOWN = 0,
    CORE = 1,        ///< Primary win-condition (main damage dealer, high value). Must be protected at all costs.
    BODYGUARD = 2,   ///< Defensive tank / screener. Absorbs hits, zones melee threats away from CORE.
    BAIT = 3,        ///< Low-value expendable unit (peasant, skeleton, single unit stack). Strips retaliation, draws fire.
    INTERCEPTOR = 4  ///< Fast/flying disruptor. Ties down enemy shooters, cuts off flankers.
};

inline const char* roleToString(StackRole role)
{
    switch (role)
    {
        case StackRole::CORE: return "CORE";
        case StackRole::BODYGUARD: return "BODYGUARD";
        case StackRole::BAIT: return "BAIT";
        case StackRole::INTERCEPTOR: return "INTERCEPTOR";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Macro battle phase determined by relative army strength and casualty count.
 */
enum class BattlePhase : uint8_t
{
    CONTAINMENT = 0, ///< High danger, enemy full strength. Protect CORE, bait enemy into no-man's land.
    BREAKTHROUGH = 1,///< Enemy key threats softened. Unleash CORE, focus down primary targets.
    MOP_UP = 2       ///< Decisive advantage. Minimize incidental losses, hunt remaining enemies.
};

} // namespace BattleAI
