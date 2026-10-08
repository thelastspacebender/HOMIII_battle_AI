#pragma once

#include "Director.h"
#include "TacticalSearch.h"
#include "Role.h"

namespace BattleAI
{

class VCMIAdapter
{
public:
    VCMIAdapter();

    MoveEvaluation decideAction(
        const std::vector<StackSnapshot>& allyStacks,
        const std::vector<StackSnapshot>& enemyStacks,
        const StackSnapshot& activeStack,
        const std::vector<int>& reachableHexes,
        const std::vector<int>& corpseHexes,
        int nextStackIdInQueue = -1,
        bool enemyCanResurrect = false
    );

    MoveEvaluation decideActionNeural(
        const std::vector<StackSnapshot>& allyStacks,
        const std::vector<StackSnapshot>& enemyStacks,
        const StackSnapshot& activeStack,
        const std::vector<int>& reachableHexes,
        const std::vector<int>& obstacleHexes = {},
        const std::vector<int>& corpseHexes = {},
        int nextStackIdInQueue = -1,
        bool isAgainstBattleAI = true,
        int roundNumber = 1
    );

    const Director& getDirector() const { return director; }
    DynamicWeights getLastWeights() const { return lastWeights; }

private:
    Director director;
    TacticalSearch searcher;
    DynamicWeights lastWeights;
};

} // namespace BattleAI
