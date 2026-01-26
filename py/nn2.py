import glob
import math
import os
import pickle
import random

import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import DataLoader, random_split
from torch.utils.data import Dataset

import engine

HIDDEN_SIZE = 256
SCALE = 40 * 100
INPUT_SIZE = 4 * 64
device = 'cuda'
# data file
pk_file = 'session6'
# model file
session = 'session74_4'


def screlu(x):
    return torch.square(torch.clamp(x, 0, 1))


def sigmoid(x):
    return 1 / (1 + math.exp(-x))


class NNUE2(nn.Module):
    def __init__(self):
        super().__init__()
        self.l0 = nn.Linear(64 * 2, HIDDEN_SIZE, dtype=torch.float32)
        self.l1 = nn.Linear(2 * HIDDEN_SIZE, 1, dtype=torch.float32)

    def forward(self, x):
        x = torch.flatten(x, 1)

        # x shape is [side2move, lily, flip_notside2move, fliplily]
        stm = x[:, :2 * 64]
        ntm = x[:, 2 * 64:]

        stm_acc = screlu(self.l0(stm))
        ntm_acc = screlu(self.l0(ntm))
        return self.l1(torch.concat((stm_acc, ntm_acc), dim=1))


def sigmoid_loss(pred, y):
    return torch.mean(torch.pow(torch.abs((torch.tanh(pred) - y)), 2.4))


# def bitmask_to_array(mask: int):
#     arr = []
#     for i in range(64):
#         if (mask & (1 << i)) > 0:
#             arr.append(1)
#         else:
#             arr.append(0)
#
#     return arr

def bitmask_to_array(mask: int):
    return np.unpackbits(
        np.frombuffer(mask.to_bytes(8, 'little'), dtype=np.uint8)
    )




def flip_array_np(arr):
    """
    arr: 1D array of length 64
    returns: flipped array using i ^ 56 mapping
    """
    idx = np.arange(64) ^ 56
    return arr[idx]

# def flip_array(arr: list[int]):
#     out = [0] * 64
#     for i in range(64):
#         out[i ^ 56] = arr[i]
#
#     return out


class FreckersDataset(Dataset):
    def __init__(self):
        X = []
        y = []

        for file in glob.glob(f'./sessions/{pk_file}/*.pk'):
            if file.endswith('_backup.pk'):
                continue

            print(f'[dataset] loading {os.path.basename(file)}')

            try:
                with open(file, 'rb') as f:
                    dataset = pickle.load(f)
            except Exception as e:
                print(f'skipping: {e}')
                continue

            skipped = 0
            for i in range(len(dataset.positions)):
                positions = dataset.positions[i]
                outcome = dataset.outcomes[i]
                lily = bitmask_to_array(positions[0])
                red = bitmask_to_array(positions[1])
                blue = bitmask_to_array(positions[2])
                turn = positions[3]
                moves = positions[4]

                eval = dataset.evals[i]

                # skip condition
                if abs(moves) < 4 or random.random() > 0.75:
                    skipped += 1
                    continue

                if turn == 0:
                    # if red
                    X.append(np.concatenate([red, lily, flip_array_np(blue), flip_array_np(lily)]))
                else:
                    # if blue
                    X.append(np.concatenate([flip_array_np(blue), flip_array_np(lily), red, lily]))

                if outcome == turn:
                    game_result = 1
                elif outcome == 1 - turn:
                    game_result = -1
                else:
                    game_result = 0

                # our score is in the perspective of the moving player
                wdl = 0.85
                if eval > 100000:
                    other = 1
                elif eval < -100000:
                    other = -1
                else:
                    other = math.tanh(eval / SCALE)
                target = wdl * game_result + (1 - wdl) * other
                y.append([target])

            print(f"skipped {skipped / len(dataset.positions) * 100:.2f}%")
            # break

        self.X = torch.tensor(X, dtype=torch.float32).reshape(-1, INPUT_SIZE)
        self.y = torch.tensor(y, dtype=torch.float32)

    def __len__(self):
        return len(self.y)

    def __getitem__(self, index):
        return self.X[index, :], self.y[index]


def train(config):
    net = NNUE2().to(device)

    criterion = sigmoid_loss
    optimizer = torch.optim.AdamW(net.parameters())

    dataset = FreckersDataset()
    train_dataset, test_dataset = random_split(dataset, [0.95, 0.05])
    print(f'[train] train_dataset {len(train_dataset)}, using {device}')

    train_dataloader = DataLoader(
        train_dataset, batch_size=int(config["batch_size"]), shuffle=True, num_workers=1
    )
    test_dataloader = DataLoader(
        test_dataset, batch_size=int(config["batch_size"]), shuffle=True, num_workers=1
    )

    xs = []
    ys = []

    for epoch in range(0, 10000):
        print(f"\n[train] running epoch {epoch}")
        # train
        net.train()
        running_loss = 0.0
        epoch_steps = 0
        for i, (X, y) in enumerate(train_dataloader, 0):
            X, y = X.to(device), y.to(device)
            optimizer.zero_grad()

            pred = net(X)
            loss = criterion(pred, y)
            loss.backward()
            optimizer.step()
            for p in net.parameters():
                p.data.clamp_(-1.98, 1.98)

            running_loss += loss.sum().item()
            epoch_steps += 1

            if i % 50 == 49:
                print(
                    "[train] [%d, %5d] loss: %.3f"
                    % (epoch + 1, i + 1, running_loss / epoch_steps)
                )

        net.eval()
        # validation
        test_loss = 0.0
        test_steps = 0
        for i, (X, y) in enumerate(test_dataloader, 0):
            X, y = X.to(device), y.to(device)
            with torch.no_grad():
                pred = net(X)
                test_loss += criterion(pred, y).sum().item()
                test_steps += 1

        xs.append(len(xs))
        ys.append((running_loss / epoch_steps, test_loss / test_steps))
        import matplotlib.pyplot as plt

        fig, ax = plt.subplots(nrows=1, ncols=1)
        ax.plot(xs, ys)
        fig.savefig(f"loss/{session}_loss.png", bbox_inches="tight")
        plt.close(fig)

        if epoch % 2 == 0:
            torch.save(net.state_dict(), f"./models/{session}/model_{epoch}.pt")

        print(
            f"[train] train_loss {running_loss / epoch_steps:.4}, test_loss {test_loss / test_steps:.4}"
        )


if __name__ == '__main__':
    train(
        {
            "batch_size": 16384 * 2,
            "test_batch": 16384 * 2,
        }
    )
