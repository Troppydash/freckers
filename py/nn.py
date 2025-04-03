import math
import os

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torch.utils.data import random_split
from torch.utils.data import DataLoader, random_split
from torch.utils.data import Dataset
import glob
import pickle
from torch.optim.lr_scheduler import ReduceLROnPlateau
import engine

device = "cuda" if torch.cuda.is_available() else "cpu"
input_size = 8 * 8 * 4
pk_file = "session6"
session = "session6"


def bitmask_to_array(mask: int):
    arr = []
    for i in range(64):
        if (mask & (1 << i)) > 0:
            arr.append(1)
        else:
            arr.append(0)

    return arr


def make_inputs(position: tuple[int, int, int, int, int]):
    lily, red, blue, turn, mode = position

    # make red
    out_red = []
    for i in range(64):
        for j in range(64):
            if lily & (1 << i) and red & (1 << j):
                out_red.append(1)
            else:
                out_red.append(0)

    # make blue, but reversed
    out_blue = []
    for i in range(64):
        for j in range(64):
            if lily & (1 << i) and blue & (1 << j):
                out_blue.append(1)
            else:
                out_blue.append(0)

    out_blue = list(reversed(out_blue))

    if turn == 0:
        return out_red + out_blue
    else:
        return out_blue + out_red


class FreckersDataset(Dataset):
    def __init__(self):
        X = []
        y = []

        for file in glob.glob(f'./sessions/{pk_file}/*.pk'):
            if file.endswith('_backup.pk'):
                continue

            # if not (os.path.basename(file).startswith(pk_file)):
            #     continue

            print(f'[dataset] loading {os.path.basename(file)}')

            try:
                with open(file, 'rb') as f:
                    dataset = pickle.load(f)
            except Exception as e:
                print(f'skipping: {e}')
                continue

            for i in range(len(dataset.positions)):
                positions = dataset.positions[i]
                outcome = dataset.outcomes[i]
                lily = bitmask_to_array(positions[0])
                red = bitmask_to_array(positions[1])
                blue = list(reversed(bitmask_to_array(positions[2])))
                turn = positions[3]
                moves = positions[4]
                # eval should be for the moving player
                eval = dataset.evals[i]

                if turn == 0:
                    X.append([*lily, *red, *list(reversed(lily)), *blue])
                else:
                    X.append([*list(reversed(lily)), *blue, *lily, *red])

                if outcome == turn:
                    score = 1
                elif outcome == 1 - turn:
                    score = -1
                else:
                    score = 0

                # our score is in the perspective of the moving player

                lambda_ = 0.7
                if eval > 100000:
                    normalized = 1
                elif eval < -100000:
                    normalized = -1
                else:
                    normalized = 2 / (1 + math.exp(-eval / 1000)) - 1
                avg = lambda_ * score + (1 - lambda_) * normalized
                y.append([avg])

        self.X = torch.tensor(X, dtype=torch.float32).reshape(-1, input_size)
        self.y = torch.tensor(y, dtype=torch.float32)

    def __len__(self):
        return len(self.y)

    def __getitem__(self, index):
        return self.X[index, :], self.y[index]


class FreckersNeuralNetwork(nn.Module):
    def __init__(self):
        super().__init__()
        self.layer1 = nn.Linear(8 * 8 * 2, 64, dtype=torch.float32)
        self.layer2 = nn.Linear(64*2, 32, dtype=torch.float32)
        self.layer3 = nn.Linear(32, 16, dtype=torch.float32)
        self.layer4 = nn.Linear(16, 1, dtype=torch.float32)

    def forward(self, x):
        x = torch.flatten(x, 1)
        x1 = self.layer1(torch.concat((x[:, :64], x[:, 64:2 * 64]), dim=1))
        x2 = self.layer1(torch.concat((x[:, 2 * 64:3 * 64], x[:, 3 * 64:]), dim=1))
        x = F.relu(torch.concat((x1, x2), dim=1)).clamp(max=1)
        x = F.relu(self.layer2(x)).clamp(max=1)
        x = F.relu(self.layer3(x)).clamp(max=1)
        x = (self.layer4(x))

        return x


def train(config):
    net = FreckersNeuralNetwork().to(device)

    criterion = nn.MSELoss()
    optimizer = optim.AdamW(net.parameters(), lr=0.005, eps=1e-8)
    scheduler = ReduceLROnPlateau(optimizer, 'min')
    # optimizer = optim.SGD(
    #     net.parameters(), lr=config["lr"], momentum=config["momentum"]
    # )

    dataset = FreckersDataset()
    train_dataset, test_dataset = random_split(dataset, [0.8, 0.2])
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
                p.data.clamp_(-2.0, 2.0)

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

        if epoch % 5 == 0:
            torch.save(net.state_dict(), f"./models/{session}/model_{epoch}.pt")

        print(
            f"[train] train_loss {running_loss / epoch_steps:.4}, test_loss {test_loss / test_steps:.4}"
        )

        # scheduler.step(test_loss / test_steps)
        # print(f"new lr {scheduler.get_last_lr()}")




# used to be 64, 32, 16, with all positions


if __name__ == '__main__':
    train(
        {
            "lr": 0.00004,
            "batch_size": 4096 * 16,
            "test_batch": 4096 * 32,
            "momentum": 0.9,
        }
    )
