import ctypes
import os

from engine import Engine

def load_dll(name):
    dirname = os.path.dirname(__file__)
    lib_file = os.path.join(dirname, f"./binaries/{name}")
    cpp = ctypes.cdll.LoadLibrary(lib_file)
    return cpp

class V0(Engine):
    def __init__(self):
        super().__init__(load_dll("v0.so"))

class Latest(Engine):
    def __init__(self):
        super().__init__(load_dll("../../cmake-build-release/libfrecker.so"))
