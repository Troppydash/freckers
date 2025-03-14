#include <iostream>
#include <fstream>
#include <map>
#include "board.h"
#include "engine.h"

struct instance {
    int last_score;
    board::move last_move;
    board::pos last_pos;
    std::vector<board::move> last_moves;
};

extern "C" {
std::map<int, instance> instances;
int next_handle = 0;

int make_instance() {
    instances[next_handle] = {0, {}, {}, {}};
    next_handle += 1;
    return next_handle - 1;
}

void free_instance(int handle) {
    instances.erase(handle);
}

/// ENGINE ///

void play(int handle, uint64_t lily, uint64_t red, uint64_t blue, int turn, int moves, int ts, bool verbose) {
    board::pos pos{lily, red, blue, turn, moves};
    engine::computer engine{pos};
    instances[handle].last_move = engine.search(ts, &instances[handle].last_score, verbose);
}

int get_last_score(int handle) {
    return instances[handle].last_score;
}

uint64_t get_last_move_start(int handle) {
    return instances[handle].last_move.m_start;
}

uint64_t get_last_move_end(int handle) {
    return instances[handle].last_move.m_end;
}

uint64_t get_last_move_grow(int handle) {
    return instances[handle].last_move.m_grow;
}

/// BOARD ///

void pos_default(int handle) {
    instances[handle].last_pos = board::pos{};
}

void pos_load(int handle, uint64_t lily, uint64_t red, uint64_t blue, int turn, int moves) {
    instances[handle].last_pos = board::pos{lily, red, blue, turn, moves};
}

void pos_display(int handle) {
    std::cout << instances[handle].last_pos.display() << std::endl;
}

void pos_push(int handle, uint64_t grow, uint64_t start, uint64_t end) {
    board::move m{grow, start, end};
    instances[handle].last_pos.push(m);
}

void pos_pop(int handle, uint64_t grow, uint64_t start, uint64_t end) {
    board::move m{grow, start, end};
    instances[handle].last_pos.pop(m);
}

int pos_state(int handle) {
    return instances[handle].last_pos.get_state();
}

uint64_t pos_lily(int handle) {
    return instances[handle].last_pos.m_lilypads;
}

uint64_t pos_red(int handle) {
    return instances[handle].last_pos.m_players[board::RED];
}

uint64_t pos_blue(int handle) {
    return instances[handle].last_pos.m_players[board::BLUE];
}

int pos_moves(int handle) {
    return instances[handle].last_pos.m_moves;
}

int pos_turn(int handle) {
    return instances[handle].last_pos.m_turn;
}

void pos_compute_moves(int handle) {
    instances[handle].last_moves = instances[handle].last_pos.get_moves();
}

int pos_moves_length(int handle) {
    return instances[handle].last_moves.size();
}

uint64_t pos_moves_start(int handle, int i) {
    return instances[handle].last_moves[i].m_start;
}

uint64_t pos_moves_end(int handle, int i) {
    return instances[handle].last_moves[i].m_end;
}

uint64_t pos_moves_grow(int handle, int i) {
    return instances[handle].last_moves[i].m_grow;
}


}


int randint(int high) {
    return rand() % high;
}

void test_position() {
    std::string file = "../positions.txt";
    std::ifstream buf{file};

    std::string tmp;
    while (buf >> tmp) {
        std::string board;
        for (int i = 0; i < 10; ++i) {
            std::string newline;
            std::getline(buf, newline);
            board += newline + "\n";
        }

        std::string turn;
        buf >> turn;
        int t = board::NONE;
        if (turn == "RED") {
            t = board::RED;
        } else if (turn == "BLUE") {
            t = board::BLUE;
        }

        int win;
        buf >> win;

        board::pos pos = board::pos::from_string(board, t);
        std::cout << "[board]\n" << pos.display() << "\n";
        engine::computer engine{pos};
        engine.search(3000, nullptr, true);
        std::cout << "[real] " << win << "\n\n";

    }

//
//    std::map<std::string, std::pair<int, std::string>> test_cases = {
//            {std::string{"0 _ _ _ B _ B . _ \n"
//                         "1 _ _ _ B B _ . _ \n"
//                         "2 . _ _ _ _ _ _ . \n"
//                         "3 . _ _ B . . _ . \n"
//                         "4 . . . . . R B _ \n"
//                         "5 _ _ _ . R . . _ \n"
//                         "6 _ R _ _ . _ R _ \n"
//                         "7 _ R R _ _ _ _ _ \n"
//                         "  0 1 2 3 4 5 6 7"},  {board::RED,  "game in 11"}},
//            {std::string{"0 _ _ _ B _ _ . _ \n"
//                         "1 _ _ _ B B _ . _ \n"
//                         "2 . _ _ _ _ _ _ . \n"
//                         "3 . _ _ B . . _ . \n"
//                         "4 . . . B R R B _ \n"
//                         "5 _ _ _ . _ . . _ \n"
//                         "6 _ R _ _ . _ R _ \n"
//                         "7 _ R R _ _ _ _ _ \n"
//                         "  0 1 2 3 4 5 6 7"},  {board::RED,  "game in 13?"}},
//            {std::string{"0 _ _ _ B _ _ . _ \n"
//                         "1 _ _ _ B B _ . _ \n"
//                         "2 . _ _ _ _ _ _ . \n"
//                         "3 . _ _ B . . _ . \n"
//                         "4 . . . B . R B _ \n"
//                         "5 _ _ _ . R . . _ \n"
//                         "6 _ R _ _ . _ R _ \n"
//                         "7 _ R R _ _ _ _ _ \n"
//                         "  0 1 2 3 4 5 6 7 "}, {board::BLUE, "game in -12?"}}
//
//    };
//
//    for (auto &pair: test_cases) {
//        board::pos pos = board::pos::from_string(pair.first, pair.second.first);
//        std::cout << pos.display() << "\n";
//        engine::computer engine{pos};
//        engine.search(3000, nullptr, true);
//        std::cout << "[actual] " << pair.second.second << "\n\n";
//    }


}

int main() {

    // board::mask m = 0b0011010;  // ctz = 3, clz = 60, popcount = 2
//
//     test_position();
//     return 0;
//    srand(42);

    board::pos pos;


    while (pos.get_state() == board::NONE) {
        if (pos.m_turn == board::RED) {
            std::cout << "RED\n";
        } else {
            std::cout << "BLUE\n";
        }

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

    std::cout << "winner: " << pos.get_state() << "\n";

    return 0;
}
