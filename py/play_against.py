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
                    start_row, start_col, end_row, end_col = map(int, stdin.split())

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



if __name__ == '__main__':
    play_against(engine.Pos.RED, agents.Latest, 2000)