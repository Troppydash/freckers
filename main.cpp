#include <iostream>
#include "board.h"
#include "engine.h"

int randint(int high) {
    return rand() % high;
}

int main() {
    srand(42);

    board::pos pos;

    while (pos.get_state() == board::NONE) {
        std::cout << pos.display() << std::endl;

        if (pos.m_turn == board::RED) {
            auto moves = pos.get_moves();
            auto move = moves[randint(moves.size())];
            std::cout << "move " << move.display() << std::endl;
            pos.push(move);
        } else {
            engine::computer engine{pos};
            auto move = engine.search(1000, nullptr, true);
            std::cout << "move " << move.display() << std::endl;
            pos.push(move);
        }

        std::cout << "\n";
    }

    return 0;
}
