import matplotlib.pyplot as plt
import pandas as pd
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import DataLoader, random_split
from torch.utils.data import Dataset
import torch.optim as optim

import numpy as np
import random
from functools import partial
import os
import tempfile
from pathlib import Path
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torch.utils.data import random_split
import torchvision
import torchvision.transforms as transforms
import glob
import math
import pickle

device = "cuda" if torch.cuda.is_available() else "cpu"
input_size = 8 * 8 * 3
session = "session0"

def bitmask_to_array(mask: int):
    arr = []
    for i in range(64):
        if (mask & (1 << i)) > 0:
            arr.append(1)
        else:
            arr.append(0)

    return arr

class FreckersDataset(Dataset):
    def __init__(self):
        X = []
        y = []

        for file in glob.glob('./sessions/*.pk'):
            if file.endswith('_backup.pk'):
                continue

            if not os.path.basename(file).startswith(session):
                continue

            print(f'[dataset] loading {os.path.basename(file)}')

            with open(file, 'rb') as f:
                dataset = pickle.load(f)

            for i in range(len(dataset.positions)):
                X.append([*bitmask_to_array(dataset.positions[i][0]), *bitmask_to_array(dataset.positions[i][1]), *bitmask_to_array(dataset.positions[i][2])])

                outcome = dataset.outcomes[i]
                score = 0.5
                if outcome == 0:
                    score = 1
                elif outcome == 1:
                    score = 0

                y.append([score])

        self.X = torch.tensor(X, dtype=torch.float16).reshape(-1, input_size)
        self.y = torch.tensor(y, dtype=torch.float16)

    def __len__(self):
        return len(self.y)

    def __getitem__(self, index):
        return self.X[index, :], self.y[index]


class FreckersNeuralNetwork(nn.Module):
    def __init__(self):
        super().__init__()
        self.layer1 = nn.Linear(input_size, 64, dtype=torch.float16)
        self.layer2 = nn.Linear(64, 32, dtype=torch.float16)
        self.layer3 = nn.Linear(32, 16, dtype=torch.float16)
        self.layer4 = nn.Linear(16, 1, dtype=torch.float16)

    def forward(self, x):
        x = torch.flatten(x, 1)
        x = F.relu(self.layer1(x)).clamp(max=1)
        x = F.relu(self.layer2(x)).clamp(max=1)
        x = F.relu(self.layer3(x)).clamp(max=1)
        x = (self.layer4(x)).clamp(min=0, max=1)

        return x


def train(config):
    net = FreckersNeuralNetwork().to(device)

    criterion = nn.MSELoss()
    # optimizer = optim.Adam(net.parameters(), lr=0.005, eps=1e-8)
    optimizer = optim.SGD(
        net.parameters(), lr=config["lr"], momentum=config["momentum"]
    )

    dataset = FreckersDataset()
    train_dataset, test_dataset = random_split(dataset, [0.8, 0.2])
    print(f'[train] train_dataset {len(train_dataset)}, using {device}')

    train_dataloader = DataLoader(
        train_dataset, batch_size=int(config["batch_size"]), shuffle=True, num_workers=6
    )
    test_dataloader = DataLoader(
        test_dataset, batch_size=int(config["batch_size"]), shuffle=True, num_workers=6
    )


    xs = []
    ys = []

    for epoch in range(0, 10000):
        print(f"\n[train] running epoch {epoch}")
        # train
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
                p.data.clamp_(-1.0, 1.0)

            running_loss += loss.sum().item()
            epoch_steps += 1

            if i % 50 == 49:
                print(
                    "[train] [%d, %5d] loss: %.3f"
                    % (epoch + 1, i + 1, running_loss / epoch_steps)
                )

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

        if epoch % 10 == 0:
            torch.save(net.state_dict(), f"./models/{session}/model_{epoch}.pt")

        print(
            f"[train] train_loss {running_loss / epoch_steps:.4}, test_loss {test_loss / test_steps:.4}"
        )


# used to be 64, 32, 16, with all positions


if __name__ == '__main__':
    train(
        {
            "lr": 0.01,
            "batch_size": 4096,
            "test_batch": 8096 * 2,
            "momentum": 0.9,
        }
    )
