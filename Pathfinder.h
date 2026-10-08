#pragma once

#include "HexGrid.h"
#include <vector>
#include <bitset>
#include <queue>
#include <array>
#include <limits>

namespace BattleAI
{

/**
 * @file Pathfinder.h
 * @brief Волновой алгоритм поиска путей (BFS / Dijkstra) для поля боя HoMM3 / VCMI.
 * 
 * Особенности механики HoMM3:
 * 1. Наземные отряды (Ground units) огибают препятствия и другие отряды шаг за шагом.
 * 2. Летающие отряды (Flying units) перемещаются по воздуху, игнорируя препятствия на пути,
 *    но целевой гекс должен быть свободен.
 * 3. Вычисление доступных клеток (Reachability) за текущий запас хода (Speed).
 * 4. Нахождение ближайшего шага в сторону цели, если цель за пределами одного хода
 *    (ключевая функция для жадного AI Зомби).
 */

struct PathResult
{
    bool reachable{false};
    int distance{999};
    std::vector<int> path; // Последовательность гексов от старта (исключая) до финиша (включая)
};

class Pathfinder
{
public:
    using ObstacleMask = std::bitset<NUM_BATTLE_HEXES>;

    /**
     * @brief Найти все гексы, достижимые отрядом за текущий ход (с учетом скорости).
     * @param startHex Начальный гекс
     * @param speed Скорость отряда (очки передвижения)
     * @param isFlying Флаг летуна
     * @param blockedHexes Маска заблокированных гексов (препятствия, союзники, враги)
     * @return Вектор ID достижимых свободных гексов (включая стартовый)
     */
    static std::vector<int> getReachableHexes(
        int startHex,
        int speed,
        bool isFlying,
        const ObstacleMask& blockedHexes
    );

    /**
     * @brief Найти кратчайший путь между startHex и targetHex.
     * @param startHex Начальный гекс
     * @param targetHex Целевой гекс
     * @param isFlying Флаг летуна
     * @param blockedHexes Маска занятых гексов (targetHex может быть занят, если мы ищем путь вплотную)
     * @param allowBlockedTarget Если true, путь строится вплоть до targetHex, даже если targetHex в blockedHexes
     */
    static PathResult findPath(
        int startHex,
        int targetHex,
        bool isFlying,
        const ObstacleMask& blockedHexes,
        bool allowBlockedTarget = false
    );

    /**
     * @brief Найти кратчайшее расстояние в гексах (с учетом огибания препятствий).
     * @return Количество шагов или 999, если пути нет
     */
    static int getPathDistance(
        int startHex,
        int targetHex,
        bool isFlying,
        const ObstacleMask& blockedHexes,
        bool allowBlockedTarget = false
    );

    /**
     * @brief Найти клетку приземления/остановки на пути к targetHex в пределах скорости.
     * Если цель ближе скорости, останавливается рядом с целью.
     * Если цель дальше скорости, делает максимальное количество шагов в направлении цели.
     * Если путь полностью заблокирован, возвращает startHex.
     */
    static int getStepTowards(
        int startHex,
        int targetHex,
        int speed,
        bool isFlying,
        const ObstacleMask& blockedHexes
    );

    /**
     * @brief Найти лучший гекс для атаки ближнего боя по цели targetHex.
     * Проверяет всех свободных соседей вокруг targetHex и находит того, путь к которому кратчайший.
     * @return ID гекса атаки или -1, если все клетки вокруг заняты или недостижимы.
     */
    static int getBestMeleeAttackHex(
        int startHex,
        int targetHex,
        int speed,
        bool isFlying,
        const ObstacleMask& blockedHexes
    );
};

} // namespace BattleAI
