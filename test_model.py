import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

import torch
from model import TwoLevelBattleAI

def test_pipeline():
    print("Initializing TwoLevelBattleAI...")
    model = TwoLevelBattleAI()
    model.eval()

    # Simulate batch of 2 battles
    # 85 features: army composition (14 stacks * 6 stats + 1 balance ratio)
    batch_size = 2
    army_features = torch.randn(batch_size, 85)

    # 4 candidate tactical actions for current active stack, each with 16 features
    num_candidates = 4
    candidate_actions = torch.randn(batch_size, num_candidates, 16)

    print(f"Running forward pass: Batch={batch_size}, Candidates={num_candidates}...")
    roles, value, scores = model(army_features, candidate_actions)

    print("\n--- Output shapes & predictions ---")
    print("Role logits shape:", roles.shape) # [2, 7, 4] -> 4 role probabilities for each of 7 ally stacks
    print("Value shape:", value.shape)       # [2, 1]    -> estimated victory probability in [-1, 1]
    print("Action scores shape:", scores.shape) # [2, 4] -> score for each of the 4 candidate moves

    best_action, _, _ = model.get_action(army_features, candidate_actions)
    print("Selected best candidate index for each batch:", best_action.tolist())

    # Test ONNX Export (for C++ VCMI inference)
    onnx_path = "two_level_ai.onnx"
    print(f"\nExporting model to ONNX: {onnx_path}...")
    torch.onnx.export(
        model,
        (army_features, candidate_actions),
        onnx_path,
        input_names=["army_features", "candidate_action_features"],
        output_names=["role_logits", "value", "action_scores"],
        dynamic_axes={
            "army_features": {0: "batch_size"},
            "candidate_action_features": {0: "batch_size", 1: "num_candidates"},
            "role_logits": {0: "batch_size"},
            "value": {0: "batch_size"},
            "action_scores": {0: "batch_size", 1: "num_candidates"}
        },
        opset_version=17
    )
    print("ONNX export succeeded! Ready for C++ ONNX Runtime integration in VCMI.")

if __name__ == "__main__":
    test_pipeline()
