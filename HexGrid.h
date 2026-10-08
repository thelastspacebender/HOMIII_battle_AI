#pragma once

#include <cstdint>
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>

namespace BattleAI
{

/**
 * @file HexGrid.h
 * @brief Математический фундамент гексагональной сетки HoMM3 / VCMI.
 * 
 * Параметры поля боя:
 * - 11 рядов (Y: 0..10)
 * - 17 колонок (X: 0..16):
 *     Колонка 0: Левый край (стартовая зона нападающего)
 *     Колонки 1..15: Основное игровое поле (165 активных гексов)
 *     Колонка 16: Правый край (стартовая зона защитника)
 * - Всего 187 гексов (номера 0..186).
 * 
 * Топология:
 * - Горизонтальная сетка с нечетным сдвигом (Odd-R layout):
 *   Нечетные строки (Y = 1, 3, 5, 7, 9) сдвинуты вправо на 0.5 шага.
 */

constexpr int BFIELD_WIDTH = 15;
constexpr int BFIELD_HEIGHT = 11;
constexpr int NUM_BATTLE_HEXES = BFIELD_WIDTH * BFIELD_HEIGHT; // 165

constexpr int PLAYABLE_MIN_X = 0;
constexpr int PLAYABLE_MAX_X = 14;
constexpr int PLAYABLE_MIN_Y = 0;
constexpr int PLAYABLE_MAX_Y = 10;
constexpr int NUM_PLAYABLE_HEXES = NUM_BATTLE_HEXES; // 165

/**
 * @brief 6 направлений движения по гексагональной сетке
 */
enum class HexDirection : int
{
    TOP_RIGHT = 0,
    RIGHT = 1,
    BOTTOM_RIGHT = 2,
    BOTTOM_LEFT = 3,
    LEFT = 4,
    TOP_LEFT = 5,
    NONE = -1
};

/**
 * @brief Кубические координаты (Cube Coordinates) для точного расчета расстояний.
 * Инвариант: q + r + s == 0.
 */
struct CubeCoord
{
    int q{0};
    int r{0};
    int s{0};

    constexpr CubeCoord() = default;
    constexpr CubeCoord(int q_, int r_, int s_) : q(q_), r(r_), s(s_) {}

    constexpr bool operator==(const CubeCoord& other) const
    {
        return q == other.q && r == other.r && s == other.s;
    }
};

/**
 * @brief Структура представления боевого гекса (BattleHex)
 */
struct BattleHex
{
    int id{-1}; // 0 .. 164
    int x{0};  // 0 .. 14 (Колонка)
    int y{0};  // 0 .. 10 (Ряд)

    constexpr BattleHex() = default;
    constexpr BattleHex(int id_) : id(id_), x(id_ >= 0 ? id_ % BFIELD_WIDTH : -1), y(id_ >= 0 ? id_ / BFIELD_WIDTH : -1) {}
    constexpr BattleHex(int x_, int y_) : id((x_ >= 0 && x_ < BFIELD_WIDTH && y_ >= 0 && y_ < BFIELD_HEIGHT) ? (y_ * BFIELD_WIDTH + x_) : -1), x(x_), y(y_) {}

    [[nodiscard]] constexpr bool isValid() const
    {
        return id >= 0 && id < NUM_BATTLE_HEXES && x >= 0 && x < BFIELD_WIDTH && y >= 0 && y < BFIELD_HEIGHT;
    }

    [[nodiscard]] constexpr bool isPlayable() const
    {
        return isValid();
    }

    [[nodiscard]] constexpr bool isBorder() const
    {
        return false;
    }

    [[nodiscard]] constexpr bool isOddRow() const
    {
        return (y & 1) != 0;
    }

    /**
     * @brief Преобразование Odd-R координат (x, y) в Cube координаты (q, r, s)
     */
    [[nodiscard]] constexpr CubeCoord toCube() const
    {
        int q = x - (y - (y & 1)) / 2;
        int r = y;
        int s = -q - r;
        return {q, r, s};
    }

    /**
     * @brief Создание BattleHex из Cube координат
     */
    static constexpr BattleHex fromCube(const CubeCoord& cube)
    {
        int col = cube.q + (cube.r - (cube.r & 1)) / 2;
        int row = cube.r;
        return BattleHex(col, row);
    }

