#include "board.h"
#include "engine.h"
#include <fstream>
#include <iostream>

extern "C" {
int last_score = 0;

board::move last_move;

void play(uint64_t lily, uint64_t red, uint64_t blue, int turn, int moves, int ts, bool verbose) {
    board::pos pos{lily, red, blue, turn, moves};
    engine::computer engine{pos};
    auto result = engine.search(ts, &last_score, verbose);


    last_move = result;
}

int get_last_score() {
    return last_score;
}

uint64_t get_last_move_start() {
    return last_move.m_start;
}

uint64_t get_last_move_end() {
    return last_move.m_end;
}

uint64_t get_last_move_grow() {
    return last_move.m_grow;
}

board::pos last_pos;
std::vector<board::move> last_moves;

void pos_default() {
    last_pos = board::pos{};
}

void pos_load(uint64_t lily, uint64_t red, uint64_t blue, int turn, int moves) {
    last_pos = board::pos{lily, red, blue, turn, moves};
}

void pos_display() {
    std::cout << last_pos.display() << std::endl;
}

void pos_push(uint64_t grow, uint64_t start, uint64_t end) {
    board::move m{grow, start, end};
    last_pos.push(m);
}

void pos_pop(uint64_t grow, uint64_t start, uint64_t end) {
    board::move m{grow, start, end};
    last_pos.pop(m);
}

int pos_state() {
    return last_pos.get_state();
}

uint64_t pos_lily() {
    return last_pos.m_lilypads;
}

uint64_t pos_red() {
    return last_pos.m_players[board::RED];
}

uint64_t pos_blue() {
    return last_pos.m_players[board::BLUE];
}

int pos_moves() {
    return last_pos.m_moves;
}

int pos_turn() {
    return last_pos.m_turn;
}

void pos_compute_moves() {
    last_moves = last_pos.get_moves();
}

int pos_moves_length() {
    return last_moves.size();
}

uint64_t pos_moves_start(int i) {
    return last_moves[i].m_start;
}

uint64_t pos_moves_end(int i) {
    return last_moves[i].m_end;
}

uint64_t pos_moves_grow(int i) {
    return last_moves[i].m_grow;
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
        std::cout << "[board]\n"
                  << pos.display() << "\n";
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
