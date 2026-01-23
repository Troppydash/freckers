import agents
import engine
from collections import deque
import base64
import pickle

def positions():
    return 50*1000*1000 // (6*24)


def search(pos, agent, output, file = 'book.pk'):

    pos.display()
    print(pos.turn)
    move, score = agent.play(pos, 1500, False)
    print(f"score {score}")

    output[(pos.red, pos.blue, pos.lily_pad, pos.turn)] = (move.grow, move.start, move.end)
    print(len(output), "entries")

    # saving
    with open(file, 'wb') as f:
        pickle.dump(output, f)
    with open('backup_'+file, 'wb') as f:
        pickle.dump(output, f)

    return score

def main():
    print("max positions", positions())
    agent = agents.Latest()

    output = {}

    # handle engine.Pos
    pos = engine.Pos()
    search(pos, agent, output)

    queue = deque()
    queue.append((engine.Pos(), 0))
    while len(queue) > 0:
        pos, depth = queue.popleft()

        print('computing children')
        # assume that pos is evaluated, evaluate all children of pos
        children = []
        for m in pos.get_moves():
            new_pos = engine.Pos()
            new_pos.of(pos.lily_pad, pos.red, pos.blue, pos.turn, pos.moves)
            new_pos.push(m)

            if (new_pos.red, new_pos.blue, new_pos.lily_pad, new_pos.turn) in output:
                continue

            print(f'depth {depth+1}')
            score = search(new_pos, agent, output)
            if abs(score) <= 15*100:
                children.append((new_pos, depth + 1, score))

        print('selecting the best 4')
        children.sort(key=lambda k: k[2], reverse=False)
        for child in children[:4]:
            queue.append((child[0], child[1]))


def main_red():
    print("max positions", positions())
    agent = agents.Latest()

    output = {}

    file = 'book_red1s.pk'

    # handle engine.Pos
    pos = engine.Pos()
    search(pos, agent, output, file)

    queue = deque()
    queue.append((engine.Pos(), 0))
    while len(queue) > 0:
        pos, depth = queue.popleft()

        if depth % 2 == 0:
            print(f"using only book move")
            new_pos = engine.Pos()
            new_pos.of(pos.lily_pad, pos.red, pos.blue, pos.turn, pos.moves)
            best_move = output[(pos.red, pos.blue, pos.lily_pad, pos.turn)]
            best_move = engine.Move(best_move[1], best_move[2], best_move[0])
            new_pos.push(best_move)

            if (new_pos.red, new_pos.blue, new_pos.lily_pad, new_pos.turn) in output:
                continue

            search(new_pos, agent, output, file)
            queue.append((new_pos, depth + 1))
        else:

            print('computing children')
            # assume that pos is evaluated, evaluate all children of pos
            children = []
            for m in pos.get_moves():
                new_pos = engine.Pos()
                new_pos.of(pos.lily_pad, pos.red, pos.blue, pos.turn, pos.moves)
                new_pos.push(m)

                if (new_pos.red, new_pos.blue, new_pos.lily_pad, new_pos.turn) in output:
                    continue

                print(f'depth {depth+1}')
                score = search(new_pos, agent, output, file)
                if abs(score) <= 15*100:
                    children.append((new_pos, depth + 1, score))

            print(f'selecting the best {4}')
            children.sort(key=lambda k: k[2], reverse=False)
            for child in children[:4]:
                queue.append((child[0], child[1]))


def main_blue():
    print("max positions", positions())
    agent = agents.Latest()

    output = {}

    file = 'book_blue1s.pk'

    # handle engine.Pos
    pos = engine.Pos()
    search(pos, agent, output, file)

    queue = deque()
    queue.append((engine.Pos(), 0))
    while len(queue) > 0:
        pos, depth = queue.popleft()

        if depth % 2 == 1:
            print(f"using only book move")
            new_pos = engine.Pos()
            new_pos.of(pos.lily_pad, pos.red, pos.blue, pos.turn, pos.moves)
            best_move = output[(pos.red, pos.blue, pos.lily_pad, pos.turn)]
            best_move = engine.Move(best_move[1], best_move[2], best_move[0])
            new_pos.push(best_move)

            if (new_pos.red, new_pos.blue, new_pos.lily_pad, new_pos.turn) in output:
                continue

            search(new_pos, agent, output, file)
            queue.append((new_pos, depth + 1))
        else:

            print('computing children')
            # assume that pos is evaluated, evaluate all children of pos
            children = []
            for m in pos.get_moves():
                new_pos = engine.Pos()
                new_pos.of(pos.lily_pad, pos.red, pos.blue, pos.turn, pos.moves)
                new_pos.push(m)

                if (new_pos.red, new_pos.blue, new_pos.lily_pad, new_pos.turn) in output:
                    continue

                print(f'depth {depth+1}')
                score = search(new_pos, agent, output, file)
                if abs(score) <= 15*100:
                    children.append((new_pos, depth + 1, score))

            print(f'selecting the best {4}')
            children.sort(key=lambda k: k[2], reverse=False)
            for child in children[:4]:
                queue.append((child[0], child[1]))



if __name__ == '__main__':
    main_red()
    # main_blue()