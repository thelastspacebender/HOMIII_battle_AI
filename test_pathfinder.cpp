#include <iostream>
#include <cassert>
#include "Pathfinder.h"

using namespace BattleAI;

void testReachableHexesEmptyField()
{
    std::cout << "[TEST] Running testReachableHexesEmptyField..." << std::endl;
    Pathfinder::ObstacleMask mask; // Empty field
    int startHex = BattleHex(8, 5).id; // Center of board (approx)

    // Speed 1 -> 1 (center) + 6 (neighbors) = 7 hexes
    auto reach1 = Pathfinder::getReachableHexes(startHex, 1, false, mask);
    assert(reach1.size() == 7);

    // Speed 3 (Zombie speed)
    auto reach3 = Pathfinder::getReachableHexes(startHex, 3, false, mask);
    // Formula for hex radius 3: 1 + 3 * 3 * 4 = 37 hexes (if not cut off by board border)
    assert(reach3.size() == 37);

    // Speed 4 (Pikeman speed)
    auto reach4 = Pathfinder::getReachableHexes(startHex, 4, false, mask);
    assert(reach4.size() == 61);

    std::cout << "  -> Passed: Reachable hexes count matches hex ring formulas (r=1: 7, r=3: 37, r=4: 61)." << std::endl;
}

void testObstacleCircumvention()
{
    std::cout << "[TEST] Running testObstacleCircumvention..." << std::endl;
    Pathfinder::ObstacleMask mask;
    // Build a vertical wall between x=5 and x=7 at x=6, from y=2 to y=7
    for (int y = 2; y <= 7; ++y)
    {
        mask.set(BattleHex(6, y).id);
    }

    int start = BattleHex(4, 5).id;
    int goal = BattleHex(8, 5).id;

    // Ground unit must go around the wall
    PathResult groundRes = Pathfinder::findPath(start, goal, false, mask);
    assert(groundRes.reachable);
    assert(groundRes.distance > HexGrid::distance(BattleHex(start), BattleHex(goal)));
    std::cout << "  Ground path around wall length = " << groundRes.distance 
              << " (direct hex distance was " << HexGrid::distance(BattleHex(start), BattleHex(goal)) << ")" << std::endl;

    // Verify none of the path hexes intersect the wall
    for (int h : groundRes.path)
    {
        assert(!mask.test(h));
    }

    // Flying unit ignores the wall
    PathResult flyRes = Pathfinder::findPath(start, goal, true, mask);
    assert(flyRes.reachable);
    assert(flyRes.distance == HexGrid::distance(BattleHex(start), BattleHex(goal)));
    std::cout << "  Flying path length = " << flyRes.distance << " (direct distance)." << std::endl;

    std::cout << "  -> Passed: Ground units correctly circumvent obstacles, flying ignores them." << std::endl;
}

void testStepTowardsAndAttackHex()
{
    std::cout << "[TEST] Running testStepTowardsAndAttackHex..." << std::endl;
    Pathfinder::ObstacleMask mask;

    // Zombie at (14, 5) trying to reach Archer at (2, 5)
    BattleHex zombie(14, 5);
    BattleHex archer(2, 5);
    mask.set(archer.id); // Archer occupies (2, 5)

    int speedZombie = 3;
    int nextHex = Pathfinder::getStepTowards(zombie.id, archer.id, speedZombie, false, mask);
    assert(nextHex != zombie.id);
    assert(nextHex != archer.id);

    int distBefore = HexGrid::distance(zombie, archer);
    int distAfter = HexGrid::distance(BattleHex(nextHex), archer);
    assert(distAfter == distBefore - speedZombie);
    std::cout << "  Zombie distance to Archer: before = " << distBefore << ", after step = " << distAfter << std::endl;

    // Test melee attack hex when adjacent to target
    BattleHex adjacentHex = HexGrid::getNeighbor(archer, HexDirection::RIGHT);
    int attackHexWhenAdjacent = Pathfinder::getBestMeleeAttackHex(adjacentHex.id, archer.id, speedZombie, false, mask);
    assert(attackHexWhenAdjacent == adjacentHex.id);

    std::cout << "  -> Passed: Zombie step-towards moves exactly speed hexes closer to Archer." << std::endl;
}

int main()
{
    std::cout << "=== HoMM3 / VCMI Pathfinder Unit Tests ===" << std::endl;
    testReachableHexesEmptyField();
    testObstacleCircumvention();
    testStepTowardsAndAttackHex();
    std::cout << "=== ALL PATHFINDER TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    return 0;
}
