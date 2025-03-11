#include <iostream>
#include <map>
#include "board.h"
#include "engine.h"


extern "C" {
int last_score = 0;

uint64_t last_move_start = 0;
uint64_t last_move_end = 0;
bool last_move_grow = false;

void play(uint64_t lily, uint64_t red, uint64_t blue, int turn, int moves, int ts, bool verbose) {
    board::pos pos{lily, red, blue, turn, moves};
    engine::computer engine{pos};
    auto result = engine.search(ts, &last_score, verbose);

    if (result.is_grow()) {
        last_move_grow = true;
    } else {
        last_move_start = result.m_start;
        last_move_end = result.m_end;
    }
}

int get_last_score() {
    return last_score;
}

uint64_t get_last_move_start() {
    return last_move_start;
}

uint64_t get_last_move_end() {
    return last_move_end;
}

bool get_last_move_grow() {
    return last_move_grow;
}

}


int randint(int high) {
    return rand() % high;
}

void test_position() {
    std::map<std::string, std::pair<int, std::string>> test_cases = {
            {std::string{"0 _ _ _ B _ B . _ \n"
                         "1 _ _ _ B B _ . _ \n"
                         "2 . _ _ _ _ _ _ . \n"
                         "3 . _ _ B . . _ . \n"
                         "4 . . . . . R B _ \n"
                         "5 _ _ _ . R . . _ \n"
                         "6 _ R _ _ . _ R _ \n"
                         "7 _ R R _ _ _ _ _ \n"
                         "  0 1 2 3 4 5 6 7"},  {board::RED,  "game in 11"}},
            {std::string{"0 _ _ _ B _ _ . _ \n"
                         "1 _ _ _ B B _ . _ \n"
                         "2 . _ _ _ _ _ _ . \n"
                         "3 . _ _ B . . _ . \n"
                         "4 . . . B R R B _ \n"
                         "5 _ _ _ . _ . . _ \n"
                         "6 _ R _ _ . _ R _ \n"
                         "7 _ R R _ _ _ _ _ \n"
                         "  0 1 2 3 4 5 6 7"},  {board::RED,  "game in 13?"}},
            {std::string{"0 _ _ _ B _ _ . _ \n"
                         "1 _ _ _ B B _ . _ \n"
                         "2 . _ _ _ _ _ _ . \n"
                         "3 . _ _ B . . _ . \n"
                         "4 . . . B . R B _ \n"
                         "5 _ _ _ . R . . _ \n"
                         "6 _ R _ _ . _ R _ \n"
                         "7 _ R R _ _ _ _ _ \n"
                         "  0 1 2 3 4 5 6 7 "}, {board::BLUE, "game in -12?"}}

    };

    for (auto &pair: test_cases) {
        board::pos pos = board::pos::from_string(pair.first, pair.second.first);
        std::cout << pos.display() << "\n";
        engine::computer engine{pos};
        engine.search(1000, nullptr, true);
        std::cout << "[actual] " << pair.second.second << "\n\n";
    }


}

int main() {
//    test_position();
//    return 0;
    srand(42);

    board::pos pos;


    while (pos.get_state() == board::NONE) {
        std::cout << pos.display() << std::endl;

        if (pos.m_turn == board::RED) {
            engine::computer engine{pos};
            auto move = engine.search(1000, nullptr, true);
            std::cout << "move " << move.display() << std::endl;
            pos.push(move);
        } else {
//            board::move goal;
//            for (auto m : pos.get_moves()) {
//                if (m.is_grow()) {
//                    goal = m;
//                    break;
//                }
//            }
//            pos.push(goal);


            engine::computer engine{pos};
            auto move = engine.search(1000, nullptr, true);
            std::cout << "move " << move.display() << std::endl;
            pos.push(move);
        }

        std::cout << "\n";
    }

    std::cout << "winner: " << pos.get_state() << "\n";

    return 0;
}
