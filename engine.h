//
// Created by terry on 4/03/25.
//

#ifndef FRECKER_ENGINE_H
#define FRECKER_ENGINE_H

#include <cinttypes>
#include <vector>
#include <tuple>
#include <chrono>
#include <iostream>
#include "board.h"
#include "param.h"

namespace engine {
    using namespace board;

    class entry {
    public:
        uint64_t m_hash = 0;
        int m_depth = 0;
        int m_score = 0;
        move m_best_move = move::null();
        int m_flag = 0;

        std::tuple<int, bool, move> get(uint64_t hash, int ply, int depth, int alpha, int beta) {
            int adj_score = 0;
            bool should_use = false;
            move best_move = move::null();

            if (m_hash == hash) {
                best_move = m_best_move;
                adj_score = m_score;

                if (m_depth >= depth) {
                    int score = m_score;
                    if (score > param::checkmate)
                        score -= ply;
                    if (score < -param::checkmate) {
                        score += ply;
                    }

                    if (m_flag == param::exact_flag) {
                        adj_score = score;
                        should_use = true;
                    } else if (m_flag == param::alpha_flag && score <= alpha) {
                        adj_score = alpha;
                        should_use = true;
                    } else if (m_flag == param::beta_flag && score >= beta) {
                        adj_score = beta;
                        should_use = true;
                    }
                }
            }

            return {adj_score, should_use, best_move};
        }

        void set(uint64_t hash, int score, move &best_move, int ply, int depth, int flag) {
            m_hash = hash;
            m_depth = depth;
            m_best_move = best_move;
            m_flag = flag;
            if (score > param::checkmate) {
                score += ply;
            }

            if (score < -param::checkmate) {
                score -= ply;
            }

            m_score = score;
        }
    };

    class table {
    public:
        std::vector<entry> m_entries;
        uint64_t m_size;

        explicit table(int size_in_mb) {
            m_size = size_in_mb * 1024 * 1024 / sizeof(entry);

            for (int i = 0; i < m_size; ++i) {
                m_entries.push_back(entry{});
            }
        }

        entry &probe(uint64_t hash) {
            uint64_t index = hash % m_size;
            return m_entries[index];
        }
    };

    class timer {
    public:
        std::chrono::milliseconds m_target;
        bool m_is_stopped;

        void start(int ts) {
            m_target = now() + std::chrono::milliseconds(ts);
            m_is_stopped = false;
        }

        void check() {
            if (m_is_stopped)
                return;

            auto current = now();
            if (current > m_target) {
                m_is_stopped = true;
            }
        }

