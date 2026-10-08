#include <iostream>
#include <iomanip>
#include <cassert>
#include "NESTrainer.h"

using namespace BattleAI;

int main()
{
    std::cout << "=========================================================" << std::endl;
    std::cout << "  HoMM3 / VCMI NES + Adam C++20 Headless Training Session" << std::endl;
    std::cout << "  Scenario: 15 Archers + 2x1 Pikeman vs 35 Walking Dead   " << std::endl;
    std::cout << "=========================================================" << std::endl;

    NeuralWeightModulator brain;
    std::mt19937 evalRng(1234);

    // Baseline evaluation before training
    std::cout << "\n[1] Evaluating Baseline Agent (Before Training)..." << std::endl;
    BattleStats baseline = NESTrainer::simulateSingleBattle(brain, evalRng, true);
    std::cout << "  Baseline Result: " << (baseline.victory ? "VICTORY" : "DEFEAT")
              << " | Archers Left: " << baseline.archersRemaining << "/15"
              << " | Pikemen Left: " << baseline.pikemenRemaining << "/2"
              << " | Zombies Dead: " << baseline.zombiesKilled << "/35"
              << " | Reward: " << baseline.reward
              << " | Zero Loss: " << (baseline.zeroLossArchers ? "YES" : "NO") << std::endl;

    // Run 10 training epochs
    std::cout << "\n[2] Starting NES + Adam Optimization (10 Epochs)..." << std::endl;
    // PopSize = 8 for fast headless verification
    NESTrainer trainer(brain, 0.05f, 0.1f, 8);

    for (int epoch = 1; epoch <= 10; ++epoch)
    {
        TrainingEpochStats stats = trainer.trainEpoch(epoch);
        std::cout << "  Epoch " << std::setw(2) << epoch
                  << " | Reward: " << std::setw(6) << std::fixed << std::setprecision(1) << stats.meanReward
                  << " | WinRate: " << std::setw(4) << std::fixed << std::setprecision(2) << stats.winRate * 100.0f << "%"
                  << " | ZeroLoss: " << std::setw(4) << std::fixed << std::setprecision(2) << stats.zeroLossRate * 100.0f << "%"
                  << " | BaitW: " << std::fixed << std::setprecision(2) << stats.currentWeights.w_bait_diversion
                  << " | KiteW: " << std::fixed << std::setprecision(2) << stats.currentWeights.w_kite_distance << std::endl;

        assert(!std::isnan(stats.meanReward));
    }

    // Save trained weights
    bool saved = brain.saveWeights("trained_weights.bin");
    assert(saved);
    std::cout << "\n[3] Trained weights successfully saved to 'trained_weights.bin'." << std::endl;

    std::cout << "=== HEADLESS TRAINING PIPELINE TEST PASSED! ===" << std::endl;
    return 0;
}
