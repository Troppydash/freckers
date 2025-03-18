import glob
import os
import pickle




class Dataset:
    def __init__(self, session: str, agent: str):
        self.session = session
        self.agent = agent
        self.positions = []
        self.flags = []
        self.evals = []
        self.outcomes = []

    def save(self):
        with open(f'./sessions/{self.session}_{self.agent}.pk', 'wb') as f:
            pickle.dump(self, f)

        with open(f'./sessions/{self.session}_{self.agent}_backup.pk', 'wb') as f:
            pickle.dump(self, f)

    def add(self, positions, flags, evals, outcomes):
        self.positions.extend(positions)
        self.flags.extend(flags)
        self.evals.extend(evals)
        self.outcomes.extend(outcomes)

        self.save()

def make_inputs(position: tuple[int, int, int, int, int]):
    lily, red, blue, turn, mode = position

    # make red
    out_red = []
    for i in range(64):
        for j in range(64):
            if lily & (1<<i) and red & (1<<j):
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

def preprocess_data(session):
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
            print(f"\r[progress] {i / len(dataset.positions)}", end='')
            x = make_inputs(dataset.positions[i])
            X.append(x)

            outcome = dataset.outcomes[i]
            score = 0.5
            if outcome == 0:
                score = 1
            elif outcome == 1:
                score = 0

            y.append([score])

        with open('saved.pk', 'wb') as f:
            pickle.dump((X, y), f)

        break




    # self.X = torch.tensor(X, dtype=torch.float32).reshape(-1, input_size)
    # self.y = torch.tensor(y, dtype=torch.float32)


if __name__ == '__main__':
    preprocess_data("session2")