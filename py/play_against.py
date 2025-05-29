import agents
import engine


def play_against(player_turn, agent_class, ts):
    agent: engine.Engine = agent_class()
    board = engine.Pos()
    while board.state() == engine.Pos.NONE:
        board.display()

        if board.turn == player_turn:
            while True:
                stdin = input()
                moves = board.get_moves()
                player_move = None
                if stdin == "g":
                    for move in moves:
                        if move.is_grow():
                            player_move = move
                            break
                else:
                    data = list(map(int, stdin.split()))
                    if len(data) != 4:
                        print('unknown input')
                        continue

                    start_row, start_col, end_row, end_col = data

                    start = 1 << (start_row * 8 + start_col)
                    end = 1 << (end_row * 8 + end_col)

                    for move in moves:
                        if move.start == start and move.end == end:
                            player_move = move
                            break

                if player_move is None:
                    print('unknown move')
                    continue

                break

            board.push(player_move)
        else:
            move, _ = agent.play(board, ts, True)
            board.push(move)


def play_engines(engine1, engine2, ts):
    engine1: engine.Engine = engine1()
    engine2: engine.Engine = engine2()
    board = engine.Pos()
    while board.state() == engine.Pos.NONE:
        board.display()

        if board.turn == 0:
            print('engine1')
            move, _ = engine1.play(board, ts, True)
            board.push(move)
        else:
            print('engine2')
            move, _ = engine2.play(board, ts, True)
            board.push(move)


if __name__ == '__main__':
    # play_against(engine.Pos.RED, agents.Latest, 5000)
    play_engines(agents.V73, agents.V73, 5000)
