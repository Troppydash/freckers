#include "board.h"
#include "endgame.h"
#include "engine.h"

#include <atomic>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <thread>

std::vector<std::string> get_weights(const std::string &base) {
    return {base + "./weights/weight_1.txt", base + "./weights/weight_2.txt", base + "./weights/weight_3.txt", base + "./weights/weight_4.txt"};
}

std::vector<std::string> get_weights_direct(const std::string &base) {
    return {base + "weight_1.txt", base + "weight_2.txt", base + "weight_3.txt", base + "weight_4.txt"};
}


struct instance {
    int last_score;
    board::move last_move;
    board::pos last_pos;
    std::vector<board::move> last_moves;
    std::string weights = "../";
    engine::analysis *analysis;
};


extern "C" {
std::mutex lock;
std::unordered_map<int, instance> instances;
int next_handle = 0;

int make_instance() {
    lock.lock();
    instances[next_handle] = {0, board::move{}, {}, {}, "../", nullptr};
    next_handle += 1;
    int ret = next_handle - 1;
    lock.unlock();
    return ret;
}

void free_instance(int handle) {
    lock.lock();
    instances.erase(handle);
    lock.unlock();
}

/// ENGINE ///
void set_weights(int handle, char *base) {
    lock.lock();

    std::cout << "[cpp] setting weights to " << base << std::endl;
    instances[handle].weights = std::string{base};

    lock.unlock();
}

void play(int handle, uint64_t lily, uint64_t red, uint64_t blue, int turn, int moves, int ts, bool verbose) {
    lock.lock();

    board::pos pos{lily, red, blue, turn, moves};
    engine::lazysmp engine{1};
    instances[handle].last_move = engine.search(pos, ts, &instances[handle].last_score, get_weights(instances[handle].weights), engine::computer_config{}, verbose);

    lock.unlock();
}

void play_board(int handle, int ts, int verbose) {
    lock.lock();

    engine::lazysmp engine{1};
    instances[handle].last_move = engine.search(instances[handle].last_pos, ts, &instances[handle].last_score, get_weights(instances[handle].weights), engine::computer_config{}, verbose);

    lock.unlock();
}


void start_ponder(int handle) {
    if (instances[handle].analysis != nullptr) {
        delete instances[handle].analysis;
    }

    throw std::runtime_error("not supported");
    //
    //
    // engine::computer engine{instances[handle].last_pos, get_weights_direct(instances[handle].weights)};
    // auto *analysis = new engine::analysis{std::move(engine)};
    // instances[handle].analysis = analysis;
}

int ponder_once(int handle) {
    if (instances[handle].analysis == nullptr) {
        return 0;
    }

    return instances[handle].analysis->ponder();
}

int ponder_depth(int handle) {
    if (instances[handle].analysis == nullptr) {
        return 0;
    }
    return instances[handle].analysis->m_depth;
}

uint64_t ponder_start(int handle) {
    return instances[handle].analysis->m_line[0].m_start;
}

uint64_t ponder_end(int handle) {
    return instances[handle].analysis->m_line[0].m_end;
}

uint64_t ponder_grow(int handle) {
    return instances[handle].analysis->m_line[0].m_grow;
}

int get_last_score(int handle) {
    lock.lock();
    auto ret = instances[handle].last_score;
    lock.unlock();
    return ret;
}

uint64_t get_last_move_start(int handle) {
    lock.lock();
    auto ret = instances[handle].last_move.m_start;
    lock.unlock();
    return ret;
}

uint64_t get_last_move_end(int handle) {
    lock.lock();
    auto ret = instances[handle].last_move.m_end;
    lock.unlock();
    return ret;
}

uint64_t get_last_move_grow(int handle) {
    lock.lock();
    auto ret = instances[handle].last_move.m_grow;
    lock.unlock();
    return ret;
}

/// BOARD ///

void pos_default(int handle) {
    lock.lock();
    instances[handle].last_pos = board::pos{};
    lock.unlock();
}

void pos_load(int handle, uint64_t lily, uint64_t red, uint64_t blue, int turn, int moves) {
    lock.lock();
    instances[handle].last_pos = board::pos{lily, red, blue, turn, moves};
    lock.unlock();
}

void pos_display(int handle) {
    lock.lock();
    std::cout << instances[handle].last_pos.display() << std::endl;
    lock.unlock();
}

void pos_push(int handle, uint64_t grow, uint64_t start, uint64_t end) {
    lock.lock();
    board::move m{grow, start, end};
    instances[handle].last_pos.push(m);
    lock.unlock();
}

void pos_pop(int handle, uint64_t grow, uint64_t start, uint64_t end) {
    lock.lock();
    board::move m{grow, start, end};
    instances[handle].last_pos.pop(m);
    lock.unlock();
}

int pos_state(int handle) {
    lock.lock();
    auto ret = instances[handle].last_pos.get_state();
    lock.unlock();
    return ret;
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
    lock.lock();
    instances[handle].last_moves = instances[handle].last_pos.get_moves();
    lock.unlock();
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
    lock.lock();
    auto res = instances[handle].last_pos.has_jumps();
    lock.unlock();
    return res;
}
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

        engine::lazysmp engine{8};
        engine.search(pos, 10000, nullptr, get_weights("../"), engine::computer_config{}, true);
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

        endgame::a_star engine{board::BLUE};
        board::move best_move;
        int result = engine.search(pos, best_move);
        std::cout << "[search] " << result << "\n";
        std::cout << "[search] searched " << engine.m_counter << "\n";
        std::cout << "[real] " << win << "\n\n";
    }
}

