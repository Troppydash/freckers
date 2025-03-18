# Py SelfPlay Reinforcement Learning Environment

## NN Topology
First layer contains two accumulative layers consist of (leaf_square, red_frog), (leaf_square, blue_frog).
The input size is 64 x 64 x 2.

The second layer is a combination layer where the first 64 inputs are from the player to move, and the second 64
are from the player to not move.

Normal sequential linear layers of size 32, size 16, size 1 follows.