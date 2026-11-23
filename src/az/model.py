"""Zero-C SE-ResNet Neural Network (Policy + WDL Value + Moves-Left Auxiliary Head).

Designed for maximum FP16 Tensor Core throughput on single-GPU setups (e.g. RTX 2060 SUPER).
"""
import torch
import torch.nn as nn
import torch.nn.functional as F

from mcts_bindings import AZ_PLANES, AZ_NUM_ACTIONS

PRESET_ARCHS = {
    "tiny": (4, 64),      # ~380k params  (ultra-fast experimentation)
    "standard": (6, 96),  # ~1.1M params  (ideal single-GPU sweet spot)
    "strong": (8, 128),   # ~2.5M params  (stronger overnight runs)
}


class SEBlock(nn.Module):
    """Squeeze-and-Excitation Residual Block (global board context per residual step)."""

    def __init__(self, channels: int, se_ratio: int = 4):
        super().__init__()
        self.conv1 = nn.Conv2d(channels, channels, kernel_size=3, padding=1, bias=False)
        self.bn1 = nn.BatchNorm2d(channels)
        self.conv2 = nn.Conv2d(channels, channels, kernel_size=3, padding=1, bias=False)
        self.bn2 = nn.BatchNorm2d(channels)
        se_hidden = max(16, channels // se_ratio)
        self.se_fc1 = nn.Linear(channels, se_hidden)
        self.se_fc2 = nn.Linear(se_hidden, channels * 2)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        h = F.silu(self.bn1(self.conv1(x)))
        h = self.bn2(self.conv2(h))
        # Global average pooling -> scale + shift gating (Leela/KataGo SE style)
        g = h.mean(dim=(2, 3))
        g = self.se_fc2(F.silu(self.se_fc1(g)))
        scale, bias = g.chunk(2, dim=1)
        scale = torch.sigmoid(scale).unsqueeze(-1).unsqueeze(-1)
        bias = bias.unsqueeze(-1).unsqueeze(-1)
        return F.silu(x + h * scale + bias)


class ZeroCNet(nn.Module):
    """Pure tabula rasa AlphaZero/KataGo SE-ResNet with Policy, WDL, and Moves-Left heads."""

    def __init__(self, num_blocks: int = 6, channels: int = 96):
        super().__init__()
        self.num_blocks = num_blocks
        self.channels = channels

        self.stem = nn.Sequential(
            nn.Conv2d(AZ_PLANES, channels, kernel_size=3, padding=1, bias=False),
            nn.BatchNorm2d(channels),
            nn.SiLU(inplace=True),
        )
        self.tower = nn.ModuleList([SEBlock(channels) for _ in range(num_blocks)])

        # 1. Policy head -> [B, 73, 8, 8] reshaped to [B, 64 * 73 = 4672]
        self.pol_conv1 = nn.Conv2d(channels, channels, kernel_size=3, padding=1, bias=False)
        self.pol_bn1 = nn.BatchNorm2d(channels)
        self.pol_conv2 = nn.Conv2d(channels, 73, kernel_size=1, bias=True)

        # 2. WDL Value head -> [B, 3] logits for (Win, Draw, Loss)
        self.val_conv = nn.Conv2d(channels, 32, kernel_size=1, bias=False)
        self.val_bn = nn.BatchNorm2d(32)
        self.val_fc1 = nn.Linear(32 * 64, 128)
        self.val_fc2 = nn.Linear(128, 3)

        # 3. Auxiliary Moves-Left head -> [B] scalar (remaining plies / 100.0)
        self.ml_conv = nn.Conv2d(channels, 8, kernel_size=1, bias=False)
        self.ml_bn = nn.BatchNorm2d(8)
        self.ml_fc1 = nn.Linear(8 * 64, 64)
        self.ml_fc2 = nn.Linear(64, 1)

    def forward(self, x: torch.Tensor):
        h = self.stem(x)
        for blk in self.tower:
            h = blk(h)

        # Policy: [B, 73, 8, 8] -> permute to [B, 8, 8, 73] -> flatten [B, 4672]
        # Note: C board square sq = rank * 8 + file matches [rank, file] spatial layout!
        p = F.silu(self.pol_bn1(self.pol_conv1(h)))
        p = self.pol_conv2(p)  # [B, 73, 8, 8]
        policy_logits = p.permute(0, 2, 3, 1).contiguous().view(x.size(0), AZ_NUM_ACTIONS)

        # WDL Value
        v = F.silu(self.val_bn(self.val_conv(h))).flatten(1)
        v = F.silu(self.val_fc1(v))
        wdl_logits = self.val_fc2(v)

        # Moves-Left (positive via softplus)
        m = F.silu(self.ml_bn(self.ml_conv(h))).flatten(1)
        m = F.silu(self.ml_fc1(m))
        moves_left = F.softplus(self.ml_fc2(m)).squeeze(-1)

        return policy_logits, wdl_logits, moves_left


def count_parameters(model: nn.Module) -> int:
    return sum(p.numel() for p in model.parameters() if p.requires_grad)


def save_checkpoint(path: str, model: ZeroCNet, optimizer=None, meta: dict = None):
    payload = {
        "num_blocks": model.num_blocks,
        "channels": model.channels,
        "state_dict": model.state_dict(),
        "meta": meta or {},
    }
    if optimizer is not None:
        payload["optimizer"] = optimizer.state_dict()
    torch.save(payload, path)


def load_checkpoint(path: str, device: torch.device):
    ckpt = torch.load(path, map_location=device, weights_only=False)
    model = ZeroCNet(ckpt["num_blocks"], ckpt["channels"]).to(device)
    model.load_state_dict(ckpt["state_dict"])
    return model, ckpt
