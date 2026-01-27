import json
import math
import random
import multiprocessing
import collections

import engine
import agents
from engine import Pos, Engine
import matplotlib.pyplot as plt

folder = 'elos/bt3'


def bradley_terry(names: list[str], wins: dict[tuple[str, str], float], target_avg_elo=1000, iters=10000):
    """
    Takes a list of names and wins, compute the relative elo scores

    :param names:
    :param wins:
    :param target_min_elo:
    :param iters:
    :return:
    """
    probs = {name: 0.5 for name in names}

    for iter in range(iters):
        geomean = 1.0
        for i in names:
            probs[i] = sum(wins[i, j] * probs[j] / (probs[i] + probs[j]) for j in names if j != i) / sum(
                wins[j, i] / (probs[i] + probs[j]) for j in names if j != i)

            geomean *= probs[i]

        # divide by geomean
        if abs(geomean) > 0.001:
            geomean = geomean ** (1 / len(names))
            for p in probs:
                probs[p] /= geomean

    # convert to elo, min set 100 elo
    elos = {name: math.log(probs[name]) / math.log(10) * 400 for name in names}
    avg_elo = sum(elos.values()) / len(elos)
    for elo in elos:
        elos[elo] += target_avg_elo - avg_elo

    return elos


def playoff(engine1, engine2):
    ts = 500
    engine1: Engine = engine1()
    engine2: Engine = engine2()

    pos = Pos()

    # random moves
    left = 7
    while pos.state() == pos.NONE and left > 0:
        if random.random() < 0.5:
            m, _ = engine1.play(game=pos, ts=ts // 2, verbose=False)
        else:
            m, _ = engine2.play(game=pos, ts=ts // 2, verbose=False)
        pos.push(m)
        left -= 1

    scores = [0, 0]

    _, e1 = engine1.play(game=pos, ts=ts, verbose=False)
    _, e2 = engine2.play(game=pos, ts=ts, verbose=False)
    average = (e1 + e2) // 2  # int
    if average > 100 * 100:
        wdl = 1
    elif average < -100 * 100:
        wdl = 0
    else:
        wdl = 1 / (1 + math.exp(-average / 1000))

    def run(pos, engine1_turn, wdl):

        while pos.state() == pos.NONE:
            if pos.turn == engine1_turn:
                m, _ = engine1.play(game=pos, ts=ts, verbose=False)
            else:
                m, _ = engine2.play(game=pos, ts=ts, verbose=False)

            pos.push(m)

        winner = pos.state()
        if winner == engine1_turn:
            result = 1
        elif winner == 1 - engine1_turn:
            result = 0
        else:
            result = 0.5

        scores[0] += result
        scores[1] += 1 - result

    run(pos.clone(), pos.turn, wdl)
    run(pos.clone(), 1 - pos.turn, 1 - wdl)

    return scores, abs(wdl - 0.5)


def play(x):
    i, j, ai, aj = x
    return (i, j, playoff(ai, aj))


def round(agents, elos, wins, names):
    n = len(agents)
    # k = 30

    with multiprocessing.Pool(10) as p:
        matchups = []
        for i in range(n):
            for j in range(i + 1, n):
                matchups.append((i, j, agents[i], agents[j]))

        random.shuffle(matchups)

        results = p.map(play, matchups)

    for i, j, res in results:
        scores, bias = res
        scaling = (1-2*bias)

        print(f"{names[i]} vs {names[j]}: scores {scores} bias {bias} scaling {scaling}")

        wins[names[i], names[j]] += scores[0] * scaling
        wins[names[j], names[i]] += scores[1] * scaling

    new_elos = bradley_terry(names, wins)
    for name in names:
        elos[name].append(new_elos[name])


def save_wins(wins):
    import pickle
    with open(f'{folder}/bradley_terry_wins.pk', 'wb') as f:
        pickle.dump(wins, f)


def load_wins():
    import pickle
    with open(f'{folder}/bradley_terry_wins.pk', 'rb') as f:
        return pickle.load(f)


def save_elos(elos):
    with open(f'{folder}/bradley_terry_elo.json', 'w') as f:
        json.dump(elos, f)


def load_elos(agents):
    with open(f'{folder}/bradley_terry_elo.json', 'r') as f:
        elos = json.load(f)

    best = 1
    for key in elos:
        best = max(best, len(elos[key]))

    for agent in agents:
        if agent not in elos:
            elos[agent] = [1000] * best

    save_elos(elos)
    return elos


def plot_elos(elos):
    fig, ax = plt.subplots(nrows=1, ncols=1)
    for key, value in elos.items():
        ax.plot(list(range(len(value))), value, label=key)

    ax.set_xlim([len(elos[key]) - 30, len(elos[key])])
    fig.legend(bbox_to_anchor=(0.9, 0.9), loc="upper left")
    ax.grid()
    plt.xlabel('iteration')
    plt.ylabel('elo')
    fig.savefig(f"{folder}/bradley_terry_elo.png", bbox_inches="tight")

    plt.close(fig)


if __name__ == '__main__':
    # agents = [agents.V0, agents.V1, agents.V2, agents.V32, agents.V4, agents.V5, agents.V62, agents.V73,
    #           agents.Latest]
    # names = [ "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "latest"]

    agents = [agents.V73, agents.V75,
              agents.Latest]
    names = ["v73", "v75", "latest"]

    assert len(agents) == len(names)
    elos = load_elos(names)
    # plot_elos(elos)

    wins = {(a, b): 0.01 for a in names for b in names}
    try:
        wins = load_wins()
        print('loaded wins')
    except:
        pass

    i = 0
    while True:
        print(f"[info] round {i}")
        round(agents, elos, wins, names)
        save_elos(elos)
        save_wins(wins)
        plot_elos(elos)

        i += 1
