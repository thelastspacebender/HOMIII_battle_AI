#include <iostream>
#include <cassert>
#include <vector>
#include "CombatStack.h"
#include "DamageCalculator.h"
#include "TurnQueue.h"

using namespace BattleAI;

void testAttackDefenseFormula()
{
    std::cout << "[TEST] Running testAttackDefenseFormula..." << std::endl;
    // Equal Atk and Def -> mult 1.0
    float multEqual = DamageCalculator::getAttackDefenseMultiplier(5, 5);
    assert(std::abs(multEqual - 1.0f) < 1e-4f);

    // Atk 15, Def 5 -> diff +10 -> +50% -> mult 1.5
    float multAdv = DamageCalculator::getAttackDefenseMultiplier(15, 5);
    assert(std::abs(multAdv - 1.5f) < 1e-4f);

    // Atk 5, Def 15 -> diff -10 -> -25% -> mult 0.75
    float multDisadv = DamageCalculator::getAttackDefenseMultiplier(5, 15);
    assert(std::abs(multDisadv - 0.75f) < 1e-4f);

    std::cout << "  -> Passed: Atk/Def multiplier matches exact HoMM3 formula." << std::endl;
}

void testRangedShootingAndBlocking()
{
    std::cout << "[TEST] Running testRangedShootingAndBlocking..." << std::endl;
    std::mt19937 rng(42);

    // 15 Archers at hex 18 (1, 1), 35 Zombies at hex 154 (1, 9) -> distance 8 (< 10)
    CombatStack archers = CombatStack::createArchers(1, 15, 18);
    CombatStack zombies = CombatStack::createWalkingDead(2, 35, 154);

    std::vector<CombatStack> stacks = {archers, zombies};

    // Archers shoot unblocked at short range
    assert(!DamageCalculator::isShooterBlocked(archers, stacks));

    int hpBefore = zombies.getTotalHp();
    auto outcome = DamageCalculator::executeAttack(archers, zombies, true, rng);
    assert(outcome.isRanged);
    assert(!outcome.isDistancePenalty);
    assert(outcome.damageDealt > 0);
    assert(!outcome.retaliationOccurred); // Zombies cannot retaliate at range!
    assert(zombies.getTotalHp() < hpBefore);
    assert(archers.shotsLeft == 11);

    // Now place a zombie adjacent to archer: hex 19 (2, 1) is neighbor of (1, 1)
    CombatStack adjacentZombie = CombatStack::createWalkingDead(3, 10, 19);
    stacks.push_back(adjacentZombie);
    assert(DamageCalculator::isShooterBlocked(archers, stacks));

    std::cout << "  -> Passed: Ranged mechanics, distance threshold, and melee block verified." << std::endl;
}

void testMeleeAndRetaliation()
{
    std::cout << "[TEST] Running testMeleeAndRetaliation..." << std::endl;
    std::mt19937 rng(1337);

    // 1 Pikeman attacks 35 Zombies in melee
    CombatStack pikeman = CombatStack::createPikemanBait(1, 35);
    CombatStack zombies = CombatStack::createWalkingDead(2, 35, 36);

    assert(zombies.hasRetaliation);

    // Pikeman strikes
    auto outcome = DamageCalculator::executeAttack(pikeman, zombies, false, rng);
    assert(outcome.damageDealt > 0);
    assert(outcome.retaliationOccurred); // Zombie survives and retaliates!
    assert(!zombies.hasRetaliation);    // Zombie's retaliation is now STRIPPED!
    assert(!pikeman.isAlive);           // 1 Pikeman is killed by 35 zombies retaliation

    std::cout << "  Pikeman stripped zombie retaliation! Zombie hasRetaliation = " 
              << zombies.hasRetaliation << std::endl;
    assert(!zombies.hasRetaliation);

    // Now if another stack hits the zombie, no retaliation will occur!
    CombatStack secondPikeman = CombatStack::createPikemanBait(3, 34);
    auto outcome2 = DamageCalculator::executeAttack(secondPikeman, zombies, false, rng);
    assert(!outcome2.retaliationOccurred); // Zombie has NO retaliation left!
    assert(secondPikeman.isAlive);         // Second pikeman survives!

    std::cout << "  -> Passed: Retaliation stripping works 100% as intended." << std::endl;
}

void testTurnQueue()
{
    std::cout << "[TEST] Running testTurnQueue..." << std::endl;
    CombatStack archers = CombatStack::createArchers(1, 15, 18); // Speed 4, Ally
    CombatStack pikeman = CombatStack::createPikemanBait(2, 35);  // Speed 4, Ally
    CombatStack zombies = CombatStack::createWalkingDead(3, 35, 100); // Speed 3, Enemy

    std::vector<CombatStack> stacks = {archers, pikeman, zombies};
    TurnQueue queue;

    // Both archers & pikemen have speed 4 (Ally), Zombies speed 3
    int firstId = queue.getNextActiveStackId(stacks);
    assert(firstId == archers.id); // Speed 4 Ally

    // Mark archers as waited
    stacks[0].hasWaited = true;
    int nextId = queue.getNextActiveStackId(stacks);
    assert(nextId == pikeman.id); // Pikeman acts before waited archers

    // Mark pikeman as acted
    stacks[1].hasActed = true;
    nextId = queue.getNextActiveStackId(stacks);
    assert(nextId == zombies.id); // Zombie acts next

    // Mark zombies as acted
    stacks[2].hasActed = true;
    nextId = queue.getNextActiveStackId(stacks);
    assert(nextId == archers.id); // Archers now act in the wait phase!

    stacks[0].hasActed = true;
    assert(queue.currentRound == 1);

    // Next call should advance round to 2
    nextId = queue.getNextActiveStackId(stacks);
    assert(queue.currentRound == 2);
    assert(stacks[0].hasRetaliation); // Reset for new round

    std::cout << "  -> Passed: Turn queue initiative, wait phase, and round rollover verified." << std::endl;
}

int main()
{
    std::cout << "=== HoMM3 / VCMI Combat Physics Unit Tests ===" << std::endl;
    testAttackDefenseFormula();
    testRangedShootingAndBlocking();
    testMeleeAndRetaliation();
    testTurnQueue();
    std::cout << "=== ALL COMBAT PHYSICS TESTS PASSED! ===" << std::endl;
    return 0;
}
