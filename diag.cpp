#include <iostream>
#include "NESTrainer.h"
#include "HexGrid.h"

using namespace BattleAI;

int main() {
    NeuralWeightModulator brain;
    auto stacks = NESTrainer::createCryptScenario();
    auto obstacles = NESTrainer::createCryptObstacles();
    TurnQueue queue;
    BattlefieldTensor tensor;
    std::mt19937 rng(42);

    std::cout << "Starting full diagnostic battle simulation..." << std::endl;
    for (int r = 1; r <= 15; ++r) {
        std::cout << "\n=== ROUND " << r << " ===" << std::endl;
        while (queue.currentRound == r) {
            int activeId = queue.getNextActiveStackId(stacks);
            if (activeId == -1) { 
                std::cout << "Battle ended! Round: " << r << std::endl;
                std::cout << "Archers: " << stacks[0].count << ", Pikemen: " << stacks[1].count + stacks[2].count 
                          << ", Zombies: " << stacks[3].count << std::endl;
                return 0; 
            }
            CombatStack* s = nullptr;
            for (auto& st : stacks) if (st.id == activeId) s = &st;
            if (!s || !s->isAlive) continue;

            if (s->side == CombatSide::ALLY) {
                tensor.extractFromState(stacks, s->id, obstacles, queue, true);
                DynamicWeights w = brain.predictWeights(tensor, true);
                MoveEvaluation best = TacticalSearch::findBestAction(*s, stacks, obstacles, w);
                std::cout << "ALLY " << s->name << " (id=" << s->id << ", hex=" << s->positionHex 
                          << " [" << s->positionHex%15 << "," << s->positionHex/15 << "]) -> action=" 
                          << (int)best.kind << ", toHex=" << best.targetHex 
                          << " [" << best.targetHex%15 << "," << best.targetHex/15 << "]"
                          << ", baits=" << best.baitsEnemy << " (score=" << best.baitScore << ")"
                          << ", screens=" << best.screensAllyCore << std::endl;
                
                if (best.kind == MoveEvaluation::ActionKind::MOVE) {
                    s->positionHex = best.targetHex;
                } else if (best.kind == MoveEvaluation::ActionKind::RANGED_ATTACK) {
                    for (auto& tgt : stacks) if (tgt.id == best.targetStackId) {
                        auto out = DamageCalculator::executeAttack(*s, tgt, true, rng);
                        std::cout << "  -> Shot! Dmg: " << out.damageDealt << ", killed: " << out.unitsKilled 
                                  << ", remaining: " << tgt.count << std::endl;
                    }
                } else if (best.kind == MoveEvaluation::ActionKind::MELEE_ATTACK) {
                    s->positionHex = best.sourceHex;
                    for (auto& tgt : stacks) if (tgt.id == best.targetStackId) {
                        auto out = DamageCalculator::executeAttack(*s, tgt, false, rng);
                        std::cout << "  -> Melee! Dmg: " << out.damageDealt << ", killed: " << out.unitsKilled << std::endl;
                    }
                } else if (best.kind == MoveEvaluation::ActionKind::WAIT) {
                    s->hasWaited = true;
                    continue;
                }
                s->hasActed = true;
            } else {
                AIAction a = EnemyBattleAI::planAction(*s, stacks, obstacles);
                std::cout << "ENEMY " << s->name << " (hex=" << s->positionHex 
                          << " [" << s->positionHex%15 << "," << s->positionHex/15 << "]) target=" 
                          << a.targetStackId << " -> toHex=" << a.toHex 
                          << " [" << a.toHex%15 << "," << a.toHex/15 << "]"
                          << " (" << a.reason << ")" << std::endl;
                
                if (a.type == AIActionType::MOVE) {
                    s->positionHex = a.toHex;
                } else if (a.type == AIActionType::MOVE_AND_ATTACK) {
                    s->positionHex = a.toHex;
                    for (auto& tgt : stacks) if (tgt.id == a.targetStackId) {
                        auto out = DamageCalculator::executeAttack(*s, tgt, false, rng);
                        std::cout << "  -> ENEMY ATK! Target " << tgt.name << " took " << out.damageDealt 
                                  << " dmg, killed: " << out.unitsKilled << ", remaining: " << tgt.count << std::endl;
                    }
                } else if (a.type == AIActionType::MELEE_ATTACK) {
                    for (auto& tgt : stacks) if (tgt.id == a.targetStackId) {
                        auto out = DamageCalculator::executeAttack(*s, tgt, false, rng);
                        std::cout << "  -> ENEMY MELEE! Target " << tgt.name << " took " << out.damageDealt 
                                  << " dmg, killed: " << out.unitsKilled << ", remaining: " << tgt.count << std::endl;
                    }
                }
                s->hasActed = true;
            }
        }
    }
    return 0;
}
