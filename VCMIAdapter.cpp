#include "VCMIAdapter.h"

namespace BattleAI
{

VCMIAdapter::VCMIAdapter()
    : director(), searcher(director)
{
}

MoveEvaluation VCMIAdapter::decideAction(
    const std::vector<StackSnapshot>& allyStacks,
    const std::vector<StackSnapshot>& enemyStacks,
    const StackSnapshot& activeStack,
    const std::vector<int>& reachableHexes,
    const std::vector<int>& corpseHexes,
    int nextStackIdInQueue,
    bool enemyCanResurrect)
{
    // 1. Макро-анализ армий
    director.analyze(allyStacks, enemyStacks);

    // 2. Генерация ДИНАМИЧЕСКИХ весов под текущую секунду боя
    lastWeights = director.generateWeightsForTurn(activeStack, nextStackIdInQueue, enemyCanResurrect);

    // 3. Сбор контекста
    BattlefieldContext ctx;
    ctx.allyStacks = allyStacks;
    ctx.enemyStacks = enemyStacks;
    ctx.corpseHexes = corpseHexes;
    ctx.nextStackIdInQueue = nextStackIdInQueue;
    ctx.enemyCanResurrect = enemyCanResurrect;

    // 4. Поиск лучшего хода по динамическим весам
    return searcher.selectBestAction(activeStack, ctx, reachableHexes, lastWeights);
}

MoveEvaluation VCMIAdapter::decideActionNeural(
    const std::vector<StackSnapshot>& allyStacks,
    const std::vector<StackSnapshot>& enemyStacks,
    const StackSnapshot& activeStack,
    const std::vector<int>& reachableHexes,
    const std::vector<int>& obstacleHexes,
    const std::vector<int>& corpseHexes,
    int nextStackIdInQueue,
    bool isAgainstBattleAI,
    int roundNumber)
{
    // 1. Макро-анализ армий
    director.analyze(allyStacks, enemyStacks);

    // 2. Сборка полного пространственного тензора поля боя (187x12 + контекст)
    auto tensor = director.extractTensor(
        allyStacks, enemyStacks, activeStack, nextStackIdInQueue,
        isAgainstBattleAI, roundNumber, obstacleHexes, corpseHexes
    );

    // 3. Генерация динамических весов через нейросеть / матрицу поля боя
    lastWeights = director.generateWeightsNeural(tensor);

    // 4. Сбор тактического контекста для поиска
    BattlefieldContext ctx;
    ctx.allyStacks = allyStacks;
    ctx.enemyStacks = enemyStacks;
    ctx.corpseHexes = corpseHexes;
    ctx.nextStackIdInQueue = nextStackIdInQueue;
    ctx.enemyCanResurrect = false;

    // 5. Точный тактический микро-поиск по легальным гексам
    return searcher.selectBestAction(activeStack, ctx, reachableHexes, lastWeights);
}

} // namespace BattleAI
