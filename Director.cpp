#include "Director.h"
#include <algorithm>

namespace BattleAI
{

void Director::analyze(const std::vector<StackSnapshot>& allyStacks, 
                       const std::vector<StackSnapshot>& enemyStacks)
{
    roleAssignments.clear();

    if (allyStacks.empty() || enemyStacks.empty())
        return;

    float totalAllyValue = 0.0f;
    for (const auto& s : allyStacks)
        totalAllyValue += s.combatValue;

    float totalEnemyValue = 0.0f;
    for (const auto& s : enemyStacks)
        totalEnemyValue += s.combatValue;

    strengthRatio = (totalEnemyValue > 0.0f) ? (totalAllyValue / totalEnemyValue) : 999.0f;

    auto allyCoreIt = std::max_element(allyStacks.begin(), allyStacks.end(),
        [](const StackSnapshot& a, const StackSnapshot& b) {
            return a.combatValue < b.combatValue;
        });
    if (allyCoreIt != allyStacks.end())
        allyCoreId = allyCoreIt->id;

    auto enemyCoreIt = std::max_element(enemyStacks.begin(), enemyStacks.end(),
        [](const StackSnapshot& a, const StackSnapshot& b) {
            return a.combatValue < b.combatValue;
        });
    if (enemyCoreIt != enemyStacks.end())
        enemyCoreId = enemyCoreIt->id;

    for (const auto& s : allyStacks)
    {
        if (s.id == allyCoreId)
        {
            roleAssignments[s.id] = StackRole::CORE;
        }
        else if (s.combatValue < (0.08f * totalAllyValue) || s.count <= 2)
        {
            roleAssignments[s.id] = StackRole::BAIT;
        }
        else if (s.speed >= 8 || s.isFlying)
        {
            roleAssignments[s.id] = StackRole::INTERCEPTOR;
        }
        else
        {
            roleAssignments[s.id] = StackRole::BODYGUARD;
        }
    }

    if (strengthRatio > 2.5f)
        currentPhase = BattlePhase::MOP_UP;
    else if (strengthRatio > 1.2f)
        currentPhase = BattlePhase::BREAKTHROUGH;
    else
        currentPhase = BattlePhase::CONTAINMENT;
}

DynamicWeights Director::generateWeightsForTurn(
    const StackSnapshot& activeStack,
    int nextStackInQueue,
    bool enemyHasResurrection) const
{
    DynamicWeights w;
    StackRole myRole = getRole(activeStack.id);

    // 1. ДИНАМИЧЕСКИЙ ВЕС СРЫВА ОТВЕТКИ:
    // Если следующим в очереди ходит НАШ ГЛАВНЫЙ СТЕК (allyCoreId) -> срыв ответки жизненно необходим!
    // Если наш главный стек уже сходил -> сливать юнита ради срыва ответки бессмысленно (штрафуем за суицид).
    if (nextStackInQueue == allyCoreId)
    {
        w.w_strip_retaliation = 850.0f; // Огромный приоритет!
    }
    else
    {
        w.w_strip_retaliation = -250.0f; // Штраф за бессмысленный суицид
    }

    // 2. ДИНАМИЧЕСКИЙ ВЕС ЗАКРЫТИЯ ТРУПА:
    // Если у врага есть воскрешение/некромантия -> труп закрывать критично!
    if (enemyHasResurrection)
    {
        w.w_stand_on_corpse = 600.0f;
    }
    else
    {
        w.w_stand_on_corpse = 0.0f; // Трупы не несут опасности
    }

    // 3. ДИНАМИЧЕСКИЙ ВЕС БЛОКИРОВКИ СТРЕЛКА:
    w.w_block_shooter = 450.0f;

    // 4. ДИНАМИЧЕСКИЙ ВЕС ПРИКРЫТИЯ ЯДРА (SCREENING):
    if (myRole == StackRole::BODYGUARD)
    {
        w.w_screen_core = 400.0f;
    }

    // 5. ДИНАМИЧЕСКИЙ ШТРАФ ЗА ПОЛУЧЕННЫЙ УРОН:
    if (myRole == StackRole::CORE)
    {
        w.w_dmg_received_penalty = 3.5f; // Наш главный стек должен беречь каждую единицу HP!
    }
    else if (myRole == StackRole::BAIT)
    {
        w.w_dmg_received_penalty = 0.2f; // Приманкой можно жертвовать, цена жизни минимальна
    }
    else
    {
        w.w_dmg_received_penalty = 1.0f;
    }

    w.w_dmg_dealt = 1.2f;
    w.w_preserve_tempo = 120.0f;

    return w;
}

StackRole Director::getRole(int stackId) const
{
    auto it = roleAssignments.find(stackId);
    if (it != roleAssignments.end())
        return it->second;
    return StackRole::UNKNOWN;
}

BattlefieldTensor Director::extractTensor(
    const std::vector<StackSnapshot>& allyStacks,
    const std::vector<StackSnapshot>& enemyStacks,
    const StackSnapshot& activeStack,
    int nextStackInQueue,
    bool isAgainstBattleAI,
    int roundNumber,
    const std::vector<int>& obstacleHexes,
    const std::vector<int>& corpseHexes) const
{
    BattlefieldTensor tensor;

    // 1. Препятствия
    for (int obHex : obstacleHexes)
    {
        tensor.setObstacle(obHex, 1.0f);
    }

    // 2. Трупы на поле боя
    for (int cHex : corpseHexes)
    {
        tensor.setCorpse(cHex, 1.0f);
    }

    // 3. Союзные отряды
    for (const auto& s : allyStacks)
    {
        if (s.totalHp <= 0) continue;

        StackRole r = getRole(s.id);
        float roleScore = 0.5f;
        if (r == StackRole::CORE) roleScore = 1.0f;
        else if (r == StackRole::BODYGUARD) roleScore = 0.5f;
        else if (r == StackRole::BAIT) roleScore = 0.25f;
        else if (r == StackRole::INTERCEPTOR) roleScore = 0.75f;

        float maxHp = (s.count > 0 && s.hpPerUnit > 0) ? (float)(s.count * s.hpPerUnit) : (float)s.totalHp;
        float hpRatio = (maxHp > 0.0f) ? ((float)s.totalHp / maxHp) : 1.0f;

        bool isActive = (s.id == activeStack.id);
        float nextSignal = (s.id == nextStackInQueue) ? ((s.id == allyCoreId) ? 1.0f : -1.0f) : 0.0f;

        tensor.setStack(s.hex, true, roleScore, hpRatio, s.count, s.speed, s.isShooter,
                        s.hasRetaliation, isActive, nextSignal);
    }

    // 4. Вражеские отряды
    for (const auto& s : enemyStacks)
    {
        if (s.totalHp <= 0) continue;

        float maxHp = (s.count > 0 && s.hpPerUnit > 0) ? (float)(s.count * s.hpPerUnit) : (float)s.totalHp;
        float hpRatio = (maxHp > 0.0f) ? ((float)s.totalHp / maxHp) : 1.0f;

        bool isActive = (s.id == activeStack.id);
        float nextSignal = (s.id == nextStackInQueue) ? -1.0f : 0.0f;

        tensor.setStack(s.hex, false, -1.0f, hpRatio, s.count, s.speed, s.isShooter,
                        s.hasRetaliation, isActive, nextSignal);
    }

    // 5. Расстояния от активного гекса до всех клеток поля
    tensor.computeDistancesFrom(activeStack.hex);

    // 6. Глобальный контекст
    tensor.setContext(isAgainstBattleAI, strengthRatio, roundNumber);

    return tensor;
}

} // namespace BattleAI
