import json
import math
import os
import queue
import random
import multiprocessing
import collections

import engine
import agents
from engine import Pos, Engine
import matplotlib.pyplot as plt

folder = 'elos/bt5'


def bradley_terry(names: list[str], wins: dict[tuple[str, str], float], target_avg_elo=1000, iters=10000):
    """
    Takes a list of names and wins, compute the relative elo scores

    :param names:
    :param wins:
    :param target_min_elo:
    :param iters:
    :return:
    """
    probs = {name: 1.0 for name in names}

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
    ts = 300
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

    def run(pos, engine1_turn):
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

    run(pos.clone(), pos.turn)
    run(pos.clone(), 1 - pos.turn)

    return scores


def play_worker(core, job_queue, out_queue):
    os.sched_setaffinity(0, {core})

    while True:
        try:
            job = job_queue.get(timeout=1)
        except queue.Empty:
            break

        i, j, ai, aj = job
        result = (i, j, playoff(ai, aj))
        out_queue.put(result)


def compute_elos(names, wins, iters=5000, tol=1e-9, base_elo=1000):
    """
    Thanks chatgpt

    :param names:
    :param wins:
    :param iters:
    :param tol:
    :param base_elo:
    :return:
    """

    n = len(names)

    # Bradley–Terry parameters (strengths)
    s = {name: 0.0 for name in names}

    for _ in range(iters):
        max_delta = 0.0
        for i in names:
            num = 0.0
            den = 0.0
            for j in names:
                if i == j:
                    continue
                w_ij = wins[i, j]
                w_ji = wins[j, i]
                games = w_ij + w_ji
                if games == 0:
                    continue

                p_ij = math.exp(s[i]) / (math.exp(s[i]) + math.exp(s[j]))
                num += w_ij
                den += games * p_ij

            if den == 0 or num == 0:
                continue

            new_s = math.log(num / den)
            delta = abs(new_s - s[i])
            s[i] = new_s
            max_delta = max(max_delta, delta)

        if max_delta < tol:
            break

    # Convert strengths to Elo
    elos = {name: base_elo + 400 / math.log(10) * si for name, si in s.items()}

    # Normalize mean Elo
    mean_elo = sum(elos.values()) / n
    elos = {name: e + (base_elo - mean_elo) for name, e in elos.items()}

    # Uncertainty estimate (≈ Elo std dev)
    # σ ≈ 400 / ln(10) / sqrt(total games)
    uncert = {}
    for i in names:
        games = sum(wins[i, j] + wins[j, i] for j in names if j != i)
        if games > 0:
            sigma = (400 / math.log(10)) / math.sqrt(games)
        else:
            sigma = 100

        uncert[i] = sigma

    return {
        name: (elos[name], uncert[name])
        for name in names
    }


def round(agents, elos, wins, names):
    n = len(agents)
    # cores to fix, 0-11 are p core threads
    cores = [0, 2, 4, 6, 8, 10]
    print(f'[elo] running with {len(cores)} threads')

    matchups = []
    for i in range(n):
        for j in range(i + 1, n):
            matchups.append((i, j, agents[i], agents[j]))

    random.shuffle(matchups)
    print(f"[elo] {len(matchups)} matchups")

    out_queue = multiprocessing.Queue()
    job_queue = multiprocessing.Queue()
    for matchup in matchups:
        job_queue.put(matchup)

    workers = []
    for core in cores:
        p = multiprocessing.Process(target=play_worker, args=(core, job_queue, out_queue))
        p.start()
        workers.append(p)

    results = []
    for _ in matchups:
        result = out_queue.get()
        results.append(result)
        print(f"[elo] {len(results)} results")

        i, j, res = result
        scores = res
        print(f"{names[i]} vs {names[j]}: scores {scores}")

        wins[names[i], names[j]] += scores[0]
        wins[names[j], names[i]] += scores[1]

        new_elos = compute_elos(names, wins)
        for name in names:
            elos[name].append(new_elos[name])

        plot_elos(elos)

    for p in workers:
        p.join()


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
            elos[agent] = [(1000, 100)] * best

    save_elos(elos)
    return elos


def plot_elos(elos: dict[str, list[tuple[float, float]]]):
    """
    Thanks chatgpt

    :param elos:
    :return:
    """
    plt.figure(figsize=(10, 6))

    for name, series in elos.items():
        if not series:
            continue

        x = list(range(len(series)))
        y = [e for e, _ in series]
        yerr = [u for _, u in series]

        plt.errorbar(
            x,
            y,
            yerr=yerr,
            label=name,
            capsize=2,
            marker='o',
            linestyle='-'
        )

    plt.xlabel("Iteration")
    plt.ylabel("Elo")
    plt.title("Elo ratings with uncertainty")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(f"{folder}/bradley_terry_elo.png", bbox_inches="tight")
    plt.close()


if __name__ == '__main__':
    # agents = [agents.V0, agents.V1, agents.V2, agents.V32, agents.V4, agents.V5, agents.V62, agents.V73,
    #           agents.Latest]
    # names = [ "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "latest"]

    agents = [agents.V62, agents.V73, agents.V74, agents.V75,
              agents.Latest]
    names = ["v62", "v73", "v74", "v75", "latest"]

    assert len(agents) == len(names)
    elos = load_elos(names)
    plot_elos(elos)

    wins = {(a, b): 0 for a in names for b in names}
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