    constexpr bool operator==(const BattleHex& other) const { return id == other.id; }
    constexpr bool operator!=(const BattleHex& other) const { return id != other.id; }
    constexpr bool operator<(const BattleHex& other) const { return id < other.id; }
};

/**
 * @brief Класс утилит для геометрии сетки боя HoMM3 / VCMI
 */
class HexGrid
{
public:
    /**
     * @brief Получить координаты соседа в заданном направлении.
     * Нечетные ряды (y % 2 != 0) сдвинуты вправо!
     */
    static constexpr BattleHex getNeighbor(const BattleHex& hex, HexDirection dir)
    {
        if (!hex.isValid()) return BattleHex(-1);

        int x = hex.x;
        int y = hex.y;
        bool isOdd = hex.isOddRow();

        switch (dir)
        {
        case HexDirection::RIGHT:
            return BattleHex(x + 1, y);
        case HexDirection::LEFT:
            return BattleHex(x - 1, y);
        case HexDirection::TOP_RIGHT:
            return BattleHex(isOdd ? (x + 1) : x, y - 1);
        case HexDirection::TOP_LEFT:
            return BattleHex(isOdd ? x : (x - 1), y - 1);
        case HexDirection::BOTTOM_RIGHT:
            return BattleHex(isOdd ? (x + 1) : x, y + 1);
        case HexDirection::BOTTOM_LEFT:
            return BattleHex(isOdd ? x : (x - 1), y + 1);
        default:
            return BattleHex(-1);
        }
    }

    /**
     * @brief Получить всех валидных соседей гекса (от 2 до 6 соседей)
     */
    static std::vector<BattleHex> getValidNeighbors(const BattleHex& hex, bool playableOnly = true)
    {
        std::vector<BattleHex> neighbors;
        neighbors.reserve(6);

        constexpr std::array<HexDirection, 6> allDirs = {
            HexDirection::TOP_RIGHT,
            HexDirection::RIGHT,
            HexDirection::BOTTOM_RIGHT,
            HexDirection::BOTTOM_LEFT,
            HexDirection::LEFT,
            HexDirection::TOP_LEFT
        };

        for (HexDirection dir : allDirs)
        {
            BattleHex n = getNeighbor(hex, dir);
            if (playableOnly ? n.isPlayable() : n.isValid())
            {
                neighbors.push_back(n);
            }
        }
        return neighbors;
    }

    /**
     * @brief Точное гексагональное расстояние между двумя гексами по формуле кубических координат:
     * dist = (|dq| + |dr| + |ds|) / 2
     */
    static constexpr int distance(const BattleHex& a, const BattleHex& b)
    {
        if (!a.isValid() || !b.isValid()) return 999;
        CubeCoord ca = a.toCube();
        CubeCoord cb = b.toCube();
        int dq = std::abs(ca.q - cb.q);
        int dr = std::abs(ca.r - cb.r);
        int ds = std::abs(ca.s - cb.s);
        return (dq + dr + ds) / 2;
    }

    /**
     * @brief Перегрузка для ID гексов
     */
    static constexpr int distance(int hexIdA, int hexIdB)
    {
        return distance(BattleHex(hexIdA), BattleHex(hexIdB));
    }

    /**
     * @brief Проверка, являются ли два гекса непосредственными соседями
     */
    static constexpr bool isAdjacent(const BattleHex& a, const BattleHex& b)
    {
        return distance(a, b) == 1;
    }

    /**
     * @brief Определение направления от гекса `from` к непосредственному соседу `to`
     */
    static HexDirection getDirection(const BattleHex& from, const BattleHex& to)
    {
        if (!isAdjacent(from, to)) return HexDirection::NONE;
        constexpr std::array<HexDirection, 6> allDirs = {
            HexDirection::TOP_RIGHT,
            HexDirection::RIGHT,
            HexDirection::BOTTOM_RIGHT,
            HexDirection::BOTTOM_LEFT,
            HexDirection::LEFT,
            HexDirection::TOP_LEFT
        };
        for (HexDirection dir : allDirs)
        {
            if (getNeighbor(from, dir) == to) return dir;
        }
        return HexDirection::NONE;
    }

    /**
     * @brief Преобразование гекса в экранные координаты Canvas (для рендеринга и отладки)
     * @param hex Исходный гекс
     * @param hexRadius Радиус описанной окружности гекса в пикселях
     * @return [pixelX, pixelY]
     */
    static std::pair<float, float> hexToPixel(const BattleHex& hex, float hexRadius)
    {
        float hexWidth = std::sqrt(3.0f) * hexRadius;
        float hexHeight = 2.0f * hexRadius;
        float xOffset = hex.isOddRow() ? (hexWidth * 0.5f) : 0.0f;
        float px = hex.x * hexWidth + xOffset;
        float py = hex.y * (hexHeight * 0.75f);
        return {px, py};
    }
};

} // namespace BattleAI
