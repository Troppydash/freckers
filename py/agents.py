import ctypes
import os
import random

from engine import Engine, Pos, Move


def load_dll(name):
    dirname = os.path.dirname(__file__)
    lib_file = os.path.join(dirname, f"./binaries/{name}")
    cpp = ctypes.cdll.LoadLibrary(lib_file)
    return cpp


class Random:
    def play(self, game: Pos, ts: int, verbose: bool) -> tuple[Move, int]:
        return random.choice(game.get_moves()), 0


class V0(Engine):
    def __init__(self):
        super().__init__(load_dll("v0.so"))


class V1(Engine):
    def __init__(self):
        super().__init__(load_dll('v1/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v1/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class V2(Engine):
    def __init__(self):
        super().__init__(load_dll('v2/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v2/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))

class V3(Engine):
    def __init__(self):
        super().__init__(load_dll('v3/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v3/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))

class V31(Engine):
    def __init__(self):
        super().__init__(load_dll('v3.1/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v3.1/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))

class V32(Engine):
    def __init__(self):
        super().__init__(load_dll('v3.2/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v3.2/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))

class Latest(Engine):
    def __init__(self):
        super().__init__(load_dll("../../cmake-build-release/libfrecker.so"))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, '../')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))
