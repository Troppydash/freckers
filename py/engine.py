import dataclasses
import ctypes
import os


@dataclasses.dataclass
class Game:
    lily_pads: int
    red: int
    blue: int
    turn: int
    moves: int


class Engine:
    def __init__(self):
        dirname = os.path.dirname(__file__)
        lib_file = os.path.join(dirname, "../cmake-build-release/libfrecker.so")
        self.engine = ctypes.cdll.LoadLibrary(lib_file)

    def play(self, game: Game, ts: int) -> int:
        self.engine.play(game.lily_pads, game.red, game.blue, game.turn, game.moves, ts, True)
        return self.engine.get_last_score()


if __name__ == '__main__':
    engine = Engine()
    # engine.play()