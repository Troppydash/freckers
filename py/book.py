import agents
import engine
from collections import deque
import base64
import pickle

def positions():
    return 50*1000*1000 // (6*24)


def main():
    print("max positions", positions())
    agent = agents.Latest()

    output = {}

    queue = deque()
    queue.append((engine.Pos(), 0))
    while len(queue) > 0:
        pos, depth = queue.popleft()

        if (pos.red, pos.blue, pos.lily_pad, pos.turn) in output:
            continue

        # do simple search
        move, score = agent.play(pos, 1000, False)
        # prune
        if abs(score) >= 20*100:
            continue

        print(f"\nSearching, depth {depth}")
        pos.display()
        print(pos.turn)
        move, score = agent.play(pos, 10_000, True)
        print(f"score {score}")

        output[(pos.red, pos.blue, pos.lily_pad, pos.turn)] = (move.grow, move.start, move.end)
        print(len(output), "entries")

        # saving
        with open('book.pk', 'wb') as f:
            pickle.dump(output, f)

        with open('book_backup.pk', 'wb') as f:
            pickle.dump(output, f)

        for m in pos.get_moves():
            new_pos = engine.Pos()
            new_pos.of(pos.lily_pad, pos.red, pos.blue, pos.turn, pos.moves)
            new_pos.push(m)
            queue.append((new_pos, depth+1))

    # agent.play(pos, 30_000, True)

if __name__ == '__main__':
    main()