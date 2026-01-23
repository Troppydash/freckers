# file to generate the texel tuning dataset
import glob
import math
import os
import pickle
import random
from selfplay import Dataset

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


def generator():
    xs = []
    ys = []
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

        for i in range(len(dataset.positions)):
            # 9mllion, requires 10k,
            prob = 50 / 9000
            if not (random.random() < prob):
                continue

            positions = dataset.positions[i]
            outcome = dataset.outcomes[i]
            turn = positions[3]
            eval = dataset.evals[i]

            xs.append([positions[0], positions[1], positions[2], positions[3]])

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
            ys.append([avg])

    print(f'loaded {len(ys)} positions')
    # write to file
    text = [str(len(ys))]
    for x, y in zip(xs, ys):
        text.append(f"{x[0]} {x[1]} {x[2]} {x[3]} {y[0]}")

    with open('texel.txt', 'w') as f:
        f.write('\n'.join(text))

if __name__ == '__main__':
    generator()