//
// double score_config(engine::computer_config config, std::vector<std::pair<board::pos, double>> positions) {
//     double total = 0;
//     int counter = 0;
//     engine::computer computer{positions[0].first, get_weights("../"), config};
//
//     std::cout << std::endl;
//     for (auto &[position, eval]: positions) {
//         int estimated_seconds = (positions.size() - counter) * 20 / 1000;
//         std::cout << "\r" << counter << " estimated " << estimated_seconds << " seconds" << std::flush;
//         counter += 1;
//
//         // compute engine
//         int score = 0;
//         computer.reset(position);
//         computer.search(10000, &score, false, 7);
//
//         // map score to logistic score
//         double logistic_score = 2.0 / (1.0 + exp(-score / 1000.0)) - 1.0;
//         if (score > param::checkmate) {
//             logistic_score = 1.0;
//         } else if (score < -param::checkmate) {
//             logistic_score = -1.0;
//         }
//
//         total += (logistic_score - eval) * (logistic_score - eval);
//     }
//
//     total /= static_cast<double>(positions.size());
//     return total;
// }
//
//
// double score_config_parallel(engine::computer_config config, const std::vector<std::pair<board::pos, double>> &positions) {
//     const int max_threads = 14;
//     std::array<double, max_threads> outputs{};
//     std::array<double, max_threads> sizes{};
//     std::vector<std::thread> threads;
//
//     int stride = positions.size() / max_threads;
//     std::vector<std::vector<std::pair<board::pos, double>>> custom_positions(max_threads);
//     for (int thread = 0; thread < max_threads; ++thread) {
//         int start = stride * thread;
//         for (int i = start; i < start + stride && i < positions.size(); ++i) {
//             custom_positions[thread].push_back(positions[i]);
//         }
//     }
//
//     for (int thread = 0; thread < max_threads; ++thread) {
//         int t = thread;
//         std::thread th{[&outputs, &sizes, t, custom_positions, config]() {
//             sizes[t] = custom_positions[t].size();
//             outputs[t] = score_config(config, custom_positions[t]);
//         }};
//         threads.push_back(std::move(th));
//     }
//
//     for (int i = 0; i < max_threads; ++i) {
//         threads[i].join();
//     }
//
//     double total = 0;
//     double total_sizes = 0;
//     for (int i = 0; i < max_threads; ++i) {
//         std::cout << outputs[i] << " " << sizes[i] << std::endl;
//         total += outputs[i] * sizes[i];
//         total_sizes += sizes[i];
//     }
//
//     total /= total_sizes;
//     return total;
// }
//
//
// void texel_tune() {
//     // read positions
//     std::vector<std::pair<board::pos, double>> positions;
//     std::ifstream file("../py/texel.txt");
//     if (file.is_open()) {
//         int amount = 0;
//         file >> amount;
//         for (int i = 0; i < amount; ++i) {
//             uint64_t lilypad, red, blue;
//             int turn;
//             double score;
//
//             file >> lilypad >> red >> blue >> turn >> score;
//             positions.push_back({{lilypad, red, blue, turn, 0},
//                                  score});
//         }
//
//
//         file.close();
//     } else {
//         throw std::runtime_error("cannot open file");
//     }
//
//     std::cout << "loaded " << positions.size() << " positions\n";
//
//     // texel tune
//     std::random_device dev;
//     std::mt19937_64 rng(dev());
//     std::uniform_real_distribution<double> dist(0.0, 1.0);
//
//     engine::computer_config config;
//     double best_score = 0.4;
//     std::map<engine::computer_config, double> cache;
//     engine::computer_config top_config = config;
//     double top_score = 0.4;
//     std::cout << dist(rng) << std::endl;
//     std::cout << dist(rng) << std::endl;
//     while (true) {
//         engine::computer_config new_config = config;
//         new_config.change(rng);
//         double score;
//         if (cache.contains(new_config)) {
//             score = cache[new_config];
//         } else {
//             score = score_config_parallel(new_config, positions);
//             cache[new_config] = score;
//         }
//         printf("global best %.5f, local best %.5f, current %.5f\n", top_score, best_score, score);
//
//         if (score < best_score || dist(rng) < 0.15) {
//             if (score >= best_score) {
//                 std::cout << "SIMANNEALED\n";
//             } else if (score < top_score) {
//                 std::cout << "Improved global\n";
//
//                 top_config = new_config;
//                 top_score = score;
//             } else {
//                 std::cout << "Improved local\n";
//             }
//
//             best_score = score;
//             config = new_config;
//         }
//         config.display();
//         top_config.display();
//     }
// }


int main() {
    //    texel_tune();
    //    return 0;
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
            printf("start\n");
            engine::lazysmp lazy(1);
            auto move = lazy.search(pos, 5000, nullptr, get_weights("../"), engine::computer_config{}, true);
            //            engine::computer engine{pos, get_weights("../")};
            //            auto move = engine.search(2000, nullptr, true);
            std::cout << "move " << move.display() << std::endl;
            pos.push(move);

        } else {
            printf("start\n");
            engine::lazysmp lazy(1);
            auto move = lazy.search(pos, 5000, nullptr, get_weights("../"), engine::computer_config{}, true);
            std::cout << "move " << move.display() << std::endl;
            pos.push(move);
        }

        std::cout << "\n";
    }

    std::cout << "winner: " << pos.get_state() << "\n";

    return 0;
}
