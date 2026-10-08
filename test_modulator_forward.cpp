#include <iostream>
#include <chrono>
#include <cassert>
#include "NeuralWeightModulator.h"

using namespace BattleAI;

void testModulatorParametersAndConditioning()
{
    std::cout << "[TEST] Running testModulatorParametersAndConditioning..." << std::endl;
    NeuralWeightModulator modulator;

    std::cout << "  Modulator parameter count: " << modulator.getParameterCount() << " floats." << std::endl;
    assert(modulator.getParameterCount() == 336464);

    BattlefieldTensor tensor;
    // Condition 1: against BattleAI (Bot)
    DynamicWeights wBot = modulator.predictWeights(tensor, true);
    std::cout << "  Bot mode: w_bait=" << wBot.w_bait_diversion 
              << ", w_kite=" << wBot.w_kite_distance 
              << ", w_screen=" << wBot.w_screen_core << std::endl;
    assert(wBot.w_bait_diversion > wBot.w_screen_core); // Exploits bot greediness!

    // Condition 2: against Human
    DynamicWeights wHuman = modulator.predictWeights(tensor, false);
    std::cout << "  Human mode: w_bait=" << wHuman.w_bait_diversion 
              << ", w_kite=" << wHuman.w_kite_distance 
              << ", w_screen=" << wHuman.w_screen_core << std::endl;
    assert(wHuman.w_screen_core > wHuman.w_bait_diversion); // Box defense against human!

    std::cout << "  -> Passed: Split conditioning matrix activates distinct tactical strategies." << std::endl;
}

void testSerialization()
{
    std::cout << "[TEST] Running testSerialization..." << std::endl;
    NeuralWeightModulator mod1;
    std::string testPath = "test_weights.bin";

    bool saved = mod1.saveWeights(testPath);
    assert(saved);

    NeuralWeightModulator mod2;
    // Corrupt mod2 weights
    std::vector<float> zeros(mod2.getParameterCount(), 0.0f);
    mod2.setFlatWeights(zeros);

    bool loaded = mod2.loadWeights(testPath);
    assert(loaded);

    auto flat1 = mod1.getFlatWeights();
    auto flat2 = mod2.getFlatWeights();
    assert(flat1.size() == flat2.size());
    for (size_t i = 0; i < flat1.size(); ++i)
    {
        assert(flat1[i] == flat2[i]);
    }
    std::cout << "  -> Passed: Binary serialization and deserialization are 100% bit-exact." << std::endl;
}

void testInferenceSpeed()
{
    std::cout << "[TEST] Running testInferenceSpeed..." << std::endl;
    NeuralWeightModulator modulator;
    BattlefieldTensor tensor;

    // Warm-up
    for (int i = 0; i < 10; ++i) modulator.predictWeights(tensor, true);

    constexpr int RUNS = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < RUNS; ++i)
    {
        volatile auto w = modulator.predictWeights(tensor, true);
        (void)w;
    }
    auto end = std::chrono::high_resolution_clock::now();
    double totalUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double avgUs = totalUs / RUNS;

    std::cout << "  Average inference time per turn: " << avgUs << " microseconds." << std::endl;
    assert(avgUs < 500.0); // Ultra-fast microsecond inference on Zen 4 CPU

    std::cout << "  -> Passed: Real-time inference speed benchmark." << std::endl;
}

int main()
{
    std::cout << "=== HoMM3 / VCMI Neural Weight Modulator Tests ===" << std::endl;
    testModulatorParametersAndConditioning();
    testSerialization();
    testInferenceSpeed();
    std::cout << "=== ALL NEURAL MODULATOR TESTS PASSED! ===" << std::endl;
    return 0;
}
