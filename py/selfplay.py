import multiprocessing
import random

import agents
from engine import Engine, Pos
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

class Session:
    def __init__(self, session: str):
        self.session = session

    def past_agents(self):
        pass

    def current_agent(self):
        pass

    def against(self, x):
        ts = 1000

        past, n, current, name = x

        past_agent: Engine = past()
        current_agent: Engine = current()

        dataset = Dataset(self.session, f"{n}vs{name}")

        rounds = 0
        while True:
            rounds += 1
            print(f"[info] {n}vs{name}, round {rounds}")


            pos = Pos()
            positions = []
            flags = []
            evals = []

            i = 0
            while pos.state() == Pos.NONE:
                eps = max(0.03, 0.85 ** i)

                positions.append((pos.lily_pad, pos.red, pos.blue, pos.turn, pos.moves))
                if pos.has_jumps:
                    flags.append(1)
                else:
                    flags.append(0)

                # play random move if rand < eps
                if random.random() < eps:
                    moves = pos.get_moves()
                    move = random.choice(moves)
                    pos.push(move)
                else:
                    if pos.turn == Pos.RED:
                        move, score = past_agent.play(pos, ts, False)
                    else:
                        move, score = current_agent.play(pos, ts, False)

                    pos.push(move)
                    evals.append(score)

                i += 1

            outcomes = [pos.state()] * len(positions)
            dataset.add(positions, flags, evals, outcomes)

    def generate(self):
        past_agents, names = self.past_agents()
        current_agent, name = self.current_agent()

        # play against each agent
        playoffs = []
        for past, n in zip(past_agents, names):
            playoffs.append((past, n, current_agent, name))
            playoffs.append((current_agent, name, past, n))

        total = len(playoffs)
        print(f'[info] using {total} threads')
        with multiprocessing.Pool(total) as p:
            p.map(self.against, playoffs)

class Session0(Session):
    # ts = 100
    # 0.85
    def past_agents(self):
        return [agents.Random, agents.V0, agents.V0, agents.V0, agents.V0], ["random", "v0(0)", "v0(1)", "v0(2)", "v0(3)"]

    def current_agent(self):
        return agents.V0, "v0(current)"


class Session1(Session):
    # ts = 1000
    # 0.85
    def past_agents(self):
        return [agents.Random, agents.V0, agents.V1, agents.V1, agents.V1], ["random", "v0(0)", "v1(0)", "v1(1)", "v1(2)"]

    def current_agent(self):
        return agents.V1, "v1(current)"


if __name__ == '__main__':
    session = Session1("session1")
    session.generate()