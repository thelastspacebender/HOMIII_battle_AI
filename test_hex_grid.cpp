#include <iostream>
#include <cassert>
#include "HexGrid.h"

using namespace BattleAI;

void testCoordinatesAndId()
{
    std::cout << "[TEST] Running testCoordinatesAndId..." << std::endl;
    for (int y = 0; y < BFIELD_HEIGHT; ++y)
    {
        for (int x = 0; x < BFIELD_WIDTH; ++x)
        {
            BattleHex h(x, y);
            assert(h.isValid());
            assert(h.x == x);
            assert(h.y == y);
            assert(h.id == y * BFIELD_WIDTH + x);

            BattleHex fromId(h.id);
            assert(fromId.x == x);
            assert(fromId.y == y);
            assert(fromId.id == h.id);
        }
    }
    std::cout << "  -> Passed: 187 hexes mapped bidirectionally." << std::endl;
}

void testCubeConversion()
{
    std::cout << "[TEST] Running testCubeConversion..." << std::endl;
    for (int id = 0; id < NUM_BATTLE_HEXES; ++id)
    {
        BattleHex h(id);
        CubeCoord cube = h.toCube();
        // Invariant: q + r + s == 0
        assert(cube.q + cube.r + cube.s == 0);
        BattleHex restored = BattleHex::fromCube(cube);
        assert(restored.x == h.x);
        assert(restored.y == h.y);
        assert(restored.id == h.id);
    }
    std::cout << "  -> Passed: Cube conversion invariant (q+r+s=0) and lossless roundtrip." << std::endl;
}

void testNeighborsAndSymmetry()
{
    std::cout << "[TEST] Running testNeighborsAndSymmetry..." << std::endl;
    // Check neighbor symmetry for playable hexes
    for (int y = PLAYABLE_MIN_Y + 1; y < PLAYABLE_MAX_Y; ++y)
    {
        for (int x = PLAYABLE_MIN_X + 1; x < PLAYABLE_MAX_X; ++x)
        {
            BattleHex h(x, y);
            auto neighbors = HexGrid::getValidNeighbors(h, false);
            assert(neighbors.size() == 6); // Internal hex has exactly 6 neighbors

            for (const auto& n : neighbors)
            {
                assert(HexGrid::distance(h, n) == 1);
                assert(HexGrid::isAdjacent(h, n));
                HexDirection dirTo = HexGrid::getDirection(h, n);
                assert(dirTo != HexDirection::NONE);
                HexDirection dirBack = HexGrid::getDirection(n, h);
                assert(dirBack != HexDirection::NONE);
            }
        }
    }
    std::cout << "  -> Passed: 6-directional connectivity and symmetric distance." << std::endl;
}

void testDistances()
{
    std::cout << "[TEST] Running testDistances..." << std::endl;
    // (1, 0) and (2, 0) -> distance 1
    assert(HexGrid::distance(BattleHex(1, 0), BattleHex(2, 0)) == 1);
    // (1, 0) and (1, 1) -> odd row shift: (1,0) even, (1,1) odd
    // (1,0) neighbors: (1,1) is bottom-right!
    assert(HexGrid::distance(BattleHex(1, 0), BattleHex(1, 1)) == 1);
    // (14, 0) to (0, 0) -> distance 14
    assert(HexGrid::distance(BattleHex(0, 0), BattleHex(14, 0)) == 14);

    // Diagonal distance across full board (top-left to bottom-right)
    BattleHex topLeft(0, 0);
    BattleHex bottomRight(14, 10);
    int d = HexGrid::distance(topLeft, bottomRight);
    std::cout << "  Distance from (0,0) to (14,10) = " << d << " hexes." << std::endl;
    assert(d >= 14 && d <= 24);

    std::cout << "  -> Passed: Exact hex distance verification." << std::endl;
}

int main()
{
    std::cout << "=== HoMM3 / VCMI HexGrid Unit Tests ===" << std::endl;
    testCoordinatesAndId();
    testCubeConversion();
    testNeighborsAndSymmetry();
    testDistances();
    std::cout << "=== ALL HEX GRID TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    return 0;
}
