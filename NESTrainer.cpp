#include "NESTrainer.h"
#include <iostream>
#include <numeric>

namespace BattleAI
{

NESTrainer::NESTrainer(NeuralWeightModulator& modulator, float learningRate, float sig, int populationSize)
    : brain(modulator),
      adam(modulator.getTrainableHeadParams().size(), learningRate),
      sigma(sig),
      popSize(populationSize),
      rng(42)
{
}

std::vector<CombatStack> NESTrainer::createCryptScenario()
{
    std::vector<CombatStack> stacks;
    // 15 Лучников в верхнем левом углу: колонка 0, ряд 1 -> гекс 15
    stacks.push_back(CombatStack::createArchers(1, 15, BattleHex(0, 1).id));

    // 2 Пикинера-приманки (BAIT): колонка 0, ряд 4 (гекс 60) и колонка 0, ряд 8 (гекс 120)
    stacks.push_back(CombatStack::createPikemanBait(2, BattleHex(0, 4).id));
    stacks.push_back(CombatStack::createPikemanBait(3, BattleHex(0, 8).id));

    // 35 Зомби в противоположном конце поля: колонка 14, ряд 5 -> гекс 89
    stacks.push_back(CombatStack::createWalkingDead(4, 35, BattleHex(14, 5).id));

    return stacks;
}

Pathfinder::ObstacleMask NESTrainer::createCryptObstacles()
{
    Pathfinder::ObstacleMask mask;
    // Статические препятствия на поле Погоста
    mask.set(BattleHex(6, 3).id);
    mask.set(BattleHex(7, 7).id);
    return mask;
}

BattleStats NESTrainer::simulateSingleBattle(
    NeuralWeightModulator& activeBrain,
    std::mt19937& battleRng,
    bool againstBattleAI,
    int maxRounds)
{
    auto stacks = createCryptScenario();
    auto obstacles = createCryptObstacles();
    TurnQueue queue;
    BattlefieldTensor tensor;

    while (queue.currentRound <= maxRounds)
    {
        int activeId = queue.getNextActiveStackId(stacks);
        if (activeId == -1) break; // Бой завершен

        CombatStack* activeStack = nullptr;
        for (auto& s : stacks)
        {
            if (s.id == activeId) { activeStack = &s; break; }
        }
        if (!activeStack || !activeStack->isAlive) continue;

        if (activeStack->side == CombatSide::ALLY)
        {
            tensor.extractFromState(stacks, activeStack->id, obstacles, queue, againstBattleAI);
            DynamicWeights weights = activeBrain.predictWeights(tensor, againstBattleAI);
            MoveEvaluation best = TacticalSearch::findBestAction(*activeStack, stacks, obstacles, weights);

            switch (best.kind)
            {
            case MoveEvaluation::ActionKind::RANGED_ATTACK:
            {
                CombatStack* target = nullptr;
                for (auto& s : stacks) { if (s.id == best.targetStackId) { target = &s; break; } }
                if (target)
                {
                    DamageCalculator::executeAttack(*activeStack, *target, true, battleRng);
                }
                activeStack->hasActed = true;
                break;
            }
            case MoveEvaluation::ActionKind::MELEE_ATTACK:
            {
                activeStack->positionHex = best.sourceHex;
                CombatStack* target = nullptr;
                for (auto& s : stacks) { if (s.id == best.targetStackId) { target = &s; break; } }
                if (target)
                {
                    DamageCalculator::executeAttack(*activeStack, *target, false, battleRng);
                }
                activeStack->hasActed = true;
                break;
            }
            case MoveEvaluation::ActionKind::MOVE:
                activeStack->positionHex = best.targetHex;
                activeStack->hasActed = true;
                break;
            case MoveEvaluation::ActionKind::WAIT:
                activeStack->hasWaited = true;
                break;
            case MoveEvaluation::ActionKind::DEFEND:
            default:
                activeStack->hasActed = true;
                break;
            }
        }
        else // ENEMY (Зомби)
        {
            AIAction enemyAction = EnemyBattleAI::planAction(*activeStack, stacks, obstacles);
            switch (enemyAction.type)
            {
            case AIActionType::MOVE:
                activeStack->positionHex = enemyAction.toHex;
                activeStack->hasActed = true;
                break;
            case AIActionType::MELEE_ATTACK:
            {
                CombatStack* target = nullptr;
                for (auto& s : stacks) { if (s.id == enemyAction.targetStackId) { target = &s; break; } }
                if (target)
                {
                    DamageCalculator::executeAttack(*activeStack, *target, false, battleRng);
                }
                activeStack->hasActed = true;
                break;
            }
            case AIActionType::MOVE_AND_ATTACK:
            {
                activeStack->positionHex = enemyAction.toHex;
                CombatStack* target = nullptr;
                for (auto& s : stacks) { if (s.id == enemyAction.targetStackId) { target = &s; break; } }
                if (target)
                {
                    DamageCalculator::executeAttack(*activeStack, *target, false, battleRng);
                }
                activeStack->hasActed = true;
                break;
            }
            case AIActionType::DEFEND:
            case AIActionType::WAIT:
            default:
                activeStack->hasActed = true;
                break;
            }
        }
    }

    BattleStats stats;
    stats.archersRemaining = stacks[0].count;
    stats.pikemenRemaining = stacks[1].count + stacks[2].count;
    stats.zombiesKilled = 35 - stacks[3].count;
    stats.roundsPlayed = queue.currentRound;

    stats.victory = (stacks[3].count <= 0);
    stats.zeroLossArchers = (stats.victory && stats.archersRemaining == 15);

    int archersLost = 15 - stats.archersRemaining;
    int pikemenLost = 2 - stats.pikemenRemaining;

    // Гладкая прогрессивная функция вознаграждения:
    // 1. Убитые зомби (главный драйвер обучения: каждый убитый зомби дает +40 очков)
    float reward = static_cast<float>(stats.zombiesKilled) * 40.0f;

    // 2. Сохранение численности лучников (+30 за каждого выжившего лучника)
    reward += static_cast<float>(stats.archersRemaining) * 30.0f;

    // 3. Выживаемость по времени: награда за раунды кайта (каждый раунд выживания лучников ценен!)
    reward += static_cast<float>(stats.roundsPlayed) * 10.0f;

    // 4. Потери пикинеров-приманок (небольшой штраф, так как их задача — размен)
    reward -= static_cast<float>(pikemenLost) * 5.0f;

    // 5. Джекпоты за победу и чистую победу
    if (stats.zeroLossArchers)
    {
        reward += 600.0f; // Главный джекпот за чистую победу без потерь!
    }
    if (stats.victory)
    {
        reward += 300.0f;
    }

    stats.reward = reward;
    return stats;
}

TrainingEpochStats NESTrainer::trainEpoch(int epochIndex)
{
    TrainingEpochStats epochStats;
    epochStats.epoch = epochIndex;
    epochStats.learningRate = adam.getLearningRate();

    std::vector<float> baseParams = brain.getTrainableHeadParams();
    size_t numParams = baseParams.size();

    std::vector<float> grad(numParams, 0.0f);
    std::normal_distribution<float> normalDist(0.0f, 1.0f);

    int numPairs = popSize / 2;
    float totalReward = 0.0f;
    int winCount = 0;
    int zeroLossCount = 0;

    NeuralWeightModulator tempBrain = brain;

    for (int p = 0; p < numPairs; ++p)
    {
        // Генерация вектора случайного возмущения epsilon для 16 тактических параметров
        std::vector<float> eps(numParams);
        for (size_t i = 0; i < numParams; ++i)
        {
            eps[i] = normalDist(rng);
        }

        // Позитивное возмущение theta + sigma * eps
        std::vector<float> paramsPos = baseParams;
        for (size_t i = 0; i < numParams; ++i)
        {
            paramsPos[i] += sigma * eps[i];
        }
        tempBrain.setTrainableHeadParams(paramsPos);
        BattleStats posStats = simulateSingleBattle(tempBrain, rng, true);

        // Негативное возмущение theta - sigma * eps (Antithetic Sampling)
        std::vector<float> paramsNeg = baseParams;
        for (size_t i = 0; i < numParams; ++i)
        {
            paramsNeg[i] -= sigma * eps[i];
        }
        tempBrain.setTrainableHeadParams(paramsNeg);
        BattleStats negStats = simulateSingleBattle(tempBrain, rng, true);

        totalReward += (posStats.reward + negStats.reward);
        if (posStats.victory) winCount++;
        if (negStats.victory) winCount++;
        if (posStats.zeroLossArchers) zeroLossCount++;
        if (negStats.zeroLossArchers) zeroLossCount++;

        // Эмпирический градиент: (R+ - R-) * eps
        float diffReward = (posStats.reward - negStats.reward) / (popSize * sigma);
        for (size_t i = 0; i < numParams; ++i)
        {
            grad[i] += diffReward * eps[i];
        }
    }

    // Шаг Adam оптимизатора
    adam.step(baseParams, grad);
    brain.setTrainableHeadParams(baseParams);

    epochStats.meanReward = totalReward / popSize;
    epochStats.winRate = static_cast<float>(winCount) / popSize;
    epochStats.zeroLossRate = static_cast<float>(zeroLossCount) / popSize;

    BattlefieldTensor dummyTensor;
    epochStats.currentWeights = brain.predictWeights(dummyTensor, true);

    return epochStats;
}

} // namespace BattleAI
