#include "Pathfinder.h"
#include <algorithm>

namespace BattleAI
{

std::vector<int> Pathfinder::getReachableHexes(
    int startHex,
    int speed,
    bool isFlying,
    const ObstacleMask& blockedHexes)
{
    std::vector<int> reachable;
    if (startHex < 0 || startHex >= NUM_BATTLE_HEXES || speed <= 0)
    {
        if (startHex >= 0 && startHex < NUM_BATTLE_HEXES) reachable.push_back(startHex);
        return reachable;
    }

    reachable.reserve(100);
    reachable.push_back(startHex); // Всегда можно остаться на месте

    if (isFlying)
    {
        for (int id = 0; id < NUM_BATTLE_HEXES; ++id)
        {
            if (id == startHex) continue;
            BattleHex h(id);
            if (!h.isPlayable()) continue;
            if (blockedHexes.test(id)) continue;

            if (HexGrid::distance(startHex, id) <= speed)
            {
                reachable.push_back(id);
            }
        }
        return reachable;
    }

    // Для наземных существ - волновой BFS
    std::array<int, NUM_BATTLE_HEXES> dist;
    dist.fill(-1);
    dist[startHex] = 0;

    std::queue<int> q;
    q.push(startHex);

    while (!q.empty())
    {
        int curr = q.front();
        q.pop();

        int d = dist[curr];
        if (d >= speed) continue;

        BattleHex currentHex(curr);
        auto neighbors = HexGrid::getValidNeighbors(currentHex, true);

        for (const auto& n : neighbors)
        {
            if (dist[n.id] != -1) continue;
            if (blockedHexes.test(n.id)) continue;

            dist[n.id] = d + 1;
            reachable.push_back(n.id);
            q.push(n.id);
        }
    }

    return reachable;
}

PathResult Pathfinder::findPath(
    int startHex,
    int targetHex,
    bool isFlying,
    const ObstacleMask& blockedHexes,
    bool allowBlockedTarget)
{
    PathResult result;
    if (startHex == targetHex)
    {
        result.reachable = true;
        result.distance = 0;
        return result;
    }

    BattleHex start(startHex);
    BattleHex target(targetHex);
    if (!start.isPlayable() || !target.isPlayable())
    {
        return result;
    }

    if (isFlying)
    {
        if (!allowBlockedTarget && blockedHexes.test(targetHex))
        {
            return result;
        }
        result.distance = HexGrid::distance(start, target);
        result.reachable = true;
        result.path.push_back(targetHex);
        return result;
    }

    // BFS для поиска кратчайшего пути
    std::array<int, NUM_BATTLE_HEXES> dist;
    std::array<int, NUM_BATTLE_HEXES> parent;
    dist.fill(-1);
    parent.fill(-1);

    dist[startHex] = 0;
    std::queue<int> q;
    q.push(startHex);

    while (!q.empty())
    {
        int curr = q.front();
        q.pop();

        if (curr == targetHex) break;

        BattleHex currentHex(curr);
        auto neighbors = HexGrid::getValidNeighbors(currentHex, true);

        for (const auto& n : neighbors)
        {
            if (n.id == targetHex && allowBlockedTarget)
            {
                if (dist[n.id] == -1)
                {
                    dist[n.id] = dist[curr] + 1;
                    parent[n.id] = curr;
                    q.push(n.id);
                }
                continue;
            }

            if (blockedHexes.test(n.id)) continue;

            if (dist[n.id] == -1)
            {
                dist[n.id] = dist[curr] + 1;
                parent[n.id] = curr;
                q.push(n.id);
            }
        }
    }

    if (dist[targetHex] != -1)
    {
        result.reachable = true;
        result.distance = dist[targetHex];

        // Восстановление цепочки пути
        int curr = targetHex;
        while (curr != startHex && curr != -1)
        {
            result.path.push_back(curr);
            curr = parent[curr];
        }
        std::reverse(result.path.begin(), result.path.end());
    }

    return result;
}

int Pathfinder::getPathDistance(
    int startHex,
    int targetHex,
    bool isFlying,
    const ObstacleMask& blockedHexes,
    bool allowBlockedTarget)
{
    PathResult res = findPath(startHex, targetHex, isFlying, blockedHexes, allowBlockedTarget);
    return res.reachable ? res.distance : 999;
}

int Pathfinder::getStepTowards(
    int startHex,
    int targetHex,
    int speed,
    bool isFlying,
    const ObstacleMask& blockedHexes)
{
    if (startHex == targetHex) return startHex;
    if (HexGrid::isAdjacent(BattleHex(startHex), BattleHex(targetHex)))
    {
        return startHex; // Уже стоим вплотную
    }

    PathResult res = findPath(startHex, targetHex, isFlying, blockedHexes, true);
    if (!res.reachable || res.path.empty())
    {
        // Если прямого пути нет, ищем ближайший свободный гекс вокруг цели
        auto neighbors = HexGrid::getValidNeighbors(BattleHex(targetHex), true);
        int bestDist = 999;
        int bestHex = startHex;

        for (const auto& n : neighbors)
        {
            if (blockedHexes.test(n.id)) continue;
            PathResult alt = findPath(startHex, n.id, isFlying, blockedHexes, false);
            if (alt.reachable && alt.distance < bestDist)
            {
                bestDist = alt.distance;
                bestHex = (alt.distance <= speed) ? alt.path.back() : alt.path[speed - 1];
            }
        }
        return bestHex;
    }

    // Если путь есть
    int pathLen = static_cast<int>(res.path.size());
    // Если последний элемент пути - занятая цель, мы не можем встать на нее
    int maxAllowedSteps = pathLen;
    if (blockedHexes.test(targetHex) && res.path.back() == targetHex)
    {
        maxAllowedSteps = pathLen - 1; // Остановка за 1 клетку до цели
    }

    if (maxAllowedSteps <= 0) return startHex;

    int actualSteps = std::min(speed, maxAllowedSteps);
    if (actualSteps <= 0) return startHex;

    return res.path[actualSteps - 1];
}

int Pathfinder::getBestMeleeAttackHex(
    int startHex,
    int targetHex,
    int speed,
    bool isFlying,
    const ObstacleMask& blockedHexes)
{
    // Если уже сосед, атакуем с текущего места
    if (HexGrid::isAdjacent(BattleHex(startHex), BattleHex(targetHex)))
    {
        return startHex;
    }

    auto neighbors = HexGrid::getValidNeighbors(BattleHex(targetHex), true);
    int bestDist = 999;
    int bestAttackHex = -1;

    for (const auto& n : neighbors)
    {
        // Клетка должна быть свободна или совпадать со стартом
        if (blockedHexes.test(n.id) && n.id != startHex) continue;

        int d = getPathDistance(startHex, n.id, isFlying, blockedHexes, false);
        if (d <= speed && d < bestDist)
        {
            bestDist = d;
            bestAttackHex = n.id;
        }
    }

    return bestAttackHex;
}

} // namespace BattleAI
