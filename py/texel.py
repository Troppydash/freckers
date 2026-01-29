# file to generate the texel tuning dataset
import glob
import math
import os
import pickle
import random
import re

from torch.utils.data import Dataset

WDL_SCALE = 2684
pk_file = "session6"


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

        pattern = r"session\d+_(?:\((current)\)|v(\d+)(?:\(\d+\))?)vs(?:\((current)\)|v(\d+)(?:\(\d+\))?)\.pk"
        match = re.findall(pattern, os.path.basename(file))[0]
        fil = [x for x in match if x]
        players = fil
        print(f"[dataset] {players[0]} vs {players[1]}")

        p0 = 6 if players[0] == 'current' else int(players[0])
        p1 = 6 if players[1] == 'current' else int(players[1])
        if not (p0 == 6 and p1 == 6):
            continue

        for i in range(len(dataset.positions)):
            positions = dataset.positions[i]
            outcome = dataset.outcomes[i]
            turn = positions[3]
            eval = dataset.evals[i]
            moves = positions[4]
            total = dataset.flags[i]

            p = 6 if players[turn] == 'current' else int(players[turn])

            # skip early, late, checkmate, unstable positions, zero positions, draws
            prev_eval = -dataset.evals[i - 1]
            eval_diff = abs(prev_eval - eval)
            zeros = (abs(prev_eval) + abs(eval)) / 2
            if moves < 10 or total - moves < 10 or abs(
                    eval) > 30 * 100 or eval_diff > 7 * 100 or zeros < 1 * 100 or outcome == 2:
                continue

            # [lily, red, blue, turn, moves]
            xs.append([positions[0], positions[1], positions[2], positions[3], positions[4]])

            if outcome == turn:
                game_result = 1
            elif outcome == 1 - turn:
                game_result = -1
            else:
                game_result = 0

            # our score is in the perspective of the moving player
            wdl = 0.9
            if p < 6:
                other = game_result
            elif eval > 40 * 100:
                other = 1
            elif eval < -40 * 100:
                other = -1
            else:
                other = math.tanh(eval / WDL_SCALE)

            target = wdl * game_result + (1 - wdl) * other
            ys.append([target])

    print(f'loaded {len(ys)} positions')

    # write to file
    text = [str(len(ys))]
    for x, y in zip(xs, ys):
        text.append(f"{x[0]} {x[1]} {x[2]} {x[3]} {x[4]} {y[0]}")

    with open('texel2.txt', 'w') as f:
        f.write('\n'.join(text))


if __name__ == '__main__':
    generator()
