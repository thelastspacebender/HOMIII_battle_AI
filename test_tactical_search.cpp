#include <iostream>
#include <cassert>
#include <vector>
#include "TacticalSearch.h"

using namespace BattleAI;

void testArcherShootingSelection()
{
    std::cout << "[TEST] Running testArcherShootingSelection..." << std::endl;
    CombatStack archers = CombatStack::createArchers(1, 15, BattleHex(1, 1).id);
    CombatStack pikeman = CombatStack::createPikemanBait(2, BattleHex(3, 3).id);
    CombatStack zombie = CombatStack::createWalkingDead(3, 35, BattleHex(10, 5).id);

    std::vector<CombatStack> stacks = {archers, pikeman, zombie};
    Pathfinder::ObstacleMask obstacles;

    DynamicWeights weights;
    weights.w_dmg_dealt = 2.0f;
    weights.w_dmg_received_penalty = 1.0f;

    auto bestAction = TacticalSearch::findBestAction(archers, stacks, obstacles, weights);

    std::cout << "  Archer best action: kind=" << static_cast<int>(bestAction.kind) 
              << ", targetStackId=" << bestAction.targetStackId 
              << ", expectedDmg=" << bestAction.expectedDmgDealt << std::endl;

    assert(bestAction.kind == MoveEvaluation::ActionKind::RANGED_ATTACK);
    assert(bestAction.targetStackId == zombie.id);
    assert(bestAction.expectedDmgDealt > 0);

    std::cout << "  -> Passed: Archer correctly chooses ranged attack on enemy zombie." << std::endl;
}

void testPikemanBaitSelection()
{
    std::cout << "[TEST] Running testPikemanBaitSelection..." << std::endl;
    CombatStack archers = CombatStack::createArchers(1, 15, BattleHex(1, 1).id);
    CombatStack pikeman = CombatStack::createPikemanBait(2, BattleHex(5, 5).id);
    CombatStack zombie = CombatStack::createWalkingDead(3, 35, BattleHex(12, 5).id);

    std::vector<CombatStack> stacks = {archers, pikeman, zombie};
    Pathfinder::ObstacleMask obstacles;

    // High bait diversion weight
    DynamicWeights weights;
    weights.w_bait_diversion = 1000.0f;
    weights.w_dmg_dealt = 0.1f;
    weights.w_dmg_received_penalty = 10.0f; // Don't suicide into 35 zombies yet

    auto bestAction = TacticalSearch::findBestAction(pikeman, stacks, obstacles, weights);

    std::cout << "  Pikeman best action: kind=" << static_cast<int>(bestAction.kind)
              << ", toHex=" << bestAction.targetHex 
              << ", baitsEnemy=" << bestAction.baitsEnemy << std::endl;

    assert(bestAction.kind == MoveEvaluation::ActionKind::MOVE);
    assert(bestAction.baitsEnemy);

    std::cout << "  -> Passed: Pikeman moves to bait position with high w_bait_diversion." << std::endl;
}

void testPikemanStripRetaliationSelection()
{
    std::cout << "[TEST] Running testPikemanStripRetaliationSelection..." << std::endl;
    CombatStack archers = CombatStack::createArchers(1, 15, BattleHex(1, 1).id);
    // Pikeman adjacent to Zombie
    BattleHex zombieHex(5, 5);
    BattleHex pikemanHex = HexGrid::getNeighbor(zombieHex, HexDirection::LEFT);

    CombatStack pikeman = CombatStack::createPikemanBait(2, pikemanHex.id);
    CombatStack zombie = CombatStack::createWalkingDead(3, 35, zombieHex.id);
    zombie.hasRetaliation = true;

    std::vector<CombatStack> stacks = {archers, pikeman, zombie};
    Pathfinder::ObstacleMask obstacles;

    // High strip retaliation weight
    DynamicWeights weights;
    weights.w_strip_retaliation = 5000.0f;
    weights.w_dmg_received_penalty = 1.0f;

    auto bestAction = TacticalSearch::findBestAction(pikeman, stacks, obstacles, weights);

    std::cout << "  Pikeman action when adjacent to retaliating zombie: kind=" 
              << static_cast<int>(bestAction.kind)
              << ", stripsRetaliation=" << bestAction.stripsRetaliation << std::endl;

    assert(bestAction.kind == MoveEvaluation::ActionKind::MELEE_ATTACK);
    assert(bestAction.stripsRetaliation);

    std::cout << "  -> Passed: Pikeman sacrifices to strip retaliation when w_strip_retaliation is high." << std::endl;
}

int main()
{
    std::cout << "=== HoMM3 / VCMI Tactical Search Unit Tests ===" << std::endl;
    testArcherShootingSelection();
    testPikemanBaitSelection();
    testPikemanStripRetaliationSelection();
    std::cout << "=== ALL TACTICAL SEARCH TESTS PASSED! ===" << std::endl;
    return 0;
}
