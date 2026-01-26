import dataclasses
import ctypes
import os
import shutil
import tempfile
import uuid


def load_dll():
    dirname = os.path.dirname(__file__)
    lib_file = os.path.join(dirname, "./binaries/libfrecker.so")
    rand = uuid.uuid4()
    temp_dir = tempfile.gettempdir()
    temp_file = os.path.join(temp_dir, f"{rand}.so")
    shutil.copy2(lib_file, temp_file)

    cpp = ctypes.cdll.LoadLibrary(temp_file)
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
        self.handle = self.cpp.make_instance()
        self.cpp.pos_default(self.handle)

        self.cpp.pos_moves_start.restype = ctypes.c_ulonglong
        self.cpp.pos_moves_end.restype = ctypes.c_ulonglong
        self.cpp.pos_moves_grow.restype = ctypes.c_ulonglong
        self.cpp.pos_lily.restype = ctypes.c_ulonglong
        self.cpp.pos_red.restype = ctypes.c_ulonglong
        self.cpp.pos_blue.restype = ctypes.c_ulonglong
        self.cpp.get_last_move_start.restype = ctypes.c_ulonglong
        self.cpp.get_last_move_end.restype = ctypes.c_ulonglong
        self.cpp.get_last_move_grow.restype = ctypes.c_ulonglong

    def __del__(self):
        self.cpp.free_instance(self.handle)

    def of(self, lily, red, blue, turn, moves):
        self.cpp.pos_load(self.handle, ctypes.c_ulonglong(lily), ctypes.c_ulonglong(red), ctypes.c_ulonglong(blue), turn, moves)

    @property
    def lily_pad(self):
        return self.cpp.pos_lily(self.handle)

    @property
    def red(self):
        return self.cpp.pos_red(self.handle)

    @property
    def blue(self):
        return self.cpp.pos_blue(self.handle)

    @property
    def moves(self):
        return self.cpp.pos_moves(self.handle)

    @property
    def turn(self):
        return self.cpp.pos_turn(self.handle)

    @property
    def has_jumps(self):
        return self.cpp.pos_has_jumps(self.handle)

    def push(self, move: Move):
        self.cpp.pos_push(self.handle, ctypes.c_ulonglong(move.grow), ctypes.c_ulonglong(move.start), ctypes.c_ulonglong(move.end))

    def pop(self, move: Move):
        self.cpp.pos_pop(self.handle, ctypes.c_ulonglong(move.grow), ctypes.c_ulonglong(move.start), ctypes.c_ulonglong(move.end))

    def state(self):
        return self.cpp.pos_state(self.handle)

    def get_moves(self):
        self.cpp.pos_compute_moves(self.handle)
        moves = []
        for i in range(self.cpp.pos_moves_length(self.handle)):
            moves.append(Move(
                start=self.cpp.pos_moves_start(self.handle, i),
                end=self.cpp.pos_moves_end(self.handle, i),
                grow=self.cpp.pos_moves_grow(self.handle, i),
            ))

        return moves

    def display(self):
        self.cpp.pos_display(self.handle)

    def clone(self):
        pos = Pos()
        pos.of(self.lily_pad, self.red, self.blue, self.turn, self.moves)
        return pos


class Engine:
    def __init__(self, dll):
        self.cpp = dll
        self.handle = self.cpp.make_instance()

        self.cpp.get_last_move_start.restype = ctypes.c_ulonglong
        self.cpp.get_last_move_end.restype = ctypes.c_ulonglong
        self.cpp.get_last_move_grow.restype = ctypes.c_ulonglong

    def __del__(self):
        self.cpp.free_instance(self.handle)

    def play(self, game: Pos, ts: int, verbose: bool) -> tuple[Move, int]:
        self.cpp.play(self.handle, ctypes.c_ulonglong(game.lily_pad), ctypes.c_ulonglong(game.red), ctypes.c_ulonglong(game.blue),
                      game.turn, game.moves, ts, verbose)
        return Move(
            start=self.cpp.get_last_move_start(self.handle),
            end=self.cpp.get_last_move_end(self.handle),
            grow=self.cpp.get_last_move_grow(self.handle),
        ), self.cpp.get_last_score(self.handle)


if __name__ == '__main__':
    import random
    import threading

    engine1 = Engine()
    game = Pos()
    i = 0
    while game.state() == Pos.NONE:
        game.display()
        print()
        move, score = engine1.play(game, 1000, True)
        game.push(move)
        i += 1

    print(i)
    print(game.state())
    # engine = Engine()
    # engine.play()
