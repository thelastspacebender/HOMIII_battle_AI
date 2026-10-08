"""
Two-Level Hierarchical Combat AI for Heroes of Might and Magic III (VCMI)
Level 1: Director Network (Macro Strategy, Role Allocation, Position Value)
Level 2: Tactical Scorer (Micro Action Evaluation over Candidate Patterns)
"""

import torch
import torch.nn as nn
import torch.nn.functional as F

class DirectorNetwork(nn.Module):
    """
    Macro Policy & Value Network.
    Evaluates army composition, assigns roles to ally stacks, and estimates win probability.
    Input: [Batch, 85] (14 stacks * 6 features + 1 global ratio)
    Output:
      - role_logits: [Batch, 7, 4] (CORE=0, BODYGUARD=1, BAIT=2, INTERCEPTOR=3)
      - state_value: [Batch, 1] (Expected game value / win probability estimate in [-1, 1])
      - context_emb: [Batch, 64] (Shared representation passed to tactical executor)
    """
    def __init__(self, in_features: int = 85, emb_dim: int = 64):
        super().__init__()
        self.backbone = nn.Sequential(
            nn.Linear(in_features, 128),
            nn.LayerNorm(128),
            nn.ReLU(),
            nn.Linear(128, emb_dim),
            nn.LayerNorm(emb_dim),
            nn.ReLU()
        )
        # Role head: for each of the 7 ally stacks, output logits over 4 tactical roles
        self.role_head = nn.Linear(emb_dim, 7 * 4)
        
        # Value head: scalar estimate of victory likelihood / army advantage
        self.value_head = nn.Sequential(
            nn.Linear(emb_dim, 32),
            nn.ReLU(),
            nn.Linear(32, 1),
            nn.Tanh()
        )

    def forward(self, army_features: torch.Tensor):
        emb = self.backbone(army_features)  # [B, 64]
        b = army_features.size(0)
        roles = self.role_head(emb).view(b, 7, 4)  # [B, 7, 4]
        val = self.value_head(emb)  # [B, 1]
        return roles, val, emb


class TacticalScorer(nn.Module):
    """
    Micro Policy Network.
    Scores a set of candidate tactical actions for the currently active stack.
    Instead of selecting blindly from 165 hexes, it evaluates 3-5 structured candidates:
    [BLOCK_SHOOTER, STRIP_RETALIATION, BODYGUARD_SCREEN, BAIT_SACRIFICE,
     KITE_RETREAT, STRIKE_PRIMARY, WAIT_TEMPO, TURTLE_DEFEND, SPELL_CAST]
    
    Inputs:
      - context_emb: [B, 64] from Director
      - action_features: [B, num_candidates, 16] features of each proposed move
    Output:
      - scores: [B, num_candidates] (logit scores for each candidate move)
    """
    def __init__(self, context_dim: int = 64, action_dim: int = 16):
        super().__init__()
        self.scorer = nn.Sequential(
            nn.Linear(context_dim + action_dim, 64),
            nn.ReLU(),
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Linear(32, 1)
        )

    def forward(self, context_emb: torch.Tensor, action_features: torch.Tensor):
        # context_emb: [B, 64] -> broadcast to [B, num_candidates, 64]
        b, num_cand, _ = action_features.shape
        ctx_expanded = context_emb.unsqueeze(1).expand(b, num_cand, -1)
        
        # Concatenate context with action features: [B, num_cand, 80]
        combined = torch.cat([ctx_expanded, action_features], dim=-1)
        scores = self.scorer(combined).squeeze(-1)  # [B, num_cand]
        return scores


class TwoLevelBattleAI(nn.Module):
    """
    Combined Hierarchical Model ready for Training and ONNX Export.
    """
    def __init__(self):
        super().__init__()
        self.director = DirectorNetwork()
        self.tactical_scorer = TacticalScorer()

    def forward(self, army_features: torch.Tensor, candidate_action_features: torch.Tensor):
        """
        army_features: [B, 85]
        candidate_action_features: [B, num_candidates, 16]
        """
        role_logits, value, context = self.director(army_features)
        action_scores = self.tactical_scorer(context, candidate_action_features)
        return role_logits, value, action_scores

    def get_action(self, army_features: torch.Tensor, candidate_action_features: torch.Tensor, deterministic: bool = True):
        role_logits, value, action_scores = self.forward(army_features, candidate_action_features)
        if deterministic:
            best_action_idx = torch.argmax(action_scores, dim=-1)
        else:
            probs = F.softmax(action_scores, dim=-1)
            dist = torch.distributions.Categorical(probs)
            best_action_idx = dist.sample()
        return best_action_idx, role_logits, value
