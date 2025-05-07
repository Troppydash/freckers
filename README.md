# Freckers AI

An nnue engine for the game Freckers.

### Engine
The c++ engine is located at the root of the repository. The weights are contained in the `weights` folder.

The `py` folder hosts the python interface of the network. It contains a selfplay data generator, neural network trainer, and an elo arena.

### Performance
Reaches 5 million positions searched per second, with a 99% win rate against the non-nnue version.