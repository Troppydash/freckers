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
session = "session62"


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


def average_row(red, blue):
    # median row
    rows = 0
    for row in range(8):
        for col in range(8):
            if red[row * 8 + col]:
                rows += row

    for row in range(8):
        for col in range(8):
            if blue[row * 8 + col]:
                rows += 7 - row

    return rows // (2 * 6 * 2)


def display_board(mask):
    for row in range(8):
        for col in range(8):
            if mask[row * 8 + col]:
                print("1", end='')
            else:
                print("_", end='')
        print()


class FreckersDataset(Dataset):
    def __init__(self):
        X = []
        y = []
        pst = []

        pos = engine.Pos()
        for file in glob.glob(f'./sessions/{pk_file}/*.pk'):
            if file.endswith('_backup.pk'):
                continue

            # if not (os.path.basename(file).startswith(pk_file)):
            #     continue
            # if "v0" in file or "v1" in file:
            #     continue

            print(f'[dataset] loading {os.path.basename(file)}')

            try:
                with open(file, 'rb') as f:
                    dataset = pickle.load(f)
            except Exception as e:
                print(f'skipping: {e}')
                continue

            pct = 0
            for i in range(len(dataset.positions)):
                positions = dataset.positions[i]
                outcome = dataset.outcomes[i]
                lily = bitmask_to_array(positions[0])
                red = bitmask_to_array(positions[1])
                blue = list(reversed(bitmask_to_array(positions[2])))
                turn = positions[3]
                moves = positions[4]

                # avg_red = math.floor(average_row(red, blue))
                # avg_blue = math.floor((3-average_row(blue)))
                # assert 0 <= avg_red <= 3
                # assert 0 <= avg_blue <= 3
                avg = average_row(red, bitmask_to_array(positions[2]))
                # if avg == 1:
                #     print(avg)
                #     display_board(red)
                #     print()
                #     display_board(blue)
                #     print()
                assert 0 <= avg <= 3
                pst.append([avg])

                # pos.of(positions[0], positions[1], positions[2], turn, 0)
                # if pos.has_jumps:
                #     # skip jump positions
                #     pct += 1
                #     continue

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

                lambda_ = 0.8
                if eval > 10000:
                    normalized = 1
                elif eval < -10000:
                    normalized = -1
                else:
                    normalized = 2 / (1 + math.exp(-eval / 1000)) - 1

                avg = lambda_ * score + (1 - lambda_) * normalized
                assert -1 <= avg <= 1
                y.append([avg])

            print(f"skipped {pct / len(dataset.positions) * 100:.2f}%")
            # break

        self.X = torch.tensor(X, dtype=torch.float32).reshape(-1, input_size)
        self.y = torch.tensor(y, dtype=torch.float32)
        self.pst = torch.tensor(pst, dtype=torch.long).reshape(-1, 1)

    def __len__(self):
        return len(self.y)

    def __getitem__(self, index):
        return self.X[index, :], self.y[index], self.pst[index]


class FreckersNeuralNetwork(nn.Module):
    def __init__(self):
        super().__init__()
        self.layer1 = nn.Linear(8 * 8 * 2, 64 + 4, dtype=torch.float32)
        self.layer2 = nn.Linear(64 * 2, 32, dtype=torch.float32)
        self.layer3 = nn.Linear(32, 16, dtype=torch.float32)
        self.layer4 = nn.Linear(16, 1, dtype=torch.float32)

    def forward(self, x, indices):
        x = torch.flatten(x, 1)
        x1 = self.layer1(torch.concat((x[:, :64], x[:, 64:2 * 64]), dim=1))
        x2 = self.layer1(torch.concat((x[:, 2 * 64:3 * 64], x[:, 3 * 64:]), dim=1))

        # make index
        x1, x1_pst = torch.split(x1, x1.shape[1] - 4, dim=1)
        x2, x2_pst = torch.split(x2, x2.shape[1] - 4, dim=1)
        idx = indices[:, 0]
        x1_pst = x1_pst.gather(1, idx.unsqueeze(1))
        x2_pst = x2_pst.gather(1, idx.unsqueeze(1))

        x = F.relu(torch.concat((x1, x2), dim=1)).clamp(max=1)
        x = F.relu(self.layer2(x)).clamp(max=1)
        x = F.relu(self.layer3(x)).clamp(max=1)
        x = (self.layer4(x))

        pst = (x1_pst - x2_pst) / 2

        return x + pst


def custom_loss(pred, y):
    return torch.mean(torch.pow(torch.abs(pred - y), 2.6))


def train(config):
    net = FreckersNeuralNetwork().to(device)

    criterion = custom_loss
    optimizer = optim.AdamW(net.parameters(), lr=0.003, eps=1e-8)
    scheduler = ReduceLROnPlateau(optimizer, 'min')

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
        for i, (X, y, pst) in enumerate(train_dataloader, 0):
            X, y, pst = X.to(device), y.to(device), pst.to(device)
            optimizer.zero_grad()

            pred = net(X, pst)
            loss = criterion(pred, y)
            loss.backward()
            optimizer.step()
            for p in net.parameters():
                p.data.clamp_(-1.96, 1.96)

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
        for i, (X, y, pst) in enumerate(test_dataloader, 0):
            X, y, pst = X.to(device), y.to(device), pst.to(device)
            with torch.no_grad():
                pred = net(X, pst)
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
            "batch_size": 4096 * 16,
            "test_batch": 4096 * 64,
        }
    )
