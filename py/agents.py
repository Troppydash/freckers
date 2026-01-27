import ctypes
import os
import random
import shutil
import tempfile
import uuid

from engine import Engine, Pos, Move


def load_dll(name):
    dirname = os.path.dirname(__file__)
    lib_file = os.path.join(dirname, f"./binaries/{name}")

    rand = uuid.uuid4()
    temp_dir = tempfile.gettempdir()
    temp_file = os.path.join(temp_dir, f"{rand}.so")
    shutil.copy2(lib_file, temp_file)

    cpp = ctypes.cdll.LoadLibrary(temp_file)
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


class V4(Engine):
    def __init__(self):
        super().__init__(load_dll('v4/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v4/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class V5(Engine):
    def __init__(self):
        super().__init__(load_dll('v5/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v5/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class V6(Engine):
    def __init__(self):
        super().__init__(load_dll('v6/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v6/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class V61(Engine):
    def __init__(self):
        super().__init__(load_dll('v61/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v61/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class V62(Engine):
    def __init__(self):
        super().__init__(load_dll('v62/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v62/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class V7(Engine):
    def __init__(self):
        super().__init__(load_dll('v7/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v7/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class V71(Engine):
    def __init__(self):
        super().__init__(load_dll('v71/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v71/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class V72(Engine):
    def __init__(self):
        super().__init__(load_dll('v72/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v72/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class V73(Engine):
    def __init__(self):
        super().__init__(load_dll('v73/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v73/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))

    def play(self, game: Pos, ts: int, verbose: bool) -> tuple[Move, int]:
        # fix ts 2/3 bug
        return super().play(game, int(ts * 2 / 3), verbose)


class V74(Engine):
    def __init__(self):
        super().__init__(load_dll('v74/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v74/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class V75(Engine):
    def __init__(self):
        super().__init__(load_dll('v75/libfrecker.so'))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, './binaries/v75/')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))


class Latest(Engine):
    def __init__(self):
        super().__init__(load_dll("../../cmake-build-release/libfrecker.so"))

        dirname = os.path.dirname(__file__)
        weights = os.path.join(dirname, '../')
        self.cpp.set_weights(self.handle, ctypes.c_char_p(str.encode(weights)))

    def play(self, game: Pos, ts: int, verbose: bool) -> tuple[Move, int]:
        return super().play(game, ts, verbose)

