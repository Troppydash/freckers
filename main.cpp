#include "board.h"
#include "endgame.h"
#include "engine.h"
#include <fstream>
#include <iostream>
#include <map>

std::vector<std::string> get_weights(const std::string &base) {
    return {base + "./weights/weight_1.txt", base + "./weights/weight_2.txt", base + "./weights/weight_3.txt", base + "./weights/weight_4.txt"};
}


struct instance {
    int last_score;
    board::move last_move;
    board::pos last_pos;
    std::vector<board::move> last_moves;
    std::string weights = "../";
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
void set_weights(int handle, char *base) {
    std::cout << "[cpp] setting weights to " << base << std::endl;
    instances[handle].weights = std::string{base};
}

void play(int handle, uint64_t lily, uint64_t red, uint64_t blue, int turn, int moves, int ts, bool verbose) {
    board::pos pos{lily, red, blue, turn, moves};
    engine::computer engine{pos, get_weights(instances[handle].weights)};
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

bool pos_has_jumps(int handle) {
    return instances[handle].last_pos.has_jumps();
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
        engine::computer engine{pos, get_weights("../")};
        engine.search(10000, nullptr, true);
        std::cout << "[real] " << win << "\n\n";
    }
}

void test_a_star_position() {
    std::string file = "../endgame_positions.txt";
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

        endgame::a_star engine{board::BLUE, 0, 0};
        board::move best_move;
        int result = engine.search(pos, best_move);
        std::cout << "[search] " << result << "\n";
        std::cout << "[search] searched " << engine.m_counter << "\n";
        std::cout << "[real] " << win << "\n\n";
    }
}


int main() {
//    test_position();
//    return 0;

    board::pos pos;

    while (pos.get_state() == board::NONE) {
        if (pos.m_turn == board::RED) {
            std::cout << "RED\n";
        } else {
            std::cout << "BLUE\n";
        }

        std::cout << pos.display() << std::endl;

        if (pos.m_turn == board::RED) {
            engine::computer engine{pos, get_weights("../")};
            auto move = engine.search(2000, nullptr, true);
            std::cout << "move " << move.display() << std::endl;
            pos.push(move);

        } else {
            engine::computer engine{pos, get_weights("../")};
            auto move = engine.search(2000, nullptr, true);
            std::cout << "move " << move.display() << std::endl;
            pos.push(move);
        }

        std::cout << "\n";
    }

    std::cout << "winner: " << pos.get_state() << "\n";

    return 0;
}
