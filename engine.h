//
// Created by terry on 4/03/25.
//

#ifndef FRECKER_ENGINE_H
#define FRECKER_ENGINE_H

#include "board.h"
#include "nnue.h"
#include "param.h"
#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <iostream>
#include <tuple>
#include <vector>

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

        void set(pos &pos, uint64_t hash, int score, move &best_move, int ply, int depth, int flag) {
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

        double occupied() {
            int count = 0;
            for (auto &entry: m_entries) {
                if (entry.m_hash != 0) {
                    count++;
                }
            }

            return (double) count / m_entries.size();
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
        int m_astar_searched;

        int m_history[2][64][64];
        int m_lmr[50][100];
        move m_killers[100][2];

        nnue::seq m_nnue;

        endgame::a_star m_solver_red;
        endgame::a_star m_solver_blue;

        explicit computer(pos pos, std::vector<std::string> weights)
            : m_tt(128), m_pos(pos), m_timer(), m_searched(0), m_astar_searched(0), m_nnue(weights),
              m_solver_red(board::RED), m_solver_blue(board::BLUE) {
            for (auto &i: m_history) {
                for (auto &j: i) {
                    for (int &k: j) {
                        k = 0;
                    }
                }
            }

            for (auto &m: m_killers) {
                m[0] = move::null();
                m[1] = move::null();
            }

            for (int depth = 0; depth < 50; ++depth) {
                for (int move = 0; move < 100; ++move) {
                    m_lmr[depth][move] = std::max(1, depth / 4) + move / 4;
                }
            }
        }

        int nnue_evaluate() {
            return m_nnue.compute(m_pos.m_turn == board::BLUE);
        }

        int classical_evaluate() {
            int v_scores[] = {0, 1, 2, 3, 5, 8, 13, 21};
            int h_scores[] = {0, 0, 0, 0, 0, 0, 0, 0};
            // compute distance heuristic
            // shorter dist to end the better
            int red_total = 0;
            int blue_total = 0;

            board::mask m = m_pos.m_players[board::RED];
            while (m > 0) {
                mask piece = 1ull << (bitboard::ROWS * bitboard::COLS - __builtin_clzll(m) - 1);
                m ^= piece;

                auto coord = bitboard::get_coord(piece);
                red_total += v_scores[coord.first] + h_scores[coord.second];
            }

            m = m_pos.m_players[board::BLUE];
            while (m > 0) {
                mask piece = 1ull << (bitboard::ROWS * bitboard::COLS - __builtin_clzll(m) - 1);
                m ^= piece;

                auto coord = bitboard::get_coord(piece);
                blue_total += v_scores[7 - coord.first] + h_scores[coord.second];
            }

            int distance_heuristic = 0;
            if (m_pos.m_turn == board::RED) {
                distance_heuristic = (red_total - blue_total) * 100;
            } else {
                distance_heuristic = (blue_total - red_total) * 100;
            }

            return distance_heuristic + 100;
        }

        int evaluate() {
            int tempo = 0;
            return nnue_evaluate() + tempo * 100;
        }

        std::vector<std::pair<int, int>> score_moves(std::vector<move> &moves, move &pv_move, int ply) {
            //            std::vector<std::pair<int, int>> scores;
            //            for (int i = 0; i < moves.size(); ++i) {
            //                int score = 0;
            //                move &move = moves[i];
            //
            //                if (move == pv_move) {
            //                    score += param::base_score + param::pv_move_score;
            //                } else if (move == m_killers[ply][0]) {
            //                    score += param::base_score + param::killer_move_score;
            //                } else if (!move.is_grow()) {
            //                    // end game rankings
            //                    auto start = bitboard::get_coord(move.m_start);
            //                    auto end = bitboard::get_coord(move.m_end);
            //                    auto coord = move.get_coords();
            //                    int vgap = abs(start.first - end.first);
            //                    int hgap = abs(start.second - end.second);
            //
            //                    bool is_red = m_pos.m_turn == board::RED;
            //                    bool is_endgame = m_pos.num_finished_piece(m_pos.m_turn) >= 4;
            //                    if (((!is_red && start.first == 0) || (is_red && start.first == bitboard::ROWS - 1)) && !is_endgame) {
            //                        // don't consider the move if we started at end, but only if num finished is below 3 so no shuffling
            //                        score += 0;
            //                    } else if (((!is_red && end.first == 0) || (is_red && end.first == bitboard::ROWS - 1)) && !is_endgame) {
            //                        // do consider the move if we will finish at end
            //                        score += param::base_score + param::end_move_score;
            //                    } else if (vgap >= 2) {
            //                        score += param::base_score + vgap * 10;
            //                    } else if (hgap >= 2) {
            //                        // dont consider large h moves
            //                        score += 0;
            //                    } else {
            //                        int history = m_history[m_pos.m_turn][coord.first][coord.second];
            //                        score += history;
            //                    }
            //                } else {
            //                    int count = __builtin_popcountll(move.m_grow);
            //                    if (count <= 1) {
            //                        score -= 10;
            //                    } else {
            //                        score += count * 10;
            //                    }
            //                }
            //
            //                scores.push_back({score, i});
            //            }

            std::vector<std::pair<int, int>> scores;
            for (int i = 0; i < moves.size(); ++i) {
                int score = 0;
                move &move = moves[i];

                if (move == pv_move) {
                    score += param::base_score + param::pv_move_score;
                } else if (move.is_jump()) {
                    auto start = bitboard::get_coord(move.m_start);
                    auto end = bitboard::get_coord(move.m_end);
                    int vgap = abs(start.first - end.first);
                    int hgap = abs(start.second - end.second);

                    bool is_red = m_pos.m_turn == board::RED;
                    bool is_endgame = m_pos.num_finished_piece(m_pos.m_turn) >= 4;
                    if (((!is_red && start.first == 0) || (is_red && start.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // don't consider the move if we started at end, but only if num finished is below 3 so no shuffling
                        score += 0;
                    } else if (((!is_red && end.first == 0) || (is_red && end.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // do consider the move if we will finish at end
                        score += param::base_score + param::end_move_score;
                    } else {
                        score += param::base_score + 20 * vgap + hgap;
                    }
                } else if (move == m_killers[ply][0]) {
                    score += param::base_score + param::killer_move_score;
                } else if (move == m_killers[ply][1]) {
                    score += param::base_score + param::killer_move_score2;
                } else if (!move.is_grow()) {
                    auto start = bitboard::get_coord(move.m_start);
                    auto end = bitboard::get_coord(move.m_end);
                    bool is_red = m_pos.m_turn == board::RED;
                    bool is_endgame = m_pos.num_finished_piece(m_pos.m_turn) >= 4;
                    if (((!is_red && start.first == 0) || (is_red && start.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // don't consider the move if we started at end, but only if num finished is below 3 so no shuffling
                        score += 0;
                    } else if (((!is_red && end.first == 0) || (is_red && end.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // do consider the move if we will finish at end
                        score += param::base_score + param::end_move_score;
                    } else {
                        // end game rankings
                        auto coord = move.get_coords();
                        int history = m_history[m_pos.m_turn][coord.first][coord.second];
                        score += history;
                    }
                } else {
                    int count = __builtin_popcountll(move.m_grow);
                    if (count <= 1) {
                        score -= 10;
                    } else {
                        score += count * 10;
                    }
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

            //            int best_score = scored_moves[i].first;
            //            int best_index = i;
            //            for (int j = i + 1; j < scored_moves.size(); ++j) {
            //                if (scored_moves[j].first > best_score) {
            //                    best_score = scored_moves[j].first;
            //                    best_index = j;
            //                }
            //            }
            //
            //            if (best_index != i) {
            //                swap(scored_moves[i], scored_moves[best_index]);
            //            }
        }


        void incr_history(move &move, int depth) {
            if (move.is_grow())
                return;

            auto coord = move.get_coords();
            m_history[m_pos.m_turn][coord.first][coord.second] += depth * depth;
            if (m_history[m_pos.m_turn][coord.first][coord.second] > param::base_score) {
                for (auto &i: m_history) {
                    for (auto &j: i) {
                        for (int &k: j) {
                            k /= 2;
                        }
                    }
                }
            }
        }

        void decr_history(move &move) {
            if (move.is_grow())
                return;

            auto coord = move.get_coords();

            if (m_history[m_pos.m_turn][coord.first][coord.second] > 0) {
                m_history[m_pos.m_turn][coord.first][coord.second] -= 1;
            }
        }

        void store_killer(int ply, const move &killer) {
            if (!killer.is_jump()) {
                if (m_killers[ply][0] != killer) {
                    m_killers[ply][1] = m_killers[ply][0];
                    m_killers[ply][0] = killer;
                }
            }
        }

        void push_nnue(move &move) {
            if (move.is_null()) {
                return;
            }

            if (move.is_grow()) {
                board::mask m = move.m_grow;
                while (m > 0) {
                    int index = (bitboard::ROWS * bitboard::COLS - __builtin_clzll(m) - 1);
                    mask piece = 1ull << index;
                    m ^= piece;

                    m_nnue.push_red(index);
                    m_nnue.push_blue(63 - index);
                }
            } else if (m_pos.m_turn == board::RED) {
                int i = __builtin_ctzll(move.m_start);
                int j = __builtin_ctzll(move.m_end);

                m_nnue.push_red(64 + j);
                m_nnue.pop_red(64 + i);

                m_nnue.pop_red(i);
                m_nnue.pop_blue(63 - i);
            } else {
                int i = __builtin_ctzll(move.m_start);
                int j = __builtin_ctzll(move.m_end);

                m_nnue.push_blue(64 + 63 - j);
                m_nnue.pop_blue(64 + 63 - i);

                m_nnue.pop_red(i);
                m_nnue.pop_blue(63 - i);
            }
        }

        void pop_nnue(move &move) {
            if (move.is_null()) {
                return;
            }

            if (move.is_grow()) {

                board::mask m = move.m_grow;
                while (m > 0) {
                    int index = (bitboard::ROWS * bitboard::COLS - __builtin_clzll(m) - 1);
                    mask piece = 1ull << index;
                    m ^= piece;

                    m_nnue.pop_red(index);
                    m_nnue.pop_blue(63 - index);
                }
            } else if (m_pos.m_turn == board::RED) {
                int i = __builtin_ctzll(move.m_start);
                int j = __builtin_ctzll(move.m_end);

                m_nnue.pop_red(64 + j);
                m_nnue.push_red(64 + i);

                m_nnue.push_red(i);
                m_nnue.push_blue(63 - i);
            } else {
                int i = __builtin_ctzll(move.m_start);
                int j = __builtin_ctzll(move.m_end);

                m_nnue.pop_blue(64 + 63 - j);
                m_nnue.push_blue(64 + 63 - i);

                m_nnue.push_red(i);
                m_nnue.push_blue(63 - i);
            }
        }

        int qsearch(int max_ply, int ply, int alpha, int beta, std::vector<move> &pv_line) {
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
            } else if (state == m_pos.m_turn) {
                return param::inf - ply;
            } else if (state == 1 - m_pos.m_turn) {
                return -param::inf + ply;
            }

            if ((max_ply + ply) >= param::max_depth) {
                return evaluate();
            }

            int best_score = evaluate();

            if (best_score >= beta) {
                return best_score;
            }
            if (best_score > alpha) {
                alpha = best_score;
            }

            std::vector<move> moves = m_pos.get_jump_moves();
            auto null = move::null();
            auto scored_moves = score_moves(moves, null, max_ply);

            std::vector<move> child_pv_line;

            for (int i = 0; i < moves.size(); ++i) {
                sort_scored_moves(scored_moves, i);
                move &move = moves[scored_moves[i].second];

                push_nnue(move);
                m_pos.push(move);

                int score = -qsearch(max_ply, ply + 1, -beta, -alpha, child_pv_line);

                m_pos.pop(move);
                pop_nnue(move);

                if (m_timer.m_is_stopped) {
                    return 0;
                }

                if (score > best_score) {
                    best_score = score;
                }

                if (score >= beta) {
                    break;
                }

                if (score > alpha) {
                    alpha = score;
                    pv_line.clear();
                    pv_line.push_back(move);
                    for (auto m: child_pv_line) {
                        pv_line.push_back(m);
                    }
                }

                child_pv_line.clear();
            }

            return best_score;
        }

        bool handle_crossed(int &score, int &turns, int depth, int ply, std::vector<move> &pv_line) {
            int results[2] = {0, 0};
            board::move best_red_move = board::move::null();
            board::move best_blue_move = board::move::null();
            m_solver_red.reset(depth);
            m_solver_blue.reset(depth);

            // use heuristic to find who is first
            if (endgame::heuristic(m_pos, board::RED) > endgame::heuristic(m_pos, board::BLUE)) {
                results[0] = m_solver_red.search(m_pos, best_red_move);
                if (results[0] == -1) {
                    return false;
                }

                m_timer.check();
                if (m_timer.m_is_stopped) {
                    return false;
                }

                m_solver_blue.m_best_depth = results[0];
                results[1] = m_solver_blue.search(m_pos, best_blue_move);
                if (results[1] == -1) {
                    return false;
                }
            } else {
                results[1] = m_solver_blue.search(m_pos, best_blue_move);
                if (results[1] == -1) {
                    return false;
                }

                m_timer.check();
                if (m_timer.m_is_stopped) {
                    return false;
                }

                m_solver_red.m_best_depth = results[1];
                results[0] = m_solver_red.search(m_pos, best_red_move);
                if (results[0] == -1) {
                    return false;
                }
            }

            m_timer.check();
            if (m_timer.m_is_stopped) {
                return false;
            }

            // todo: this might push_back null
            //            if (m_pos.m_turn == board::RED) {
            //                pv_line.push_back(best_red_move);
            //            } else {
            //                pv_line.push_back(best_blue_move);
            //            }

            if (m_pos.m_turn == board::RED) {
                if (results[board::BLUE] < results[board::RED]) {
                    turns = results[board::BLUE] * 2;
                    score = -param::inf + ply + turns;
                } else {
                    turns = results[board::RED] * 2 - 1;
                    score = param::inf - ply - turns;
                }
            } else {
                if (results[board::RED] < results[board::BLUE]) {
                    turns = results[board::RED] * 2;
                    score = -param::inf + ply + turns;
                } else {
                    turns = results[board::BLUE] * 2 - 1;
                    score = param::inf - ply - turns;
                }
            }

            return true;
        }


        int negamax(int depth, int ply, int alpha, int beta, std::vector<move> &pv_line, bool do_null = true) {
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
            } else if (state == m_pos.m_turn) {
                return param::inf - ply;
            } else if (state == 1 - m_pos.m_turn) {
                return -param::inf + ply;
            }

            if (ply >= param::max_depth) {
                return evaluate();
            }

            bool is_root = ply == 0;
            bool is_pv_node = (beta - alpha) != 1;

            if (depth <= 0) {
                return qsearch(ply, 0, alpha, beta, pv_line);
            }

            auto &entry = m_tt.probe(m_pos.hash());
            auto [tt_score, should_use, tt_move] = entry.get(m_pos.hash(), ply, depth, alpha, beta);
            if (should_use && !is_root) {
                return tt_score;
            }

            //            if (!is_root && !is_pv_node && m_pos.has_crossed() && depth >= 5) {
            //                int best_score = 0;
            //                int turns = 0;
            //                bool ok = handle_crossed(best_score, turns, depth, ply, pv_line);
            //                m_astar_searched += m_solver_red.m_counter + m_solver_blue.m_counter;
            //
            //                if (ok) {
            //                    move null = move::null();
            //                    entry.set(m_pos.hash(), best_score, null, ply, 1e9, param::exact_flag);
            //
            //                    return best_score;
            //                }
            //            }


            // null move pruning
            if (do_null && !is_pv_node && depth >= 4 && m_pos.num_unfinished_piece() >= 4 && evaluate() >= beta) {
                move null = move::null();
                m_pos.push(null);

                int r = 3;
                std::vector<move> child_pv_line;
                int score = -negamax(depth - 1 - r, ply + 1, -beta, -beta + 1, child_pv_line, false);
                m_pos.pop(null);

                if (m_timer.m_is_stopped) {
                    return 0;
                }

                if (score >= beta) {
                    return beta;
                }
            }

            auto moves = m_pos.get_moves();
            auto scored_moves = score_moves(moves, tt_move, ply);

            std::vector<move> child_pv_line;
            int best_score = -param::inf;
            move best_move = move::null();
            int tt_flag = param::alpha_flag;

            for (int i = 0; i < moves.size(); ++i) {
                sort_scored_moves(scored_moves, i);
                move &move = moves[scored_moves[i].second];

                push_nnue(move);
                m_pos.push(move);

                int explored_moves = i + 1;
                int score;
                if (explored_moves == 1) {
                    score = -negamax(depth - 1, ply + 1, -beta, -alpha, child_pv_line);
                } else {
                    int reduction = 0;
                    if (!is_pv_node && m_pos.get_jump_moves().empty() && explored_moves >= 3 && depth >= 3) {
                        reduction = m_lmr[depth][explored_moves];
                    }

                    score = -negamax(depth - 1 - reduction, ply + 1, -(alpha + 1), -alpha, child_pv_line);
                    if (score > alpha && reduction > 0) {
                        child_pv_line.clear();
                        score = -negamax(depth - 1, ply + 1, -(alpha + 1), -alpha, child_pv_line);
                        if (score > alpha) {
                            child_pv_line.clear();
                            score = -negamax(depth - 1, ply + 1, -beta, -alpha, child_pv_line);
                        }
                    } else if (alpha < score && score < beta) {
                        child_pv_line.clear();
                        score = -negamax(depth - 1, ply + 1, -beta, -alpha, child_pv_line);
                    }
                }
                m_pos.pop(move);
                pop_nnue(move);

                if (m_timer.m_is_stopped) {
                    return 0;
                }

                if (score > best_score) {
                    best_score = score;
                    best_move = move;
                }

                if (score >= beta) {
                    tt_flag = param::beta_flag;
                    incr_history(move, depth);
                    store_killer(ply, move);
                    break;
                } else {
                    decr_history(move);
                }

                if (score > alpha) {
                    alpha = score;
                    tt_flag = param::exact_flag;
                    pv_line.clear();
                    pv_line.push_back(move);
                    for (auto m: child_pv_line) {
                        pv_line.push_back(m);
                    }
                    incr_history(move, depth);
                } else {
                    decr_history(move);
                }

                child_pv_line.clear();
            }

            if (depth > entry.m_depth && !m_timer.m_is_stopped) {
                entry.set(m_pos, m_pos.hash(), best_score, best_move, ply, depth, tt_flag);
            }

            return best_score;
        }

        std::string get_score(int score) {
            if (score > param::checkmate) {
                int ply = param::inf - score;
                return std::string{"game in "} + std::to_string(ply) + " ply";
            }

            if (score < -param::checkmate) {
                int ply = -param::inf - score;
                return std::string{"game in "} + std::to_string(ply) + " ply";
            }

            return std::to_string((double) score / 100);
        }

        std::pair<std::vector<int16_t>, std::vector<int16_t>> init_nnue() {
            std::vector<int16_t> red;
            for (int i = 0; i < 64; ++i) {
                if (m_pos.m_lilypads & (1ull << i)) {
                    red.push_back(1);
                } else {
                    red.push_back(0);
                }
            }

            for (int i = 0; i < 64; ++i) {
                if (m_pos.m_players[0] & (1ull << i)) {
                    red.push_back(1);
                } else {
                    red.push_back(0);
                }
            }

            std::vector<int16_t> blue;
            for (int i = 0; i < 64; ++i) {
                if (m_pos.m_lilypads & (1ull << (63 - i))) {
                    blue.push_back(1);
                } else {
                    blue.push_back(0);
                }
            }

            for (int i = 0; i < 64; ++i) {
                if (m_pos.m_players[1] & (1ull << (63 - i))) {
                    blue.push_back(1);
                } else {
                    blue.push_back(0);
                }
            }

            return {red, blue};
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

            auto [red, blue] = init_nnue();
            m_nnue.init(red, blue);

            while (depth <= param::max_depth) {
                pv_line.clear();
                int score = negamax(depth, 0, alpha, beta, pv_line);

                if (!pv_line.empty()) {
                    best_move = pv_line[0];
                }

                if (verbose) {
                    std::chrono::milliseconds now = m_timer.now();
                    long delta = (now - last).count();
                    int nps = (m_searched - last_searched) / std::max(1, (int) delta) * 1000;
                    printf("[info] depth %2d, nodes %10d + %10d, value %10s (%7d), nps %10d, occ %.2lf%%, ", depth, m_searched, m_astar_searched,
                           get_score(score).c_str(), score, nps, m_tt.occupied() * 100.0);

                    printf("pv = [ ");
                    for (auto m: pv_line) {
                        std::cout << m.display() << ", ";
                    }
                    printf("]\n");

                    last = now;
                    last_searched = m_searched;
                }

                if (m_timer.m_is_stopped) {
                    break;
                }
                if (new_score != nullptr) {
                    *new_score = score;
                }
                depth += 1;
            }

            if (verbose) {
                printf("nodes %d\n", m_searched);
            }
            return best_move;
        }
    };
}// namespace engine


#endif//FRECKER_ENGINE_H
