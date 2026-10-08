#include "HexGrid.h"
#include "CombatStack.h"
#include "DamageCalculator.h"
#include "TurnQueue.h"
#include "EnemyBattleAI.h"
#include "BattlefieldTensor.h"
#include "NeuralWeightModulator.h"
#include "TacticalSearch.h"
#include "AdamOptimizer.h"
#include "NESTrainer.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>
#include <iomanip>

using namespace BattleAI;

class EngineSession
{
public:
    NeuralWeightModulator brain;
    std::vector<CombatStack> stacks;
    Pathfinder::ObstacleMask obstacles;
    TurnQueue queue;
    BattlefieldTensor tensor;
    std::mt19937 rng{1337};
    bool againstBattleAI{true};
    bool isFinished{false};
    std::string winner{"NONE"};

    EngineSession()
    {
        initBattle(true);
    }

    void initBattle(bool againstBot)
    {
        againstBattleAI = againstBot;
        stacks = NESTrainer::createCryptScenario();
        obstacles = NESTrainer::createCryptObstacles();
        queue = TurnQueue();
        queue.currentRound = 1;
        isFinished = false;
        winner = "NONE";
    }

    std::string serializeStacksJson() const
    {
        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < stacks.size(); ++i)
        {
            const auto& s = stacks[i];
            ss << "{"
               << "\"id\":" << s.id << ","
               << "\"name\":\"" << s.name << "\","
               << "\"side\":\"" << (s.side == CombatSide::ALLY ? "ALLY" : "ENEMY") << "\","
               << "\"role\":\"" << roleToString(s.role) << "\","
               << "\"count\":" << s.count << ","
               << "\"initialCount\":" << s.initialCount << ","
               << "\"hpTop\":" << s.currentHpTopUnit << ","
               << "\"hpMax\":" << s.hpPerUnit << ","
               << "\"totalHp\":" << s.getTotalHp() << ","
               << "\"hex\":" << s.positionHex << ","
               << "\"speed\":" << s.getEffectiveSpeed() << ","
               << "\"isAlive\":" << (s.isAlive ? "true" : "false") << ","
               << "\"isShooter\":" << (s.isShooter ? "true" : "false") << ","
               << "\"shots\":" << s.shotsLeft << ","
               << "\"hasRetal\":" << (s.hasRetaliation ? "true" : "false")
               << "}";
            if (i + 1 < stacks.size()) ss << ",";
        }
        ss << "]";
        return ss.str();
    }

    std::string serializeObstaclesJson() const
    {
        std::ostringstream ss;
        ss << "[";
        bool first = true;
        for (size_t h = 0; h < NUM_BATTLE_HEXES; ++h)
        {
            if (obstacles.test(h))
            {
                if (!first) ss << ",";
                ss << h;
                first = false;
            }
        }
        ss << "]";
        return ss.str();
    }

    std::string serializeWeightsJson(const DynamicWeights& w) const
    {
        std::ostringstream ss;
        ss << "{"
           << "\"strip_retaliation\":" << w.w_strip_retaliation << ","
           << "\"block_shooter\":" << w.w_block_shooter << ","
           << "\"stand_on_corpse\":" << w.w_stand_on_corpse << ","
           << "\"screen_core\":" << w.w_screen_core << ","
           << "\"kite_distance\":" << w.w_kite_distance << ","
           << "\"bait_diversion\":" << w.w_bait_diversion << ","
           << "\"dmg_dealt\":" << w.w_dmg_dealt << ","
           << "\"dmg_penalty\":" << w.w_dmg_received_penalty
           << "}";
        return ss.str();
    }

    void emitStateEvent(const std::string& extraActionJson = "{}")
    {
        DynamicWeights currentW = brain.predictWeights(tensor, againstBattleAI);

        std::cout << "{"
                  << "\"event\":\"state_update\","
                  << "\"round\":" << queue.currentRound << ","
                  << "\"againstBot\":" << (againstBattleAI ? "true" : "false") << ","
                  << "\"isFinished\":" << (isFinished ? "true" : "false") << ","
                  << "\"winner\":\"" << winner << "\","
                  << "\"action\":" << extraActionJson << ","
                  << "\"weights\":" << serializeWeightsJson(currentW) << ","
                  << "\"stacks\":" << serializeStacksJson() << ","
                  << "\"obstacles\":" << serializeObstaclesJson()
                  << "}" << std::endl;
    }

    void stepTurn()
    {
        if (isFinished)
        {
            emitStateEvent("{\"type\":\"GAME_OVER\",\"reason\":\"Battle already completed\"}");
            return;
        }

        int activeId = queue.getNextActiveStackId(stacks);
        if (activeId == -1)
        {
            // Проверка победителя
            bool allyAlive = false;
            bool enemyAlive = false;
            for (const auto& s : stacks)
            {
                if (s.isAlive)
                {
                    if (s.side == CombatSide::ALLY) allyAlive = true;
                    if (s.side == CombatSide::ENEMY) enemyAlive = true;
                }
            }

            isFinished = true;
            winner = allyAlive ? "ALLY" : (enemyAlive ? "ENEMY" : "DRAW");
            emitStateEvent("{\"type\":\"GAME_OVER\",\"winner\":\"" + winner + "\"}");
            return;
        }

        CombatStack* activeStack = nullptr;
        for (auto& s : stacks) { if (s.id == activeId) { activeStack = &s; break; } }
        if (!activeStack || !activeStack->isAlive)
        {
            return;
        }

        std::ostringstream act;

        if (activeStack->side == CombatSide::ALLY)
        {
            tensor.extractFromState(stacks, activeStack->id, obstacles, queue, againstBattleAI);
            DynamicWeights weights = brain.predictWeights(tensor, againstBattleAI);
            MoveEvaluation best = TacticalSearch::findBestAction(*activeStack, stacks, obstacles, weights);

            switch (best.kind)
            {
            case MoveEvaluation::ActionKind::RANGED_ATTACK:
            {
                CombatStack* target = nullptr;
                for (auto& s : stacks) { if (s.id == best.targetStackId) { target = &s; break; } }
                AttackOutcome outcome;
                if (target) outcome = DamageCalculator::executeAttack(*activeStack, *target, true, rng);
                activeStack->hasActed = true;

                act << "{"
                    << "\"type\":\"RANGED_ATTACK\","
                    << "\"unitId\":" << activeStack->id << ","
                    << "\"fromHex\":" << activeStack->positionHex << ","
                    << "\"targetHex\":" << best.targetHex << ","
                    << "\"targetId\":" << (target ? target->id : -1) << ","
                    << "\"damage\":" << outcome.damageDealt << ","
                    << "\"killed\":" << outcome.unitsKilled << ","
                    << "\"retaliation\":" << (outcome.retaliationOccurred ? outcome.retaliationDamage : 0) << ","
                    << "\"reason\":\"Ranged strike from distance " << HexGrid::distance(activeStack->positionHex, best.targetHex) << "\""
                    << "}";
                break;
            }
            case MoveEvaluation::ActionKind::MELEE_ATTACK:
            {
                int oldHex = activeStack->positionHex;
                activeStack->positionHex = best.sourceHex;
                CombatStack* target = nullptr;
                for (auto& s : stacks) { if (s.id == best.targetStackId) { target = &s; break; } }
                AttackOutcome outcome;
                if (target) outcome = DamageCalculator::executeAttack(*activeStack, *target, false, rng);
                activeStack->hasActed = true;

                act << "{"
                    << "\"type\":\"MELEE_ATTACK\","
                    << "\"unitId\":" << activeStack->id << ","
                    << "\"fromHex\":" << oldHex << ","
                    << "\"stanceHex\":" << best.sourceHex << ","
                    << "\"targetHex\":" << best.targetHex << ","
                    << "\"targetId\":" << (target ? target->id : -1) << ","
                    << "\"damage\":" << outcome.damageDealt << ","
                    << "\"killed\":" << outcome.unitsKilled << ","
                    << "\"retaliation\":" << (outcome.retaliationOccurred ? outcome.retaliationDamage : 0) << ","
                    << "\"retalKilled\":" << outcome.retaliationKilled << ","
                    << "\"reason\":\"" << (best.stripsRetaliation ? "Bait sacrifice to strip enemy retaliation!" : "Melee strike") << "\""
                    << "}";
                break;
            }
            case MoveEvaluation::ActionKind::MOVE:
            {
                int oldHex = activeStack->positionHex;
                activeStack->positionHex = best.targetHex;
                activeStack->hasActed = true;

                std::string reasonStr = best.kitesEnemy ? "Kiting backwards away from slow melee" :
                                       (best.baitsEnemy ? "Bait positioning to divert zombie aggro away from shooter" : "Tactical advance");
                act << "{"
                    << "\"type\":\"MOVE\","
                    << "\"unitId\":" << activeStack->id << ","
                    << "\"fromHex\":" << oldHex << ","
                    << "\"toHex\":" << best.targetHex << ","
                    << "\"reason\":\"" << reasonStr << "\""
                    << "}";
                break;
            }
            case MoveEvaluation::ActionKind::WAIT:
                activeStack->hasWaited = true;
                act << "{"
                    << "\"type\":\"WAIT\","
                    << "\"unitId\":" << activeStack->id << ","
                    << "\"hex\":" << activeStack->positionHex << ","
                    << "\"reason\":\"Preserving tempo to act late in round\""
                    << "}";
                break;
            case MoveEvaluation::ActionKind::DEFEND:
            default:
                activeStack->hasActed = true;
                act << "{"
                    << "\"type\":\"DEFEND\","
                    << "\"unitId\":" << activeStack->id << ","
                    << "\"hex\":" << activeStack->positionHex << ","
                    << "\"reason\":\"Defensive stance (+20% def)\""
                    << "}";
                break;
            }
        }
        else // ENEMY (Зомби)
        {
            AIAction enemyAction = EnemyBattleAI::planAction(*activeStack, stacks, obstacles);
            switch (enemyAction.type)
            {
            case AIActionType::MOVE:
            {
                int oldHex = activeStack->positionHex;
                activeStack->positionHex = enemyAction.toHex;
                activeStack->hasActed = true;
                act << "{"
                    << "\"type\":\"ENEMY_MOVE\","
                    << "\"unitId\":" << activeStack->id << ","
                    << "\"fromHex\":" << oldHex << ","
                    << "\"toHex\":" << enemyAction.toHex << ","
                    << "\"reason\":\"" << enemyAction.reason << "\""
                    << "}";
                break;
            }
            case AIActionType::MELEE_ATTACK:
            {
                CombatStack* target = nullptr;
                for (auto& s : stacks) { if (s.id == enemyAction.targetStackId) { target = &s; break; } }
                AttackOutcome outcome;
                if (target) outcome = DamageCalculator::executeAttack(*activeStack, *target, false, rng);
                activeStack->hasActed = true;
                act << "{"
                    << "\"type\":\"ENEMY_ATTACK\","
                    << "\"unitId\":" << activeStack->id << ","
                    << "\"targetId\":" << (target ? target->id : -1) << ","
                    << "\"damage\":" << outcome.damageDealt << ","
                    << "\"killed\":" << outcome.unitsKilled << ","
                    << "\"reason\":\"" << enemyAction.reason << "\""
                    << "}";
                break;
            }
            case AIActionType::MOVE_AND_ATTACK:
            {
                int oldHex = activeStack->positionHex;
                activeStack->positionHex = enemyAction.toHex;
                CombatStack* target = nullptr;
                for (auto& s : stacks) { if (s.id == enemyAction.targetStackId) { target = &s; break; } }
                AttackOutcome outcome;
                if (target) outcome = DamageCalculator::executeAttack(*activeStack, *target, false, rng);
                activeStack->hasActed = true;
                act << "{"
                    << "\"type\":\"ENEMY_MOVE_ATTACK\","
                    << "\"unitId\":" << activeStack->id << ","
                    << "\"fromHex\":" << oldHex << ","
                    << "\"toHex\":" << enemyAction.toHex << ","
                    << "\"targetId\":" << (target ? target->id : -1) << ","
                    << "\"damage\":" << outcome.damageDealt << ","
                    << "\"killed\":" << outcome.unitsKilled << ","
                    << "\"reason\":\"" << enemyAction.reason << "\""
                    << "}";
                break;
            }
            case AIActionType::DEFEND:
            case AIActionType::WAIT:
            default:
                activeStack->hasActed = true;
                act << "{"
                    << "\"type\":\"ENEMY_DEFEND\","
                    << "\"unitId\":" << activeStack->id << ","
                    << "\"reason\":\"" << enemyAction.reason << "\""
                    << "}";
                break;
            }
        }

        // Проверка завершения боя после действия
        bool allyAlive = false;
        bool enemyAlive = false;
        for (const auto& s : stacks)
        {
            if (s.isAlive)
            {
                if (s.side == CombatSide::ALLY) allyAlive = true;
                if (s.side == CombatSide::ENEMY) enemyAlive = true;
            }
        }

        if (!allyAlive || !enemyAlive)
        {
            isFinished = true;
            winner = allyAlive ? "ALLY" : (enemyAlive ? "ENEMY" : "DRAW");
        }

        emitStateEvent(act.str());
    }

    void runTrain(int epochs)
    {
        NESTrainer trainer(brain, 0.05f, 0.1f, 8);
        for (int e = 1; e <= epochs; ++e)
        {
            TrainingEpochStats stats = trainer.trainEpoch(e);
            std::cout << "{"
                      << "\"event\":\"training_epoch\","
                      << "\"epoch\":" << e << ","
                      << "\"totalEpochs\":" << epochs << ","
                      << "\"reward\":" << stats.meanReward << ","
                      << "\"winRate\":" << stats.winRate << ","
                      << "\"zeroLossRate\":" << stats.zeroLossRate << ","
                      << "\"weights\":" << serializeWeightsJson(stats.currentWeights)
                      << "}" << std::endl;
        }
        brain.saveWeights("trained_weights.bin");
        std::cout << "{\"event\":\"training_complete\",\"epochs\":" << epochs << "}" << std::endl;
    }
};

