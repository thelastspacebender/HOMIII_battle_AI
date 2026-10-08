#include <iostream>
#include <cassert>
#include <vector>
#include "EnemyBattleAI.h"

using namespace BattleAI;

void testZombieBaitAttraction()
{
    std::cout << "[TEST] Running testZombieBaitAttraction..." << std::endl;
    // Setup:
    // Archers at (1, 1) [hex 18] -> 15 units (CORE)
    // Pikeman at (9, 5) [hex 94] -> 1 unit (BAIT)
    // Zombie at (13, 5) [hex 98] -> 35 units (ENEMY)
    // Distance Zombie to Pikeman is 4 hexes.
    // Distance Zombie to Archers is 12 hexes.
    CombatStack archers = CombatStack::createArchers(1, 15, BattleHex(1, 1).id);
    CombatStack pikeman = CombatStack::createPikemanBait(2, BattleHex(9, 5).id);
    CombatStack zombie = CombatStack::createWalkingDead(3, 35, BattleHex(13, 5).id);

    std::vector<CombatStack> stacks = {archers, pikeman, zombie};
    Pathfinder::ObstacleMask obstacles;

    // Plan Zombie action
    AIAction action = EnemyBattleAI::planAction(zombie, stacks, obstacles);

    std::cout << "  Zombie target: " << action.targetStackId 
              << ", action type: " << static_cast<int>(action.type)
              << ", reason: " << action.reason << std::endl;

    // The Zombie must target the closer Pikeman!
    assert(action.targetStackId == pikeman.id);
    assert(action.type == AIActionType::MOVE || action.type == AIActionType::MOVE_AND_ATTACK);
    assert(action.toHex != zombie.positionHex);

    int distToPikemanBefore = HexGrid::distance(zombie.positionHex, pikeman.positionHex);
    int distToPikemanAfter = HexGrid::distance(action.toHex, pikeman.positionHex);
    assert(distToPikemanAfter < distToPikemanBefore);

    std::cout << "  -> Passed: Zombie is successfully baited by Pikeman instead of rushing Archers!" << std::endl;
}

void testZombieMeleeStrikeWhenInReach()
{
    std::cout << "[TEST] Running testZombieMeleeStrikeWhenInReach..." << std::endl;
    // Setup: Zombie next to Pikeman
    BattleHex pikemanHex(5, 5);
    BattleHex zombieHex = HexGrid::getNeighbor(pikemanHex, HexDirection::RIGHT);

    CombatStack archers = CombatStack::createArchers(1, 15, BattleHex(1, 1).id);
    CombatStack pikeman = CombatStack::createPikemanBait(2, pikemanHex.id);
    CombatStack zombie = CombatStack::createWalkingDead(3, 35, zombieHex.id);

    std::vector<CombatStack> stacks = {archers, pikeman, zombie};
    Pathfinder::ObstacleMask obstacles;

    AIAction action = EnemyBattleAI::planAction(zombie, stacks, obstacles);

    std::cout << "  Zombie action when adjacent: " << action.reason << std::endl;
    assert(action.type == AIActionType::MELEE_ATTACK);
    assert(action.targetStackId == pikeman.id);

    std::cout << "  -> Passed: Zombie executes melee attack immediately when in contact." << std::endl;
}

int main()
{
    std::cout << "=== HoMM3 / VCMI Enemy Battle AI Unit Tests ===" << std::endl;
    testZombieBaitAttraction();
    testZombieMeleeStrikeWhenInReach();
    std::cout << "=== ALL ENEMY AI TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    return 0;
}
