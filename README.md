# Freckers AI

An nnue engine for the game Freckers.

### Engine

The c++ engine is located at the root of the repository. The weights are contained in the `weights` folder.

The `py` folder hosts the python interface of the network. It contains a selfplay data generator, neural network
trainer, and an elo arena.

### Performance

Reaches 5 million positions searched per second, with a 99% win rate against the non-nnue version.

### Documentation

nnue

- `nnue2.h` contains the implementation
- weights in `weights/nnue.bin` in binary format
- need to adjust the `HIDDEN_LAYER` size on retrain

nnue training

- use `selfplay.py` to generate selfplay dataset in `sessions`
- run `nn2.py` to train new nnue, save results in `models`
    - loss graph in `loss` folder
- run `nnue2weights.py` to select weights to export, into `new.bin`

tuner

- `optimizer.h` has the cmaes optimizer using texel information
- steps are:
    1. run `texel.py` to create dataset
    2. run optimizer to get results
    3. paste results into `engine::computer_config`

elo tester

- run `elo.py` to generate elo graph
- graph saved in `elos` folder

adding new agent

- copy weight and so to `binaries/vblah`
- add new class in `agents.py`