int main()
{
    // Отключаем буферизацию потоков ввода-вывода для мгновенного IPC обмена с Node.js
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    EngineSession session;
    std::cout << "{\"event\":\"engine_ready\",\"version\":\"1.0-cpp20-hybrid\"}" << std::endl;
    session.emitStateEvent();

    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line.empty()) continue;

        if (line.rfind("CMD:RESET", 0) == 0)
        {
            bool againstBot = (line.find("human") == std::string::npos);
            session.initBattle(againstBot);
            session.emitStateEvent("{\"type\":\"RESET\",\"mode\":\"" + std::string(againstBot ? "BOT" : "HUMAN") + "\"}");
        }
        else if (line.rfind("CMD:STEP", 0) == 0)
        {
            session.stepTurn();
        }
        else if (line.rfind("CMD:GET_STATE", 0) == 0)
        {
            session.emitStateEvent();
        }
        else if (line.rfind("CMD:TRAIN", 0) == 0)
        {
            int epochs = 10;
            if (line.size() > 10)
            {
                try { epochs = std::stoi(line.substr(10)); } catch (...) { epochs = 10; }
            }
            session.runTrain(epochs);
        }
        else if (line.rfind("CMD:EXIT", 0) == 0)
        {
            std::cout << "{\"event\":\"engine_exit\"}" << std::endl;
            break;
        }
        else
        {
            std::cout << "{\"event\":\"error\",\"unknown_cmd\":\"" << line << "\"}" << std::endl;
        }
    }

    return 0;
}
