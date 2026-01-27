import glob
import math
import os
import pickle
import re
import matplotlib.pyplot as plt
import numpy as np
import scipy.optimize
from torch.utils.data import Dataset

pk_file = 'session6'


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


wdls = []
evals = []

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

    pattern = r"session\d+_(?:\((current)\)|v(\d+)(?:\(\d+\))?)vs(?:\((current)\)|v(\d+)(?:\(\d+\))?)\.pk"
    match = re.findall(pattern, os.path.basename(file))[0]
    fil = [x for x in match if x]
    players = fil
    print(f"[dataset] {players[0]} vs {players[1]}")

    skipped = 0
    for i in range(len(dataset.positions)):
        positions = dataset.positions[i]
        outcome = dataset.outcomes[i]
        lily = bitmask_to_array(positions[0])
        red = bitmask_to_array(positions[1])
        blue = bitmask_to_array(positions[2])
        turn = positions[3]
        moves = positions[4]
        total = dataset.flags[i]

        eval = dataset.evals[i]

        # skip condition
        if abs(moves) < 4:
            skipped += 1
            continue

        p = 100 if players[turn] == 'current' else int(players[turn])
        if p < 5:
            skipped += 1
            continue

        if eval > 41 * 100:
            eval = 41 * 100
        elif eval < -41 * 100:
            eval = -41 * 100

        wdl = 0
        if outcome == turn:
            wdl = 1
        elif outcome == 1 - turn:
            wdl = -1
        else:
            wdl = 0

        wdls.append(wdl)
        evals.append(eval)

    print(f"skipped {skipped / len(dataset.positions) * 100:.2f}%")


def f(x, a):
    return math.tanh(x / a)


popt, pcov = scipy.optimize.curve_fit(np.vectorize(f), evals, wdls, [2000])

print(popt)

plt.scatter(evals, wdls, s=1)

xs = np.linspace(-40 * 100, 40 * 100, 1000)
ys = np.vectorize(f)(xs, np.ones_like(xs) * popt[0])
plt.plot(xs, ys, 'r')
plt.savefig('wdl_eval.png')
