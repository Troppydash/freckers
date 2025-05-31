//
// Created by terry on 4/03/25.
//

#ifndef FRECKER_ENGINE_H
#define FRECKER_ENGINE_H

#include "board.h"
#include "nnue.h"
#include "param.h"
#include "threadpool.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

namespace engine {
    using namespace board;

    class entry {
    public:
        board::hash m_hash = 0;
        int m_depth = 0;
        int m_score = 0;
        move m_best_move = move::null();
        int m_flag = 0;

        std::tuple<int, bool, move> get(board::hash hash, int ply, int depth, int alpha, int beta) const {
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

        void set(pos &pos, board::hash hash, int score, move &best_move, int ply, int depth, int flag) {
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
        board::hash m_size;

        explicit table(size_t size_in_mb) {
            // should prob make this a power of 2
            m_size = size_in_mb * 1024 * 1024 / sizeof(entry);

            for (int i = 0; i < m_size; ++i) {
                m_entries.push_back(entry{});
            }
        }

        entry &probe(board::hash hash) {
            board::hash index = hash % m_size;
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
    private:
        std::chrono::milliseconds m_target;
        bool m_is_stopped = false;
        bool m_forced_stopped = false;

    public:
        void stop() {
            m_forced_stopped = true;
        }

        void unstop() {
            m_forced_stopped = false;
        }

        void start(int ts) {
            m_target = now() + std::chrono::milliseconds(ts);
            m_is_stopped = false;
            m_forced_stopped = false;
        }

        void add(int ts) {
            m_target = m_target + std::chrono::milliseconds(ts);
            m_is_stopped = false;
        }

        bool is_force_stopped() const {
            return m_forced_stopped;
        }

        bool is_stopped() const {
            return m_is_stopped || m_forced_stopped;
        }

        void check() {
            if (m_is_stopped || m_forced_stopped)
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

    class computer_config {
    public:
        std::array<int, 6> m_lmp_margins{};
        int m_lmr_depth;
        int m_lmr_move;
        int m_tempo;

        int m_countermove;
        int m_lily_min;
        int m_lily_scale;
        int m_static_null_move_margin;
        int m_window;

        std::array<int, 9> m_fut_margins{};


        computer_config() {
            m_lmp_margins = {0, 6, 14, 16, 18, 23};
            m_lmr_depth = 3;
            m_lmr_move = 7;
            m_tempo = 55;
            m_static_null_move_margin = 400;
            m_countermove = 7;
            m_lily_min = 3;
            m_lily_scale = 9;
            m_window = 5;
            m_fut_margins = {0, 100, 160, 220, 280, 340, 400, 460, 520};
        }


        void change(std::mt19937_64 &rng) {
            std::uniform_int_distribution<int> dist(2, 6 + 3 + 4 + 1);
            std::uniform_int_distribution<int> perb(0, 1);
            int value = dist(rng);
            int per = perb(rng) * 2 - 1;
            std::cout << value << std::endl;
            if (value <= 6) {
                m_lmp_margins[value - 1] += per;
            } else if (value == 7) {
                m_lmr_depth += per;
            } else if (value == 8) {
                m_lmr_move += per;
            } else if (value == 9) {
                m_tempo += 2 * per;
            } else if (value == 10) {
                m_static_null_move_margin += 10 * per;
            } else if (value == 11) {
                m_countermove += 2 * per;
            } else if (value == 12) {
                m_lily_min += per;
            } else if (value == 13) {
                m_lily_scale += 1 * per;
            } else if (value == 14) {
                m_window += per;
            }
        }

        void display() {
            std::cout << "LMP_MARGIN ";
            for (auto m: m_lmp_margins) {
                std::cout << m << " ";
            }
            std::cout << std::endl;
            std::cout << "LMR " << m_lmr_depth << " " << m_lmr_move << std::endl;
            std::cout << "TEMP " << m_tempo << std::endl;
            std::cout << "Static " << m_static_null_move_margin << std::endl;
            std::cout << "Counter " << m_countermove << std::endl;
            std::cout << "Lily " << m_lily_min << " " << m_lily_scale << std::endl;
            std::cout << "Window " << m_window << std::endl;
        }

        bool operator<(const computer_config &other) const {
            if (m_countermove < other.m_countermove) {
                return true;
            }

            if (m_lmp_margins < other.m_lmp_margins) {
                return true;
            }

            if (m_lily_scale < other.m_lily_scale) {
                return true;
            }

            if (m_lmr_depth < other.m_lmr_depth) {
                return true;
            }

            if (m_window < other.m_window) {
                return true;
            }

            if (m_static_null_move_margin < other.m_static_null_move_margin) {
                return true;
            }

            if (m_lily_min < other.m_lily_min) {
                return true;
            }

            if (m_lmr_move < other.m_lmr_move) {
                return true;
            }

            if (m_tempo < other.m_tempo) {
                return true;
            }

            return false;
        }
    };

    class computer {
    public:
        table &m_tt;
        pos m_pos;
        int m_searched;

        int m_history[2][64][64];
        int m_lmr[50][100];
        move m_killers[100][2];
        move m_counter[2][64][64];
        nnue::seq m_nnue;
        computer_config m_config;

        timer m_timer;

        explicit computer(pos pos, table &tt, timer timer, std::vector<std::string> weights)
            : computer(pos, tt, timer, std::move(weights), computer_config()) {}


        explicit computer(pos pos, table &tt, timer timer, std::vector<std::string> weights, computer_config config)
            : m_tt(tt), m_pos(pos), m_searched(0), m_nnue(weights), m_config(config), m_timer(timer) {
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

            for (auto &i: m_counter) {
                for (auto &j: i) {
                    for (auto &k: j) {
                        k = move::null();
                    }
                }
            }

            for (int depth = 0; depth < 50; ++depth) {
                for (int move = 0; move < 100; ++move) {
                    m_lmr[depth][move] = std::max(1, depth / std::max(1, m_config.m_lmr_depth)) + move / std::max(1, m_config.m_lmr_move);
                }
            }
        }

        int median_piece(uint64_t red, uint64_t blue) {
            int row = 0;
            while (red > 0) {
                int i = __builtin_ctzll(red);
                red ^= 1ull << i;

                row += i / 8;
            }

            while (blue > 0) {
                int i = __builtin_ctzll(blue);
                blue ^= 1ull << i;

                row += 7 - i / 8;
            }

            return row / (2 * 6 * 2);
        }


        int nnue_evaluate() {
            int idx = median_piece(m_pos.m_players[board::RED], m_pos.m_players[board::BLUE]);
            return m_nnue.compute(m_pos.m_turn == board::BLUE, idx);
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

            return distance_heuristic;
        }

        int evaluate() {
            return nnue_evaluate() + m_config.m_tempo;
        }

        std::vector<std::pair<int, int>> score_moves(std::vector<move> &moves, move &pv_move, int ply, const move &prev_move) {
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

                    bool is_red = m_pos.m_turn == board::RED;
                    bool is_endgame = m_pos.num_finished_piece(m_pos.m_turn) >= 3;
                    if (((!is_red && start.first == 0) || (is_red && start.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // don't consider the move if we started at end, but only if num finished is below 3 so no shuffling
                        score += 0;
                    } else if (((!is_red && end.first == 0) || (is_red && end.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // do consider the move if we will finish at end
                        score += param::base_score + param::end_move_score;
                    } else if (vgap == 0) {
                        score += 0;
                    } else {
                        score += param::base_score + vgap;
                    }
                } else if (move == m_killers[ply][0]) {
                    score += param::base_score + param::killer_move_score;
                } else if (move == m_killers[ply][1]) {
                    score += param::base_score + param::killer_move_score2;
                } else if (!move.is_grow()) {
                    auto start = bitboard::get_coord(move.m_start);
                    auto end = bitboard::get_coord(move.m_end);
                    bool is_red = m_pos.m_turn == board::RED;
                    int vgap = abs(start.first - end.first);

                    bool is_endgame = m_pos.num_finished_piece(m_pos.m_turn) >= 3;
                    if (((!is_red && start.first == 0) || (is_red && start.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // don't consider the move if we started at end, but only if num finished is below 3 so no shuffling
                        score += 0;
                    } else if (((!is_red && end.first == 0) || (is_red && end.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // do consider the move if we will finish at end
                        score += param::base_score + param::end_move_score / 2;
                    } else {
                        if (prev_move.is_storable()) {
                            auto coord = prev_move.get_coords();
                            if (move == m_counter[m_pos.m_turn][coord.first][coord.second]) {
                                score += m_config.m_countermove;
                            }
                        }

                        auto coord = move.get_coords();
                        int history = m_history[m_pos.m_turn][coord.first][coord.second];
                        score += history;
                    }
                } else {
                    int count = __builtin_popcountll(move.m_grow);
                    if (count <= m_config.m_lily_min) {
                        score -= 10;
                    } else {
                        score += count * m_config.m_lily_scale;
                    }
                }

                scores.push_back({score, i});
            }

            return scores;
        }

        void sort_scored_moves(std::vector<std::pair<int, int>> &scored_moves, int i) {
            //            int best_score = scored_moves[i].first;
            //            for (int j = i + 1; j < scored_moves.size(); ++j) {
            //                if (scored_moves[j].first > best_score) {
            //                    best_score = scored_moves[j].first;
            //                    swap(scored_moves[i], scored_moves[j]);
            //                }
            //            }

            int best_score = scored_moves[i].first;
            int best_index = i;
            for (int j = i + 1; j < scored_moves.size(); ++j) {
                if (scored_moves[j].first > best_score) {
                    best_score = scored_moves[j].first;
                    best_index = j;
                }
            }

            if (best_index != i) {
                swap(scored_moves[i], scored_moves[best_index]);
            }
        }

        void incr_counter(const move &prev_move, move &move) {
            if (prev_move.is_storable() && move.is_slient()) {
                auto coord = prev_move.get_coords();
                m_counter[m_pos.m_turn][coord.first][coord.second] = move;
            }
        }


        void incr_history(move &move, int depth) {
            if (!move.is_slient())
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
            if (!move.is_slient())
                return;

            auto coord = move.get_coords();

            if (m_history[m_pos.m_turn][coord.first][coord.second] > 0) {
                m_history[m_pos.m_turn][coord.first][coord.second] -= 1;
            }
        }

        void store_killer(int ply, const move &killer) {
            if (killer.is_slient()) {
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

            if (m_timer.is_stopped()) {
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
            auto scored_moves = score_moves(moves, null, max_ply, null);

            std::vector<move> child_pv_line;

            for (int i = 0; i < moves.size(); ++i) {
                sort_scored_moves(scored_moves, i);
                move &move = moves[scored_moves[i].second];

                push_nnue(move);
                m_pos.push(move);

                int score = -qsearch(max_ply, ply + 1, -beta, -alpha, child_pv_line);

                m_pos.pop(move);
                pop_nnue(move);

                if (m_timer.is_stopped()) {
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

        //        bool handle_crossed(int &score, int &turns, int depth, int ply, std::vector<move> &pv_line) {
        //            int results[2] = {0, 0};
        //            board::move best_red_move = board::move::null();
        //            board::move best_blue_move = board::move::null();
        //            m_solver_red.reset(depth);
        //            m_solver_blue.reset(depth);
        //
        //            // use heuristic to find who is first
        //            int winning = evaluate() > 0 ? m_pos.m_turn : 1 - m_pos.m_turn;
        //            if (winning == RED) {
        //                results[0] = m_solver_red.search(m_pos, best_red_move);
        //                if (results[0] == -1) {
        //                    return false;
        //                }
        //
        //                m_timer.check();
        //                if (m_timer.m_is_stopped) {
        //                    return false;
        //                }
        //
        //                m_solver_blue.m_best_depth = results[0];
        //                results[1] = m_solver_blue.search(m_pos, best_blue_move);
        //                if (results[1] == -1) {
        //                    return false;
        //                }
        //            } else {
        //                results[1] = m_solver_blue.search(m_pos, best_blue_move);
        //                if (results[1] == -1) {
        //                    return false;
        //                }
        //
        //                m_timer.check();
        //                if (m_timer.m_is_stopped) {
        //                    return false;
        //                }
        //
        //                m_solver_red.m_best_depth = results[1];
        //                results[0] = m_solver_red.search(m_pos, best_red_move);
        //                if (results[0] == -1) {
        //                    return false;
        //                }
        //            }
        //
        //            m_timer.check();
        //            if (m_timer.m_is_stopped) {
        //                return false;
        //            }
        //
        //            if (m_pos.m_turn == board::RED) {
        //                if (results[board::BLUE] < results[board::RED]) {
        //                    turns = results[board::BLUE] * 2;
        //                    score = -param::inf + ply + turns;
        //                } else {
        //                    turns = results[board::RED] * 2 - 1;
        //                    score = param::inf - ply - turns;
        //                }
        //            } else {
        //                if (results[board::RED] < results[board::BLUE]) {
        //                    turns = results[board::RED] * 2;
        //                    score = -param::inf + ply + turns;
        //                } else {
        //                    turns = results[board::BLUE] * 2 - 1;
        //                    score = param::inf - ply - turns;
        //                }
        //            }
        //
        //            return true;
        //        }


        int negamax(int depth, int ply, int alpha, int beta, std::vector<move> &pv_line, const move &prev_move, bool do_null = true) {
            m_searched += 1;

            if (m_searched % 2048 == 0) {
                m_timer.check();
            }

            if (m_timer.is_stopped()) {
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

            //            if (!m_pos.has_move()) {
            //                depth += 1;
            //            }

            bool is_root = ply == 0;
            bool is_pv_node = (beta - alpha) != 1;


            if (depth <= 0) {
                return qsearch(ply, 0, alpha, beta, pv_line);
            }

            auto &entry = m_tt.probe(m_pos.get_hash());
            auto [tt_score, should_use, tt_move] = entry.get(m_pos.get_hash(), ply, depth, alpha, beta);
            if (should_use && !is_root) {
                return tt_score;
            }

            //            if (!is_root && !is_pv_node && m_pos.has_crossed() && depth >= 7 && abs(evaluate()) <= 10 * 100) {
            //                int best_score = 0;
            //                int turns = 0;
            //                bool ok = handle_crossed(best_score, turns, depth, ply, pv_line);
            //                m_astar_searched += m_solver_red.m_counter + m_solver_blue.m_counter;
            //
            //                if (ok) {
            //                    move null = move::null();
            //                    entry.set(m_pos, m_pos.hash(), best_score, null, ply, 1e9, param::exact_flag);
            //
            //                    return best_score;
            //                }
            //            }


            // static null move pruning
            if (!is_pv_node && abs(beta) < param::checkmate && !m_pos.has_jumps()) {
                int stat = evaluate();
                int score_margin = depth * m_config.m_static_null_move_margin;
                if ((stat - score_margin) >= beta) {
                    return stat - score_margin;
                }
            }

            // null move pruning
            int remain = m_pos.num_unfinished_piece();
            int growth_count = m_pos.growth_count();
            if (do_null && !is_pv_node && depth >= 2 && remain > 2 && growth_count <= 8) {
                move null = move::null();
                m_pos.push(null);

                int r = 3;
                std::vector<move> child_pv_line;
                int score = -negamax(depth - 1 - r, ply + 1, -beta, -beta + 1, child_pv_line, null, false);
                m_pos.pop(null);

                if (m_timer.is_stopped()) {
                    return 0;
                }

                if (score >= beta) {
                    return beta;
                }
            }


            // razoring
            // if static eval is really bad, check via qsearch to see if it fails-low
            if (depth <= 4 && !is_pv_node && !m_pos.has_jumps()) {
                int static_score = evaluate();
                if (static_score + depth * 400 < alpha) {
                    std::vector<move> line;
                    int score = qsearch(ply, 0, alpha, beta, line);
                    if (score < alpha) {
                        return alpha;
                    }
                }
            }

            // futility pruning, when static is much worse than alpha, likely not a cut-node, so prune every quiet-move except the pv node
            bool canFutilityPrune = false;
            if (depth <= 8 && !is_pv_node && alpha < param::checkmate && beta < param::checkmate && !m_pos.has_jumps()) {
                int static_score = evaluate();
                canFutilityPrune = static_score + depth * 300 <= alpha;
            }


            std::vector<move> child_pv_line;

            // internal ID
            //            if (depth >= 4 && (is_pv_node || entry.m_flag == param::beta_flag) && tt_move.is_null()) {
            //                negamax(depth - 2 - 1, ply + 1, -beta, -alpha, child_pv_line, move::null());
            //
            //                if (m_timer.m_is_stopped) {
            //                    return 0;
            //                }
            //
            //                if (!child_pv_line.empty()) {
            //                    tt_move = child_pv_line[0];
            //                    child_pv_line.clear();
            //                }
            //            }

            // lazily compute the list of moves, since the tt move likely causes the beta cutoff
            std::vector<move> moves;
            std::vector<std::pair<int, int>> scored_moves;
            if (tt_move.is_null()) {
                moves = m_pos.get_moves();
                scored_moves = score_moves(moves, tt_move, ply, prev_move);
            } else {
                moves = {tt_move};
                scored_moves = {{0, 0}};
            }


            int best_score = -param::inf;
            move best_move = move::null();
            int tt_flag = param::alpha_flag;

            for (int i = 0; i < moves.size(); ++i) {
                sort_scored_moves(scored_moves, i);
                move &move = moves[scored_moves[i].second];

                int explored_moves = i + 1;

                push_nnue(move);
                m_pos.push(move);

                // lmp
                if (depth <= 5 && !is_pv_node && explored_moves > m_config.m_lmp_margins[depth] && !move.is_jump()) {
                    bool tactical = m_pos.has_jumps();
                    if (!tactical) {
                        m_pos.pop(move);
                        pop_nnue(move);
                        continue;
                    }
                }

                if (canFutilityPrune && explored_moves > 1 && !move.is_jump()) {
                    bool tactical = m_pos.has_jumps();
                    if (!tactical) {
                        m_pos.pop(move);
                        pop_nnue(move);
                        continue;
                    }
                }


                int score;
                if (explored_moves == 1) {
                    score = -negamax(depth - 1, ply + 1, -beta, -alpha, child_pv_line, move);
                } else {
                    int reduction = 0;
                    if (!is_pv_node && explored_moves >= 3 && depth >= 3) {
                        reduction = m_lmr[depth][explored_moves];
                    }

                    score = -negamax(depth - 1 - reduction, ply + 1, -(alpha + 1), -alpha, child_pv_line, move);
                    if (score > alpha && reduction > 0) {
                        child_pv_line.clear();
                        score = -negamax(depth - 1, ply + 1, -(alpha + 1), -alpha, child_pv_line, move);
                        if (score > alpha) {
                            child_pv_line.clear();
                            score = -negamax(depth - 1, ply + 1, -beta, -alpha, child_pv_line, move);
                        }
                    } else if (alpha < score && score < beta) {
                        child_pv_line.clear();
                        score = -negamax(depth - 1, ply + 1, -beta, -alpha, child_pv_line, move);
                    }
                }
                m_pos.pop(move);
                pop_nnue(move);

                if (m_timer.is_stopped()) {
                    return 0;
                }

                if (score > best_score) {
                    best_score = score;
                    best_move = move;
                }

                if (score >= beta) {
                    tt_flag = param::beta_flag;
                    incr_history(move, depth);
                    incr_counter(prev_move, move);
                    store_killer(ply, move);
                    break;
                } else {
                    //                    decr_history(move);
                }

                if (score > alpha) {
                    alpha = score;
                    tt_flag = param::exact_flag;
                    pv_line.clear();
                    pv_line.push_back(move);
                    for (auto m: child_pv_line) {
                        pv_line.push_back(m);
                    }
                    //                    incr_history(move, depth);
                } else {
                    //                    decr_history(move);
                }

                child_pv_line.clear();

                // compute full list if tt failed to beta cutoff
                if (explored_moves == 1 && !tt_move.is_null()) {
                    moves = m_pos.get_moves();
                    scored_moves = score_moves(moves, tt_move, ply, prev_move);

                    // ignore tt move, since this will always get the tt move first
                    sort_scored_moves(scored_moves, 0);
                }
            }

            if (depth > entry.m_depth && !m_timer.is_stopped()) {
                entry.set(m_pos, m_pos.get_hash(), best_score, best_move, ply, depth, tt_flag);
            }

            return best_score;
        }


        std::pair<std::vector<int32_t>, std::vector<int32_t>> init_nnue() {
            std::vector<int32_t> red;
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

            std::vector<int32_t> blue;
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

        void reset(board::pos position) {
            m_pos = position;
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

            for (auto &i: m_counter) {
                for (auto &j: i) {
                    for (auto &k: j) {
                        k = move::null();
                    }
                }
            }

            // reset tt
            for (auto &entry: m_tt.m_entries) {
                entry.m_hash = 0;
            }
        }

        void init() {
            m_searched = 0;
            auto [red, blue] = init_nnue();
            m_nnue.init(red, blue);
        }

        //        move search(int ts, int *new_score, bool verbose, int max_depth = param::max_depth) {
        //            move best_move = move::null();
        //            int alpha = -param::inf;
        //            int beta = param::inf;
        //            std::vector<move> pv_line;
        //            int depth = 1;
        //
        //            m_timer.start(ts);
        //
        //            m_searched = 0;
        //
        //            std::chrono::milliseconds last = m_timer.now();
        //            int last_searched = m_searched;
        //
        //            auto [red, blue] = init_nnue();
        //            m_nnue.init(red, blue);
        //
        //            bool extended = false;
        //
        //            while (depth <= max_depth) {
        //                pv_line.clear();
        //                move null = move::null();
        //                int score = negamax(depth, 0, alpha, beta, pv_line, null);
        //
        //                bool ok = alpha < score && score < beta;
        //                if (!pv_line.empty()) {
        //                    if (m_timer.m_is_stopped) {
        //                        if (alpha != -param::inf) {
        //                            best_move = pv_line[0];
        //                        }
        //                    } else if (ok) {
        //                        best_move = pv_line[0];
        //                    }
        //                }
        //
        //
        //                if (verbose) {
        //                    std::chrono::milliseconds now = m_timer.now();
        //                    long delta = (now - last).count();
        //                    int nps = (m_searched - last_searched) / std::max(1, (int) delta) * 1000;
        //                    printf("[info] depth %2d, nodes %10d + %10d, value %10s (%7d), nps %10d, occ %.2lf%%, ", depth, m_searched, m_astar_searched,
        //                           get_score(score).c_str(), score, nps, m_tt.occupied() * 100.0);
        //
        //                    printf("pv = [ ");
        //                    for (auto m: pv_line) {
        //                        std::cout << m.display() << ", ";
        //                    }
        //                    printf("]\n");
        //
        //                    last = now;
        //                    last_searched = m_searched;
        //                }
        //
        //
        //                if (m_timer.m_is_stopped) {
        //                    break;
        //                }
        //
        //                if (!ok) {
        //                    alpha = -param::inf;
        //                    beta = param::inf;
        //
        //                    // restart
        //                    if (depth >= 6 && !extended) {
        //                        m_timer.add(ts / 2);
        //                        extended = true;
        //                    }
        //
        //                    continue;
        //                }
        //
        //
        //                if (new_score != nullptr) {
        //                    *new_score = score;
        //                }
        //
        //                alpha = score - m_config.m_window * 100;
        //                beta = score + m_config.m_window * 100;
        //
        //                depth += 1;
        //            }
        //
        //            if (verbose) {
        //                printf("nodes %d\n", m_searched);
        //            }
        //            return best_move;
        //        }
    };


    class lazysmp_thread_context {
    public:
        int i;
        engine::computer &computer;
        int alpha;
        int beta;
        int depth;
        std::vector<move> &best_move;
        int &score;
        int &depths;
    };

    class lazysmp {
    public:
        timer m_timer;
        int m_threads;
        threadpool m_pool;
        table m_tt;

        explicit lazysmp(int threads)
            // need one less thread since we are the main thread
            : m_threads(threads), m_pool(), m_tt(1024) {
        }


        std::string get_score(int score) {
            if (score > param::checkmate) {
                int ply = param::inf - score;
                return std::string{"IN "} + std::to_string(ply) + " ply";
            }

            if (score < -param::checkmate) {
                int ply = -param::inf - score;
                return std::string{"In "} + std::to_string(ply) + " ply";
            }

            std::stringstream stream;
            stream << std::fixed << std::setprecision(2) << (double) score / 100;
            return stream.str();
        }


        move search(pos pos, int ts, int *new_score, const std::vector<std::string> &weights, const computer_config &config, bool verbose, int max_depth = param::max_depth) {
            ts += ts / 2;
            m_timer.start(ts);

            // setup
            std::vector<computer> computers;

            for (int i = 0; i < m_threads; ++i) {
                computer comp{pos, m_tt, m_timer, weights, config};
                comp.init();
                computers.emplace_back(std::move(comp));
            }
            int root_thread = 0;

            m_timer.start(ts);

            if (verbose)
                printf("searching with %d threads\n", m_threads);

            // search depth one fully using the first comp
            std::vector<move> pv_line;
            move null = move::null();
            int alpha = -param::inf;
            int beta = param::inf;
            int depth = 1;
            int score = computers[root_thread].negamax(depth, 0, alpha, beta, pv_line, null);
            depth += 1;
            move best_move = pv_line[0];
            if (new_score != nullptr)
                *new_score = score;
            alpha = score - config.m_window * 100;
            beta = score + config.m_window * 100;

            std::vector<int> scores(m_threads);
            std::vector<std::vector<move>> best_moves(m_threads);
            std::vector<int> depths(m_threads);


            std::atomic<int> finished_tasks = 0;
            std::atomic<bool> is_finished = false;

            while (depth <= max_depth) {
                for (int i = 0; i < m_threads; ++i) {
                    computers[i].m_timer = m_timer;
                    computers[i].m_pos = pos;
                    computers[i].m_timer.unstop();
                }

                // start helper search
                is_finished = false;
                finished_tasks = 0;
                for (int i = 0; i < m_threads; ++i) {
                    computers[i].m_pos = pos;

                    if (i == root_thread) {
                        continue;
                    }

                    lazysmp_thread_context context{
                            i,
                            computers[i],
                            alpha,
                            beta,
                            depth + i % 2,
                            best_moves[i],
                            scores[i],
                            depths[i]};
                    int threads = m_threads;
                    m_pool.enqueue([context, &finished_tasks, &is_finished, threads]() {
                        int alpha = context.alpha;
                        int beta = context.beta;
                        while (true) {
                            std::vector<move> pv_line;
                            move null = move::null();
                            int score = context.computer.negamax(context.depth, 0,alpha, beta, pv_line, null);

                            if (context.computer.m_timer.is_stopped()) {
                                context.best_move = {};
                                context.depths = 0;
                                context.score = 0;
                                break;
                            } else {
                                // useless eval if outside the asp window, so why not research
                                if (score <= alpha || score >= beta) {
                                    alpha = -param::inf;
                                    beta = -param::inf;
                                    continue;
                                }

                                context.best_move = pv_line;
                                context.depths = context.depth;
                                context.score = score;
                                break;
                            }
                        }


                        finished_tasks += 1;
                        if (finished_tasks == threads - 1) {
                            is_finished = true;
                            is_finished.notify_one();
                        }
                    });
                }

                // run root search
                lazysmp_thread_context context{
                        root_thread,
                        computers[root_thread],
                        alpha,
                        beta,
                        depth + root_thread % 2,
                        best_moves[root_thread],
                        scores[root_thread],
                        depths[root_thread]};
                std::vector<move> pv_line;
                move null = move::null();
                int score = context.computer.negamax(context.depth, 0, context.alpha, context.beta, pv_line, null);
                context.score = score;
                context.best_move = pv_line;
                context.depths = context.depth;

                // when root search stopped,
                // stop the other engines
                for (int i = 0; i < m_threads; ++i) {
                    computers[i].m_timer.stop();
                }

                // wait until threads finished
                is_finished.wait(false);

                // check if timer exceeds
                m_timer.check();
                if (m_timer.is_stopped()) {
                    break;
                }

                // check aspiration
                //                for (int i = 0; i < m_threads; ++i) {
                //                    int score = scores[i];
                //                }
                if (score <= alpha || score >= beta) {
                    // re-search
                    alpha = -param::inf;
                    beta = param::inf;
                    continue;
                }

                int worse_score = param::inf;
                std::map<move, int> vote;

                for (int i = 0; i < m_threads; ++i) {
                    if (!best_moves[i].empty() && scores[i] > alpha && scores[i] < beta) {
                        worse_score = std::min(worse_score, scores[i]);
                        vote[best_moves[i][0]] = 0;
                    }
                }

                for (int i = 0; i < m_threads; ++i) {
                    // compute thread value, favor depth than score
                    if (!best_moves[i].empty() && scores[i] > alpha && scores[i] < beta) {
                        int threadvalue = (scores[i] - worse_score) + 200*depths[i];
                        vote[best_moves[i][0]] += threadvalue;
                    }
                }

                int best_thread = root_thread;
                int best_score = scores[root_thread];
                int best_vote_score = vote[best_moves[root_thread][0]];

                for (int i = 0; i < m_threads; ++i) {
                    if (i == root_thread) {
                        continue;
                    }

                    if (best_moves[i].empty() || !(scores[i] > alpha && scores[i] < beta)) {
                        continue;
                    }

                    int current_score = scores[i];
                    int current_vote_score = vote[best_moves[i][0]];

                    // choose the fastest mate
                    if (std::abs(best_score) >= param::checkmate) {
                        if (current_score > best_score) {
                            best_thread = i;
                            best_score = current_score;
                            best_vote_score = current_vote_score;
                        }
                    } else if (current_vote_score > -param::checkmate) {
                        if (current_vote_score > best_vote_score) {
                            best_thread = i;
                            best_score = current_score;
                            best_vote_score = current_vote_score;
                        }
                    }
                }


                root_thread = best_thread;

                // update and stuff
                if (new_score != nullptr)
                    *new_score = best_score;
                best_move = best_moves[root_thread][0];

                if (verbose) {
                    int nodes = 0;
                    for (int i = 0; i < m_threads; ++i) {
                        nodes += computers[i].m_searched;
                    }
                    printf("nodes %10d, depth %2d, value %7s (%5d), occ %.2lf%%", nodes, depth, get_score(score).c_str(), score, m_tt.occupied() * 100.0);

                    printf(", pv = [");
                    for (auto move: best_moves[root_thread]) {
                        printf("%s, ", move.display().c_str());
                    }
                    printf("]\n");
                }

                alpha = best_score - config.m_window * 100;
                beta = best_score + config.m_window * 100;

                depth += 1;
            }

            if (verbose) {
                int nodes = 0;
                for (int i = 0; i < m_threads; ++i) {
                    nodes += computers[i].m_searched;
                }

                printf("total nodes searched %10d\n", nodes);
            }


            return best_move;
        }
    };

    class analysis {
    public:
        int m_depth = 1;
        int m_score = 0;
        computer m_computer;
        std::vector<move> m_line;

        explicit analysis(computer &&computer)
            : m_computer(std::move(computer)) {

            auto [red, blue] = m_computer.init_nnue();
            m_computer.m_nnue.init(red, blue);

            m_computer.m_timer.start(10000);
        }

        int ponder() {
            if (m_computer.m_timer.is_stopped()) {
                return m_score;
            }

            int alpha = -param::inf;
            int beta = param::inf;
            m_line.clear();
            move null = move::null();
            int score = m_computer.negamax(m_depth, 0, alpha, beta, m_line, null);
            if (m_computer.m_timer.is_stopped()) {
                return m_score;
            }

            m_score = score;
            m_depth++;
            return m_score;
        }
    };
}// namespace engine


#endif//FRECKER_ENGINE_H
