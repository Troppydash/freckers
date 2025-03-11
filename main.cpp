#include <iostream>
#include "board.h"
#include "engine.h"

int randint(int high) {
    return rand() % high;
}

void test_position() {
    std::string state = {"0 _ _ _ B _ . . _ \n"
                         "1 _ B _ _ _ _ . _ \n"
                         "2 _ _ B _ B _ _ . \n"
                         "3 . _ _ _ _ B _ _ \n"
                         "4 . _ _ . _ B _ _ \n"
                         "5 . _ . _ _ _ _ _ \n"
                         "6 _ R . _ . . . _ \n"
                         "7 _ R _ R R R . R \n"
                         "  0 1 2 3 4 5 6 7"};

    board::pos pos = board::pos::from_string(state, board::BLUE);
    engine::computer engine{pos};
    auto move = engine.search(1000, nullptr, true);
    std::cout << "move " << move.display() << std::endl;
}

int main() {
    srand(42);

//    test_position();
    board::pos pos;


    while (pos.get_state() == board::NONE) {
        std::cout << pos.display() << std::endl;

        if (pos.m_turn == board::RED) {
            engine::computer engine{pos};
            auto move = engine.search(1000, nullptr, true);
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