        std::chrono::milliseconds now() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch());
        }
    };

    class computer {
    public:
        table m_tt;
        pos m_pos;
        timer m_timer;
        int m_searched;


        explicit computer(pos pos)
                : m_tt(128), m_pos(pos), m_searched(0), m_timer() {
        }

        int evaluate() {

            int red_total = 6 * 7;
            int blue_total = 6 * 7;
            board::mask m = m_pos.m_players[board::RED];
            while (m > 0) {
                mask piece = 1ull << (bitboard::ROWS * bitboard::COLS - __builtin_clzll(m) - 1);
                m ^= piece;

                auto coord = bitboard::get_coord(piece);
                red_total -= (7 - coord.first);
            }

            m = m_pos.m_players[board::BLUE];
            while (m > 0) {
                mask piece = 1ull << (bitboard::ROWS * bitboard::COLS - __builtin_clzll(m) - 1);
                m ^= piece;

                auto coord = bitboard::get_coord(piece);
                blue_total -= coord.first;
            }

            if (m_pos.m_turn == board::RED) {
                return (red_total - blue_total + 3) * 100;
            } else {
                return (blue_total - red_total + 3) * 100;
            }
        }

        std::vector<std::pair<int, int>> score_moves(std::vector<move> &moves, move &pv_move) {
            std::vector<std::pair<int, int>> scores;
            for (int i = 0; i < moves.size(); ++i) {
                int score = 0;
                move &move = moves[i];

                if (move == pv_move) {
                    score += param::pv_move_score;
                } else {
//                    int history = m_history[m_pos.m_turn][0][move.m_file];
//                    score += history;
                }

                scores.push_back({score, i});
            }

            return scores;
        }

        void sort_scored_moves(std::vector<std::pair<int, int>> &scored_moves, int i) {
            int best_score = scored_moves[i].first;
            for (int j = i + 1; j < scored_moves.size(); ++j) {
                if (scored_moves[j].first > best_score) {
                    best_score = scored_moves[j].first;
                    swap(scored_moves[i], scored_moves[j]);
                }
            }
        }

        int negamax(int depth, int ply, int alpha, int beta, std::vector<move> &pv_line) {
            m_searched += 1;

            if (m_searched % 2048 == 0) {
                m_timer.check();
            }

            if (m_timer.m_is_stopped) {
                return 0;
            }

            int state = m_pos.get_state();
            if (state == board::DRAW) {
                return 0;
            } else if (state != board::NONE) {
                return -param::inf + ply;
            }

            if (ply >= param::max_depth) {
                return evaluate();
            }

            bool is_root = ply == 0;
            bool is_pv_node = (beta - alpha) != 1;

            if (depth <= 0) {
                return evaluate();
            }

            auto &entry = m_tt.probe(m_pos.hash());
            auto [tt_score, should_use, tt_move] = entry.get(m_pos.hash(), ply, depth, alpha, beta);
            if (should_use && !is_root) {
                return tt_score;
            }

            auto moves = m_pos.get_moves();
            auto scored_moves = score_moves(moves, tt_move);

            std::vector<move> child_pv_line;
            int best_score = -param::inf;
            move best_move = move::null();
            int tt_flag = param::alpha_flag;

            for (int i = 0; i < moves.size(); ++i) {
                sort_scored_moves(scored_moves, i);
                move &move = moves[scored_moves[i].second];


                m_pos.push(move);

                int score;
                score = -negamax(depth - 1, ply + 1, -beta, -alpha, child_pv_line);

                m_pos.pop(move);

                if (score > best_score) {
                    best_score = score;
                    best_move = move;
                }

                if (score >= beta) {
                    tt_flag = param::beta_flag;
                    break;
                }

                if (score > alpha) {
                    alpha = score;
                    tt_flag = param::exact_flag;
                    pv_line.clear();
                    pv_line.push_back(move);
                    for (auto m: child_pv_line) {
                        pv_line.push_back(m);
                    }
                }

                child_pv_line.clear();

            }

            if (depth > entry.m_depth && !m_timer.m_is_stopped) {
                entry.set(m_pos.hash(), best_score, best_move, ply, depth, tt_flag);
            }

            return best_score;
        }

        std::string get_score(int score) {
            if (score > param::checkmate) {
                int ply = param::inf - score;
                return std::string{"mate in "} + std::to_string(ply) + " ply";
            }

            if (score < -param::checkmate) {
                int ply = -param::inf - score;
                return std::string{"mate in "} + std::to_string(ply) + " ply";
            }

            return std::to_string((double) score / 100);
        }

        move search(int ts, int *new_score, bool verbose) {
            move best_move = move::null();
            int alpha = -param::inf;
            int beta = param::inf;
            std::vector<move> pv_line;
            int depth = 1;

            m_timer.start(ts);


            std::chrono::milliseconds last = m_timer.now();
            int last_searched = m_searched;

            while (depth <= param::max_depth) {
                pv_line.clear();
                int score = negamax(depth, 0, alpha, beta, pv_line);

                if (m_timer.m_is_stopped) {
                    if (best_move.is_null() && depth == 1) {
                        best_move = pv_line[0];
                    }
                    break;
                }

                if (new_score != nullptr) {
                    *new_score = score;
                }

                best_move = pv_line[0];

                if (verbose) {
                    std::chrono::milliseconds now = m_timer.now();
                    long delta = (now - last).count();
                    int nps = (m_searched - last_searched) / std::max(1, (int) delta) * 1000;
                    printf("[info] depth %d, nodes %d, value %s (%d), nps %d, ", depth, m_searched,
                           get_score(score).c_str(), score, nps);

                    printf("pv = [ ");
                    for (auto m: pv_line) {
                        std::cout << m.display() << ", ";
                    }
                    printf("]\n");

                    last = now;
                    last_searched = m_searched;
                }


                depth += 1;
            }

            if (verbose) {
                printf("nodes %d\n", m_searched);
            }
            return best_move;
        }
    };
}


#endif //FRECKER_ENGINE_H
