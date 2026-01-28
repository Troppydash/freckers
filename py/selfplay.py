import multiprocessing
import random
import os.path
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

    def load(self):
        fname = f'./sessions/{self.session}_{self.agent}.pk'
        if os.path.isfile(fname):
            raise Exception()
            with open(f'./sessions/{self.session}_{self.agent}.pk', 'wb') as f:
                new_dataset = pickle.load(f)

            self.session = new_dataset.session
            self.agent = new_dataset.agent
            self.positions = new_dataset.positions
            self.flags = new_dataset.flags
            self.evals = new_dataset.evals
            self.outcomes = new_dataset.outcomes

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
        ts = 800

        past, n, current, name = x

        past_agent: Engine = past()
        current_agent: Engine = current()

        dataset = Dataset(self.session, f"{n}vs{name}")
        dataset.load()

        rounds = 0
        while True:
            rounds += 1
            print(f"[info] {n}vs{name}, round {rounds}")

            pos = Pos()
            positions = []
            evals = []

            i = 0
            while pos.state() == Pos.NONE:
                eps = 0.85 ** i

                positions.append((pos.lily_pad, pos.red, pos.blue, pos.turn, pos.moves))
                # if pos.has_jumps:
                #     flags.append(1)
                # else:
                #     flags.append(0)

                # play random move if rand < eps
                if random.random() < eps:
                    if name == '(current)':
                        _, score = current_agent.play(pos, ts, False)
                    else:
                        _, score = past_agent.play(pos, ts, False)

                    moves = pos.get_moves()
                    move = random.choice(moves)
                    pos.push(move)
                    evals.append(score)
                else:
                    if pos.turn == Pos.RED:
                        move, score = past_agent.play(pos, ts, False)
                    else:
                        move, score = current_agent.play(pos, ts, False)

                    pos.push(move)
                    evals.append(score)

                i += 1

            flags = [pos.moves] * len(positions)
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
        return [agents.Random, agents.V0, agents.V0, agents.V0, agents.V0], ["random", "v0(0)", "v0(1)", "v0(2)",
                                                                             "v0(3)"]

    def current_agent(self):
        return agents.V0, "v0(current)"


class Session1(Session):
    # ts = 1000
    # 0.85
    def past_agents(self):
        return [agents.Random, agents.V0, agents.V1, agents.V1, agents.V1], ["random", "v0(0)", "v1(0)", "v1(1)",
                                                                             "v1(2)"]

    def current_agent(self):
        return agents.V1, "v1(current)"


class Session2(Session):
    # ts = 150
    # 0.85
    def past_agents(self):
        return [agents.Random, agents.V0, agents.V1, agents.V2, agents.V2, agents.V2], ["random", "v0(0)", "v1(0)",
                                                                                        "v2(0)", "v2(1)", "v2(2)"]

    def current_agent(self):
        return agents.V2, "v2(current)"


class Session3(Session):
    # ts = 200
    # 0.85
    def past_agents(self):
        return [agents.V2, agents.V3, agents.V31, agents.V32, agents.V32, agents.V32, agents.V32], ["v2", "v3", "v3.1",
                                                                                                    "v3.2(0)",
                                                                                                    "v3.2(1)",
                                                                                                    "v3.2(2)",
                                                                                                    "v3.2(3)"]

    def current_agent(self):
        return agents.V32, "(current)"


class Session4(Session):
    # ts = 200
    # 0.85
    def past_agents(self):
        return [agents.V3, agents.V31, agents.V32, agents.V4, agents.V4, agents.V4, agents.V4], ["v3", "v3.1", "v3.2",
                                                                                                 "v4(0)", "v4(1)",
                                                                                                 "v4(2)", "v4(3)"]

    def current_agent(self):
        return agents.V4, "(current)"


class Session5(Session):
    # ts = 2000
    # 0.87, max = 0.001
    def past_agents(self):
        return ([agents.Random, agents.V1, agents.V2, agents.V32, agents.V4, agents.V4,
                 agents.V5, agents.V5, agents.V5, agents.V5, agents.V5],
                ["v0", "v1", "v2", "v3_2", "v4(1)", "v4(2)", "v5(1)", "v5(2)", "v5(3)", "v5(4)", "v5(5)"])

    def current_agent(self):
        return agents.V5, "(current)"


class Session51(Session):
    # ts = 300
    # 0.85
    def past_agents(self):
        return ([agents.V0, agents.V1, agents.V2, agents.V32, agents.V32, agents.V4,
                 agents.V4, agents.V5, agents.V5, agents.V5, agents.V5],
                ["v0", "v1", "v2", "v3_2(1)", "v3_2(2)", "v4(1)", "v4(2)", "v5(1)", "v5(2)", "v5(3)", "v5(4)"])

    def current_agent(self):
        return agents.V5, "(current)"


class Session6(Session):
    # ts = 800
    # 0.85
    def past_agents(self):
        opp = [agents.V0] * 1 + [agents.V1] * 1 + [agents.V2] * 1 + [agents.V32] * 2 + [agents.V4] * 2 + \
              [agents.V5] * 4 + [agents.V6] * 8 + [agents.V62] * 16
        names = ["v0", "v1", "v2", "v32(1)", "v32(2)", "v4(1)", "v4(2)", "v5(1)", "v5(2)", "v5(3)", "v5(4)"] + \
                [f"v6({i})" for i in range(8)] + [f"v62({i})" for i in range(16)]

        assert len(opp) == len(names) == 35
        return opp, names

    def current_agent(self):
        return agents.V62, "(current)"


if __name__ == '__main__':
    session = Session6("session7")
    session.generate()
