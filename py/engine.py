import dataclasses
import ctypes
import os


def load_dll():
    dirname = os.path.dirname(__file__)
    lib_file = os.path.join(dirname, "../cmake-build-release/libfrecker.so")
    cpp = ctypes.cdll.LoadLibrary(lib_file)
    return cpp


@dataclasses.dataclass
class Move:
    start: int
    end: int
    grow: int

    def is_grow(self):
        return self.grow != (1 << 64) - 1


class Pos:
    RED = 0
    BLUE = 1
    NONE = 2
    DRAW = 3

    def __init__(self):
        self.cpp = load_dll()
        self.cpp.pos_default()

        self.cpp.pos_moves_start.restype = ctypes.c_ulonglong
        self.cpp.pos_moves_end.restype = ctypes.c_ulonglong
        self.cpp.pos_moves_grow.restype = ctypes.c_ulonglong
        self.cpp.pos_lily.restype = ctypes.c_ulonglong
        self.cpp.pos_red.restype = ctypes.c_ulonglong
        self.cpp.pos_blue.restype = ctypes.c_ulonglong

    @property
    def lily_pad(self):
        return self.cpp.pos_lily()

    @property
    def red(self):
        return self.cpp.pos_red()

    @property
    def blue(self):
        return self.cpp.pos_blue()

    @property
    def moves(self):
        return self.cpp.pos_moves()

    @property
    def turn(self):
        return self.cpp.pos_turn()

    def push(self, move: Move):
        self.cpp.pos_push(ctypes.c_ulonglong(move.grow), ctypes.c_ulonglong(move.start), ctypes.c_ulonglong(move.end))

    def pop(self, move: Move):
        self.cpp.pos_pop(ctypes.c_ulonglong(move.grow), ctypes.c_ulonglong(move.start), ctypes.c_ulonglong(move.end))

    def state(self):
        return self.cpp.pos_state()

    def get_moves(self):
        self.cpp.pos_compute_moves()
        moves = []
        for i in range(self.cpp.pos_moves_length()):
            moves.append(Move(
                start=self.cpp.pos_moves_start(i),
                end=self.cpp.pos_moves_end(i),
                grow=self.cpp.pos_moves_grow(i),
            ))

        return moves

    def display(self):
        self.cpp.pos_display()


class Engine:
    def __init__(self):
        self.cpp = load_dll()

    def play(self, game: Pos, ts: int, verbose: bool) -> tuple[Move, int]:
        self.cpp.play(ctypes.c_ulonglong(game.lily_pad), ctypes.c_ulonglong(game.red), ctypes.c_ulonglong(game.blue),
                      game.turn, game.moves, ts, verbose)
        return Move(
            start=self.cpp.get_last_move_start(),
            end=self.cpp.get_last_move_end(),
            grow=self.cpp.get_last_move_grow(),
        ), self.cpp.get_last_score()


if __name__ == '__main__':
    import random

    engine = Engine()
    game = Pos()
    i = 0
    while game.state() == Pos.NONE:
        game.display()
        print()
        move, score = engine.play(game, 1000, True)
        game.push(move)
        i += 1

    print(i)
    print(game.state())
    # engine = Engine()
    # engine.play()
