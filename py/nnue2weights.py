import torch
import numpy as np

import nn2
from nn2 import NNUE2

base = './models/session74_4/'
file = f"{base}model_78.pt"
model = torch.load(file)

HIDDEN_SIZE = 256
QA = 255
QB = 64


def export_model(model: nn2.NNUE2, filename="model.bin"):
    with open(filename, "wb") as f:
        # 1. Feature Weights: l0.weight (256, 192) -> (192, 256)
        # Quantization: weight * QA
        l0_weight = model.l0.weight.data.t().contiguous()
        l0_weight_int = (l0_weight * QA).round().clamp(-32768, 32767).numpy().astype(np.int16)
        f.write(l0_weight_int.tobytes())

        # 2. Feature Bias: l0.bias (256)
        # Quantization: bias * QA
        l0_bias = model.l0.bias.data
        l0_bias_int = (l0_bias * QA).round().clamp(-32768, 32767).numpy().astype(np.int16)
        f.write(l0_bias_int.tobytes())

        # 3. Output Weights: l1.weight (1, 512) -> (512)
        # Quantization: weight * QB
        l1_weight = model.l1.weight.data.t().contiguous()
        l1_weight_int = (l1_weight * QB).round().clamp(-32768, 32767).numpy().astype(np.int16)
        f.write(l1_weight_int.tobytes())

        # 4. Output Bias: l1.bias (1)
        # Quantization: bias * QA * QB
        l1_bias = model.l1.bias.data
        l1_bias_scaled = (l1_bias * QA * QB).round()
        l1_bias_int = l1_bias_scaled.clamp(-32768, 32767).numpy().astype(np.int16)
        f.write(l1_bias_int.tobytes())

    print(f"Model exported successfully to {filename}")


weights = torch.load(file, weights_only=True)
m = NNUE2()
m.load_state_dict(weights)

export_model(m, 'new.bin')
