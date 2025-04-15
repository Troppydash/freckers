import json
import random
import multiprocessing

import engine
import agents
from engine import Pos, Engine
import matplotlib.pyplot as plt


def playoff(engine1, engine2):
    ts = 500
    engine1: Engine = engine1()
    engine2: Engine = engine2()

    pos = Pos()

    # random moves
    left = 5
    while pos.state() == pos.NONE and left > 0:
        # if random.random() < 0.9:
        # if random.random() < 0.5:
        #     m, _ = engine1.play(game=pos, ts=ts, verbose=False)
        # else:
        #     m, _ = engine2.play(game=pos, ts=ts, verbose=False)
        # pos.push(m)
        # else:
        moves = pos.get_moves()
        pos.push(random.choice(moves))
        left -= 1

    scores = [0, 0]

    def run(pos, engine1_turn):
        while pos.state() == pos.NONE:
            if pos.turn == engine1_turn:
                m, _ = engine1.play(game=pos, ts=ts, verbose=False)
            else:
                m, _ = engine2.play(game=pos, ts=ts, verbose=False)

            pos.push(m)

        winner = pos.state()
        if winner == engine1_turn:
            scores[0] += 1
        elif winner == 1 - engine1_turn:
            scores[1] += 1
        else:
            scores[0] += 0.5
            scores[1] += 0.5

    run(pos.clone(), pos.turn)
    run(pos.clone(), 1 - pos.turn)

    return scores


def play(x):
    i, j, ai, aj = x
    return (i, j, playoff(ai, aj))


def round(agents, elos, names):
    n = len(agents)
    k = 32

    with multiprocessing.Pool(6) as p:
        matchups = []
        for i in range(n):
            for j in range(i + 1, n):
                matchups.append((i, j, agents[i], agents[j]))

        random.shuffle(matchups)

        results = p.map(play, matchups)

    for i, j, scores in results:
        # update result
        probi = 1 / (1 + 10 ** ((elos[names[j]][-1] - elos[names[i]][-1]) / 400))
        probj = 1 / (1 + 10 ** ((elos[names[i]][-1] - elos[names[j]][-1]) / 400))
        elos[names[i]].append(max(400, elos[names[i]][-1] + k * (scores[0] / 2 - probi)))
        elos[names[j]].append(max(400, elos[names[j]][-1] + k * (scores[1] / 2 - probj)))


def save_elos(elos):
    with open('elo.json', 'w') as f:
        json.dump(elos, f)


def load_elos(agents):
    with open('elo.json', 'r') as f:
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

    ax.set_xlim([len(elos[key]) - 150, len(elos[key])])
    fig.legend(bbox_to_anchor=(1.04, 1), loc="upper left")
    ax.grid()
    fig.savefig("elos.png", bbox_inches="tight")
    plt.close(fig)


if __name__ == '__main__':
    # agents = [agents.V0, agents.V1, agents.V2, agents.V32, agents.V4, agents.V5, agents.V6, agents.Latest]
    # names = ["v0", "v1", "v2", "v32", "v4", "v5", "v6", "latest"]

    agents = [agents.V0, agents.V1, agents.V2, agents.V32, agents.V4, agents.V5, agents.V6, agents.V62, agents.Latest]
    names = ["v0", "v1", "v2", "v3.2", "v4", "v5", "v6", "v6.2", "latest"]
    assert len(agents) == len(names)
    elos = load_elos(names)
    plot_elos(elos)

    i = 0
    while True:
        print(f"[info] round {i}")
        round(agents, elos, names)
        save_elos(elos)
        plot_elos(elos)

        i += 1
