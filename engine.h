//
// Created by terry on 4/03/25.
//

#ifndef FRECKER_ENGINE_H
#define FRECKER_ENGINE_H

#include "board.h"
#include "nnue.h"
#include "nnue2.h"
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
                        adj_score = score;
                        should_use = true;
                    } else if (m_flag == param::beta_flag && score >= beta) {
                        adj_score = score;
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
        size_t m_size;
        int m_power;
        hash m_mask;

        explicit table(size_t size_in_mb) {
            // make this a power of 2
            size_t max_size = size_in_mb * 1024 * 1024 / sizeof(entry);
            m_power = std::floor(std::log2(max_size));
            m_size = (1ull << m_power);
            m_mask = m_size - 1;

            m_entries.resize(m_size);
        }

        size_t get_index(hash h) const {
            return h & m_mask;
        }

        entry &probe(board::hash hash) {
            // the hash check is done in entry
            return m_entries[get_index(hash)];
        }

        double occupied() const {
            int count = 0;
            for (const auto &entry: m_entries) {
                if (entry.m_hash != 0) {
                    count++;
                }
            }

            return static_cast<double>(count) / m_entries.size();
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
            if (current >= m_target) {
                m_is_stopped = true;
            }
        }

        std::chrono::milliseconds now() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch());
        }
    };


    struct computer_config_static {
        // evaluation features
        static constexpr int m_tempo = 50;
        static constexpr int m_tempo_limit = 1289;
        static constexpr int m_lily_min = 2;
        static constexpr std::array<int, 8> m_lily_range = {0, 56, 750, 1443, 1904, 2412, 2418, 2872};
        static constexpr int m_long_jump_end_move = 78;
        static constexpr int m_short_jump_end_move = 35;
        static constexpr int m_long_jump_vgap_mult = 2;
        static constexpr int m_long_jump_hgap_mult = 0;
        static constexpr int m_jump_endgame = 3;
        static constexpr int m_tt_eval_prop = 92;
        static constexpr int m_countermove = 16;

        // pruning features
        static constexpr std::array<int, 6> m_lmp_margins = {0, 7, 13, 16, 18, 25};
        static constexpr int m_lmr_depth = 4;
        static constexpr int m_lmr_move = 12;
        static constexpr int m_static_null_move_margin = 1000;
        static constexpr std::array<int, 7> m_fut_margins = {0, 444, 937, 1336, 1558, 1646, 2102};
        static constexpr int m_razor_mult = 3;
        static constexpr int m_razor_limit = 6;
        static constexpr int m_nmr_const = 3;
        static constexpr int m_nmr_depth = 5;
        static constexpr int m_nmr_min_depth = 3;
        static constexpr int m_nmr_beta_mult = 34;
        static constexpr int m_iid_depth = 5;
        static constexpr int m_iid_depth_reduction = 3;

        static constexpr int m_window = 500;
        static constexpr int m_window_scale = 3;
    };

#ifdef FRECKER_HARDCODE_CONFIG
#define CONFIG_GET(config, entry) (computer_config_static::entry)

    struct computer_config {};
#else
#define CONFIG_GET(config, entry) (config.entry)

    class computer_config {
    public:
        // m_lmp_margins[depth] is the nodes to search before lmp at depth
        std::array<int, 6> m_lmp_margins{};
        // the depth divisor reduction
        int m_lmr_depth;
        // the move divisor reduction
        int m_lmr_move;
        // extra tempo per move
        int m_tempo;
        // countermove score gain
        int m_countermove;
        // smallest lily gain to ignore lily move
        int m_lily_min;
        // m_lily_range[i+1] = lily move ordering weight if lily_count / 8 = i
        std::array<int, 8> m_lily_range;
        // margin for static null move
        int m_static_null_move_margin;
        // m_fut_margins[depth] is the margin before fut pruning
        std::array<int, 7> m_fut_margins{};
        // the mult on m_fut_margins[depth] to do razoring
        int m_razor_mult;
        // max depth for razoring
        int m_razor_limit;
        // nmr depth reduction const
        int m_nmr_const;
        // nmr depth reduction depth divisor
        int m_nmr_depth;
        // tempo adding limit (inside)
        int m_tempo_limit;
        // long jump to end scoring
        int m_long_jump_end_move;
        // short jump to end scoring
        int m_short_jump_end_move;
        // long jump vgap multiplier scoring
        int m_long_jump_vgap_mult;
        // long jump hgap multiplier scoring
        int m_long_jump_hgap_mult;
        // endgame check for scoring
        int m_jump_endgame;
        // min depth for nmr
        int m_nmr_min_depth;
        // beta scaling for nmr depth reduction
        int m_nmr_beta_mult;
        // min depth for iid
        int m_iid_depth;
        // iid depth reduction amount
        int m_iid_depth_reduction;
        // proportion of eval to consider when correcting with tt_score (x100)
        int m_tt_eval_prop;

        // asp window
        // NOT TUNED
        int m_window;
        // asp window expand scale
        // NOT TUNED
        int m_window_scale;

        computer_config() {
            // m_lmp_margins = {0, 6, 10, 14, 16, 24};
            // m_lmr_depth = 4;
            // m_lmr_move = 12;
            // m_tempo = 50;
            // m_static_null_move_margin = 1000;
            // m_countermove = 20;
            // m_lily_min = 2;
            // m_lily_range = {0, 32, 715, 1407, 1866, 2365, 2369, 2833};
            // m_fut_margins = {0, 423, 862, 1277, 1377, 1729, 2075};
            // m_razor_mult = 3;
            // m_razor_limit = 6;
            // m_nmr_const = 3;
            // m_nmr_depth = 6;
            // m_tempo_limit = 1000;
            // m_long_jump_end_move = 57;
            // m_short_jump_end_move = 31;
            // m_long_jump_vgap_mult = 2;
            // m_long_jump_hgap_mult = 0;
            // m_jump_endgame = 3;
            // m_nmr_min_depth = 3;
            // m_nmr_beta_mult = 25;
            // m_iid_depth = 5;
            // m_iid_depth_reduction = 3;
            // m_tt_eval_prop = 100;

            //oldgood
            // m_lmp_margins = {0, 7, 13, 16, 18, 25};
            // m_lmr_depth = 4;
            // m_lmr_move = 12;
            // m_tempo = 51;
            // m_static_null_move_margin = 1082;
            // m_countermove = 16;
            // m_lily_min = 2;
            // m_lily_range = {0, 56, 750, 1443, 1904, 2412, 2418, 2872};
            // m_fut_margins = {0, 383, 827, 1247, 1397, 1739, 2113};
            // m_razor_mult = 3;
            // m_razor_limit = 6;
            // m_nmr_const = 3;
            // m_nmr_depth = 5;
            // m_tempo_limit = 946;
            // m_long_jump_end_move = 62;
            // m_short_jump_end_move = 32;
            // m_long_jump_vgap_mult = 2;
            // m_long_jump_hgap_mult = 0;
            // m_jump_endgame = 3;
            // m_nmr_min_depth = 3;
            // m_nmr_beta_mult = 34;
            // m_iid_depth = 5;
            // m_iid_depth_reduction = 3;
            // m_tt_eval_prop = 99;

            // m_lmp_margins = {0, 9, 16, 21, 21, 31};
            // m_lmr_depth = 4;
            // m_lmr_move = 14;
            // m_tempo = 82;
            // m_static_null_move_margin = 100;
            // m_countermove = 10;
            // m_lily_min = 1;
            // m_lily_range = {0, 156, 750, 1447, 1941, 2640, 2951, 3314};
            // m_fut_margins = {0, 444, 937, 1336, 1558, 1646, 2102};
            // m_razor_mult = 3;
            // m_razor_limit = 4;
            // m_nmr_const = 1;
            // m_nmr_depth = 6;
            // m_tempo_limit = 1289;
            // m_long_jump_end_move = 78;
            // m_short_jump_end_move = 35;
            // m_long_jump_vgap_mult = 2;
            // m_long_jump_hgap_mult = 0;
            // m_jump_endgame = 4;
            // m_nmr_min_depth = 2;
            // m_nmr_beta_mult = 39;
            // m_iid_depth = 5;
            // m_iid_depth_reduction = 4;
            // m_tt_eval_prop = 92;

            // evaluation features
            m_tempo = computer_config_static::m_tempo;
            m_lily_min = computer_config_static::m_lily_min;
            m_lily_range = computer_config_static::m_lily_range;
            m_tempo_limit = computer_config_static::m_tempo_limit;
            m_long_jump_end_move = computer_config_static::m_long_jump_end_move;
            m_short_jump_end_move = computer_config_static::m_short_jump_end_move;
            m_long_jump_vgap_mult = computer_config_static::m_long_jump_vgap_mult;
            m_long_jump_hgap_mult = computer_config_static::m_long_jump_hgap_mult;
            m_jump_endgame = computer_config_static::m_jump_endgame;
            m_tt_eval_prop = computer_config_static::m_tt_eval_prop;
            m_countermove = computer_config_static::m_countermove;

            // pruning features
            m_lmp_margins = computer_config_static::m_lmp_margins;
            m_lmr_depth = computer_config_static::m_lmr_depth;
            m_lmr_move = computer_config_static::m_lmr_move;
            m_static_null_move_margin = computer_config_static::m_static_null_move_margin;
            m_fut_margins = computer_config_static::m_fut_margins;
            m_razor_mult = computer_config_static::m_razor_mult;
            m_razor_limit = computer_config_static::m_razor_limit;
            m_nmr_const = computer_config_static::m_nmr_const;
            m_nmr_depth = computer_config_static::m_nmr_depth;
            m_nmr_min_depth = computer_config_static::m_nmr_min_depth;
            m_nmr_beta_mult = computer_config_static::m_nmr_beta_mult;
            m_iid_depth = computer_config_static::m_iid_depth;
            m_iid_depth_reduction = computer_config_static::m_iid_depth_reduction;

            m_window = computer_config_static::m_window;
            m_window_scale = computer_config_static::m_window_scale;


            m_window = 500;
            m_window_scale = 3;
        }

        void display_one(std::string var, int value) {
            std::cout << "static constexpr int " << var << " = " << value << ";\n";
        }

        template<int T>
        void display_list(std::string var, std::array<int, T> &values) {
            std::cout << "static constexpr std::array<int, " << T << "> " << var << " = {";
            for (int i = 0; i < values.size(); ++i) {
                std::cout << values[i];

                if (i < values.size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "};\n";
        }

        void display() {
            std::cout << "evaluation features\n";
            display_one("m_tempo", m_tempo);
            display_one("m_tempo_limit", m_tempo_limit);
            display_one("m_lily_min", m_lily_min);
            display_list<8>("m_lily_range", m_lily_range);
            display_one("m_long_jump_end_move", m_long_jump_end_move);
            display_one("m_short_jump_end_move", m_short_jump_end_move);
            display_one("m_long_jump_vgap_mult", m_long_jump_vgap_mult);
            display_one("m_long_jump_hgap_mult", m_long_jump_hgap_mult);
            display_one("m_jump_endgame", m_jump_endgame);
            display_one("m_tt_eval_prop", m_tt_eval_prop);
            display_one("m_countermove", m_countermove);

            std::cout << "\npruning features\n";
            display_list<6>("m_lmp_margins", m_lmp_margins);
            display_one("m_lmr_depth", m_lmr_depth);
            display_one("m_lmr_move", m_lmr_move);
            display_one("m_static_null_move_margin", m_static_null_move_margin);
            display_list<7>("m_fut_margins", m_fut_margins);
            display_one("m_razor_mult", m_razor_mult);
            display_one("m_razor_limit", m_razor_limit);
            display_one("m_nmr_const", m_nmr_const);
            display_one("m_nmr_depth", m_nmr_depth);
            display_one("m_nmr_min_depth", m_nmr_min_depth);
            display_one("m_nmr_beta_mult", m_nmr_beta_mult);
            display_one("m_iid_depth", m_iid_depth);
            display_one("m_iid_depth_reduction", m_iid_depth_reduction);
        }
    };
#endif

    struct nnue_evaluator {
        nnue2 m_nnue{};

        void load_nnue(const std::string &path) {
            m_nnue.load_network(path);
        }

        void push_move(const board::pos &position, const board::move &move) {
            m_nnue.clone_ply();
            m_nnue.m_ply += 1;

            if (move.is_grow()) {
                mask m = move.m_grow;
                while (m > 0) {
                    int index = __builtin_ctzll(m);
                    mask piece = 1ull << index;
                    m ^= piece;

                    m_nnue.addlily_feature(64 * 2 + index, 64 * 2 + (index ^ 56));
                }
            } else if (position.m_turn == board::RED) {
                int start = __builtin_ctzll(move.m_start);
                int end = __builtin_ctzll(move.m_end);

                // add piece
                // remove lilypad
                m_nnue.subaddsub_feature(RED, start, end, 64 * 2 + start);
                m_nnue.subaddsub_feature(BLUE, 64 + (start ^ 56), 64 + (end ^ 56), 64 * 2 + (start ^ 56));
            } else {
                int start = __builtin_ctzll(move.m_start);
                int end = __builtin_ctzll(move.m_end);

                // add piece
                // remove lilypad
                m_nnue.subaddsub_feature(BLUE, start ^ 56, end ^ 56, 64 * 2 + (start ^ 56));
                m_nnue.subaddsub_feature(RED, 64 + start, 64 + end, 64 * 2 + start);
            }
        }

        void pop_move(const board::pos &position, const board::move &move) {
            m_nnue.m_ply -= 1;
            return;

            if (move.is_grow()) {
                mask m = move.m_grow;
                while (m > 0) {
                    int index = __builtin_ctzll(m);
                    mask piece = 1ull << index;
                    m ^= piece;

                    m_nnue.sublily_feature(64 * 2 + index, 64 * 2 + (index ^ 56));
                }
            } else if (position.m_turn == board::RED) {
                int start = __builtin_ctzll(move.m_start);
                int end = __builtin_ctzll(move.m_end);

                m_nnue.subaddadd_feature(RED, end, start, 64 * 2 + start);
                m_nnue.subaddadd_feature(BLUE, 64 + (end ^ 56), 64 + (start ^ 56), 64 * 2 + (start ^ 56));
            } else {
                int start = __builtin_ctzll(move.m_start);
                int end = __builtin_ctzll(move.m_end);

                m_nnue.subaddadd_feature(BLUE, end ^ 56, start ^ 56, 64 * 2 + (start ^ 56));
                m_nnue.subaddadd_feature(RED, 64 + end, 64 + start, 64 * 2 + start);
            }
        }

        int32_t evaluate(const board::pos &position) const {
            return m_nnue.evaluate(position.m_turn);
        }

        void initialize(const board::pos &position) {
            m_nnue.initialize(position);
        }
    };

    struct pv_line {
        // pv_table[ply][i] is the ith pv move at ply
        board::move pv_table[param::max_depth][param::max_depth];

        // pv_length[ply] is the number of moves at ply
        int pv_length[param::max_depth]{};

        explicit pv_line() = default;

        void ply_init(int32_t ply) {
            pv_length[ply] = ply;
        }

        void update(int32_t ply, const board::move &move) {
            pv_table[ply][ply] = move;
            for (int i = ply + 1; i < pv_length[ply + 1]; i++)
                pv_table[ply][i] = pv_table[ply + 1][i];

            pv_length[ply] = pv_length[ply + 1];
        }

        std::vector<board::move> get_moves() const {
            std::vector<board::move> result(pv_length[0]);
            for (int i = 0; i < pv_length[0]; ++i) {
                result[i] = pv_table[0][i];
            }
            return result;
        }
    };

    class computer {
    public:
        table &m_tt;
        pos m_pos;
        int m_searched;

        int m_history[2][64][64];
        int m_lmr[param::max_depth][100];
        move m_killers[param::max_depth][2];
        move m_counter[2][64][64];
        pv_line m_line{};

        // unused
        nnue_evaluator m_nnue_evaluator;
        struct eval_cache {
            int score;
            uint64_t hash;
        };
        constexpr static size_t EVAL_CACHE_SIZE = 32 * 1024 * 1024 / sizeof(eval_cache);
        std::vector<eval_cache> m_eval_cache{EVAL_CACHE_SIZE};

        computer_config m_config;

        timer m_timer;

        explicit computer(pos &pos, table &tt, timer timer, std::vector<std::string> weights)
            : computer(pos, tt, timer, std::move(weights), computer_config()) {
        }

        explicit computer(pos &pos, table &tt, timer timer, std::vector<std::string> weights, computer_config config)
            : m_tt(tt), m_pos(pos), m_searched(0), m_config(config), m_timer(timer) {

            m_nnue_evaluator.load_nnue(weights[0]);


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

            for (int depth = 0; depth < param::max_depth; ++depth) {
                for (int move = 0; move < 100; ++move) {
                    m_lmr[depth][move] = std::max(1,
                                                  depth / std::max(1, CONFIG_GET(m_config, m_lmr_depth)) +
                                                          move / std::max(1, CONFIG_GET(m_config, m_lmr_move)));
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

            return row / (2 * 6);
        }


        int nnue_evaluate() {
            return m_nnue_evaluator.evaluate(m_pos);
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
            uint64_t hash = m_pos.get_hash();
            if (m_eval_cache[hash % EVAL_CACHE_SIZE].hash == hash)
                return m_eval_cache[hash % EVAL_CACHE_SIZE].score;

            int eval = nnue_evaluate();
            if (std::abs(eval) <= CONFIG_GET(m_config, m_tempo_limit))
                eval += CONFIG_GET(m_config, m_tempo);

            m_eval_cache[hash % EVAL_CACHE_SIZE].score = eval;
            m_eval_cache[hash % EVAL_CACHE_SIZE].hash = hash;

            return eval;
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
                    int hgap = abs(start.second - end.second);

                    bool is_red = m_pos.m_turn == board::RED;
                    bool is_endgame = m_pos.num_finished_piece(m_pos.m_turn) >= CONFIG_GET(m_config, m_jump_endgame);
                    if (((!is_red && start.first == 0) || (is_red && start.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // don't consider the move if we started at end, but only if num finished is below 3 so no shuffling
                        score += 0;
                    } else if (((!is_red && end.first == 0) || (is_red && end.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // do consider the move if we will finish at end
                        score += param::base_score + CONFIG_GET(m_config, m_long_jump_end_move);
                    } else if (vgap == 0) {
                        score += 0;
                    } else {
                        score += param::base_score + vgap * CONFIG_GET(m_config, m_long_jump_vgap_mult) + hgap * CONFIG_GET(m_config, m_long_jump_hgap_mult);
                    }
                } else if (move == m_killers[ply][0]) {
                    score += param::base_score + param::killer_move_score;
                } else if (move == m_killers[ply][1]) {
                    score += param::base_score + param::killer_move_score2;
                } else if (!move.is_grow()) {
                    auto start = bitboard::get_coord(move.m_start);
                    auto end = bitboard::get_coord(move.m_end);
                    bool is_red = m_pos.m_turn == board::RED;

                    bool is_endgame = m_pos.num_finished_piece(m_pos.m_turn) >= CONFIG_GET(m_config, m_jump_endgame);
                    if (((!is_red && start.first == 0) || (is_red && start.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // don't consider the move if we started at end, but only if num finished is below 3 so no shuffling
                        score += 0;
                    } else if (((!is_red && end.first == 0) || (is_red && end.first == bitboard::ROWS - 1)) && !is_endgame) {
                        // do consider the move if we will finish at end
                        score += param::base_score + CONFIG_GET(m_config, m_short_jump_end_move);
                    } else {
                        if (prev_move.is_storable()) {
                            auto coord = prev_move.get_coords();
                            if (move == m_counter[m_pos.m_turn][coord.first][coord.second]) {
                                score += CONFIG_GET(m_config, m_countermove);
                            }
                        }

                        auto coord = move.get_coords();
                        int history = m_history[m_pos.m_turn][coord.first][coord.second];
                        score += history;
                    }
                } else {
                    int count = __builtin_popcountll(move.m_grow);
                    if (count <= CONFIG_GET(m_config, m_lily_min)) {
                        score -= 10;
                    } else {
                        score += CONFIG_GET(m_config, m_lily_range)[count / 8 + 1];
                    }
                }

                scores.push_back({score, i});
            }

            return scores;
        }

        void sort_scored_moves(std::vector<std::pair<int, int>> &scored_moves, int i) {
            int best_score = scored_moves[i].first;
            int best_index = i;
            for (int j = i + 1; j < scored_moves.size(); ++j) {
                if (scored_moves[j].first > best_score) {
                    best_score = scored_moves[j].first;
                    best_index = j;
                }
            }

            swap(scored_moves[i], scored_moves[best_index]);
        }

        void incr_counter(const move &prev_move, move &move) {
            if (prev_move.is_storable() && move.is_quiet()) {
                auto coord = prev_move.get_coords();
                m_counter[m_pos.m_turn][coord.first][coord.second] = move;
            }
        }


        void incr_history(move &move, int depth) {
            if (!move.is_slient())
                return;

            auto coord = move.get_coords();
            m_history[m_pos.m_turn][coord.first][coord.second] += depth * depth;
            if (m_history[m_pos.m_turn][coord.first][coord.second] >= param::base_score) {
                for (auto &i: m_history) {
                    for (auto &j: i) {
                        for (int &k: j) {
                            k /= 2;
                        }
                    }
                }
            }
        }

        void decr_history(move &move, int depth) {
            auto coord = move.get_coords();
            auto &score = m_history[m_pos.m_turn][coord.first][coord.second];
            score -= depth * depth;
            if (score < 0)
                score = 0;
        }

        void store_killer(int ply, const move &killer) {
            if (killer.is_quiet()) {
                if (m_killers[ply][0] != killer) {
                    m_killers[ply][1] = m_killers[ply][0];
                    m_killers[ply][0] = killer;
                }
            }
        }


        int qsearch(int max_ply, int ply, int alpha, int beta) {
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


            // cache evaluation
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
            for (int i = 0; i < moves.size(); ++i) {
                sort_scored_moves(scored_moves, i);
                move &move = moves[scored_moves[i].second];

                m_nnue_evaluator.push_move(m_pos, move);
                m_pos.push(move);

                int score = -qsearch(max_ply, ply + 1, -beta, -alpha);

                m_pos.pop(move);
                m_nnue_evaluator.pop_move(m_pos, move);

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
                }
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


        int negamax(int depth, int ply, int alpha, int beta, const move &prev_move, bool do_null = true) {
            m_searched += 1;
            m_line.ply_init(ply);

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


            bool is_root = ply == 0;
            bool is_pv_node = (beta - alpha) != 1;


            if (depth <= 0) {
                m_searched -= 1;
                return qsearch(ply, 0, alpha, beta);
            }

            auto &entry = m_tt.probe(m_pos.get_hash());
            auto [tt_score, should_use, tt_move] = entry.get(m_pos.get_hash(), ply, depth, alpha, beta);
            if (should_use && !is_root) {
                return tt_score;
            }

            bool tt_score_valid = !tt_move.is_null();


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
                int adjusted_evaluation = evaluate();
                if (tt_score_valid) {
                    double p = CONFIG_GET(m_config, m_tt_eval_prop) / 100.0;
                    adjusted_evaluation =
                            static_cast<int>(round(p * adjusted_evaluation + (1.0 - p) * tt_score));
                }

                int stat = adjusted_evaluation;
                int score_margin = depth * CONFIG_GET(m_config, m_static_null_move_margin);
                if ((stat - score_margin) >= beta) {
                    return stat - score_margin;
                }
            }

            // null move pruning
            int remain = m_pos.num_unfinished_piece();
            int growth_count = m_pos.growth_count();
            if (do_null && !is_pv_node && depth >= CONFIG_GET(m_config, m_nmr_min_depth) && remain > 2 && growth_count <= 8) {
                move null = move::null();

                int adjusted_evaluation = evaluate();
                if (tt_score_valid) {
                    double p = CONFIG_GET(m_config, m_tt_eval_prop) / 100.0;
                    adjusted_evaluation =
                            static_cast<int>(round(p * adjusted_evaluation + (1.0 - p) * tt_score));
                }

                int static_score = adjusted_evaluation;
                int r = CONFIG_GET(m_config, m_nmr_const) + depth / CONFIG_GET(m_config, m_nmr_depth) +
                        std::max(0, static_cast<int>(round((static_score - beta) / 10000.0 * CONFIG_GET(m_config, m_nmr_beta_mult))));

                m_pos.push(null);
                int score = -negamax(depth - 1 - r, ply + 1, -beta, -beta + 1, null, false);
                m_pos.pop(null);

                if (m_timer.is_stopped()) {
                    return 0;
                }

                if (score >= beta) {
                    return score;
                }
            }


            // razoring
            // if static eval is really bad, check via qsearch to see if it fails-low
            if (depth <= CONFIG_GET(m_config, m_razor_limit) && !is_pv_node && !m_pos.has_jumps()) {
                int adjusted_evaluation = evaluate();
                if (tt_score_valid) {
                    double p = CONFIG_GET(m_config, m_tt_eval_prop) / 100.0;
                    adjusted_evaluation =
                            static_cast<int>(round(p * adjusted_evaluation + (1.0 - p) * tt_score));
                }

                int static_score = adjusted_evaluation;
                if (static_score + CONFIG_GET(m_config, m_fut_margins)[depth] * CONFIG_GET(m_config, m_razor_mult) < alpha) {
                    int score = qsearch(ply, 0, alpha, beta);
                    if (score < alpha) {
                        return alpha;
                    }
                }
            }

            // futility pruning, when static is much worse than alpha, likely not a cut-node, so prune every quiet-move except the pv node
            bool canFutilityPrune = false;
            if (depth < CONFIG_GET(m_config, m_fut_margins).size() && !is_pv_node && std::abs(alpha) < param::checkmate && std::abs(beta) < param::checkmate && !m_pos.has_jumps()) {
                int adjusted_evaluation = evaluate();
                if (tt_score_valid) {
                    double p = CONFIG_GET(m_config, m_tt_eval_prop) / 100.0;
                    adjusted_evaluation =
                            static_cast<int>(round(p * adjusted_evaluation + (1.0 - p) * tt_score));
                }

                int static_score = adjusted_evaluation;
                int margin = CONFIG_GET(m_config, m_fut_margins)[depth];
                canFutilityPrune = static_score + margin <= alpha;
            }


            // internal ID
            if (depth >= CONFIG_GET(m_config, m_iid_depth) && (is_pv_node || entry.m_flag == param::beta_flag) && tt_move.is_null()) {
                negamax(depth - CONFIG_GET(m_config, m_iid_depth_reduction), ply + 1, -beta, -alpha, move::null());

                if (m_line.pv_length[ply + 1] > ply + 1) {
                    tt_move = m_line.pv_table[ply + 1][ply + 1];
                }
            }

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
            move quiet_moves[64];
            int quiet_moves_count = 0;

            for (int i = 0; i < moves.size(); ++i) {
                sort_scored_moves(scored_moves, i);
                move &move = moves[scored_moves[i].second];

                int explored_moves = i + 1;

                m_nnue_evaluator.push_move(m_pos, move);
                m_pos.push(move);

                // lmp
                if (depth < CONFIG_GET(m_config, m_lmp_margins).size() && !is_pv_node && explored_moves > CONFIG_GET(m_config, m_lmp_margins)[depth] && !move.is_jump()) {
                    bool tactical = m_pos.has_jumps();
                    if (!tactical) {
                        m_pos.pop(move);
                        m_nnue_evaluator.pop_move(m_pos, move);
                        continue;
                    }
                }

                if (canFutilityPrune && explored_moves > 1 && !move.is_jump()) {
                    bool tactical = m_pos.has_jumps();
                    if (!tactical) {
                        m_pos.pop(move);
                        m_nnue_evaluator.pop_move(m_pos, move);
                        continue;
                    }
                }


                int score;
                if (explored_moves == 1) {
                    score = -negamax(depth - 1, ply + 1, -beta, -alpha, move);
                } else {
                    int reduction = 0;
                    if (!is_pv_node && explored_moves >= 3 && depth >= 3 && !move.is_jump()) {
                        reduction = m_lmr[depth][explored_moves];
                    }

                    score = -negamax(depth - 1 - reduction, ply + 1, -(alpha + 1), -alpha, move);
                    if (score > alpha && reduction > 0) {
                        score = -negamax(depth - 1, ply + 1, -(alpha + 1), -alpha, move);
                        if (score > alpha) {
                            score = -negamax(depth - 1, ply + 1, -beta, -alpha, move);
                        }
                    } else if (alpha < score && score < beta) {
                        score = -negamax(depth - 1, ply + 1, -beta, -alpha, move);
                    }
                }
                m_pos.pop(move);
                m_nnue_evaluator.pop_move(m_pos, move);

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
                    if (move.is_slient()) {
                        for (int j = 0; j < quiet_moves_count; ++j)
                            decr_history(quiet_moves[j], depth);
                    }

                    incr_counter(prev_move, move);
                    store_killer(ply, move);
                    break;
                }

                if (move.is_slient() && quiet_moves_count < 64) {
                    quiet_moves[quiet_moves_count++] = move;
                }

                if (score > alpha) {
                    alpha = score;
                    tt_flag = param::exact_flag;
                    m_line.update(ply, move);
                }

                // compute full list if tt failed to beta cutoff
                if (explored_moves == 1 && !tt_move.is_null()) {
                    moves = m_pos.get_moves();
                    scored_moves = score_moves(moves, tt_move, ply, prev_move);
                    sort_scored_moves(scored_moves, 0);
                }
            }

            if (!m_timer.is_stopped() && depth >= entry.m_depth) {
                entry.set(m_pos, m_pos.get_hash(), best_score, best_move, ply, depth, tt_flag);
            }

            return best_score;
        }


        void init() {
            m_searched = 0;
            m_nnue_evaluator.initialize(m_pos);
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

    template<
            class result_t = std::chrono::microseconds,
            class clock_t = std::chrono::steady_clock,
            class duration_t = std::chrono::microseconds>
    auto since(std::chrono::time_point<clock_t, duration_t> const &start) {
        return std::chrono::duration_cast<result_t>(clock_t::now() - start);
    }


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

        explicit lazysmp(int threads, int table = 1024)
            // need one less thread since we are the main thread
            : m_threads(threads), m_pool(threads), m_tt(table) {
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

        std::string get_score_uci(int score) {
            if (score > param::checkmate) {
                int ply = param::inf - score;
                return std::string{"ply "} + std::to_string(ply);
            }

            if (score < -param::checkmate) {
                int ply = -param::inf - score;
                return std::string{"ply "} + std::to_string(ply);
            }

            return std::string{"cp "} + std::to_string(score);
        }

        int thread_value(int score, int worse_score, int depth) const {
            return (score - worse_score) + 500 * depth;
        }

        move search_one(
                pos pos, int ts,
                int *new_score,
                const std::vector<std::string> &weights,
                const computer_config &config,
                bool verbose,
                int *reached_depth = nullptr,
                int max_depth = param::max_depth) {
            m_timer.start(ts);
            auto start = m_timer.now();

            computer computer{pos, m_tt, m_timer, weights, config};
            computer.init();

            int alpha = -param::inf;
            int beta = param::inf;
            int depth = 1;
            move null = move::null();
            move best_move = move::null();
            int last_score = 0;
            while (depth <= max_depth) {
                int score = computer.negamax(depth, 0, alpha, beta, null, true);

                if (computer.m_timer.is_stopped()) {
                    if (!computer.m_line.get_moves().empty())
                        best_move = computer.m_line.get_moves()[0];

                    break;
                }


                if (score <= alpha || score >= beta) {
                    if (score <= alpha) {
                        alpha = last_score + (alpha - last_score) * CONFIG_GET(config, m_window_scale);
                        if (alpha <= -40 * 100) {
                            alpha = -param::inf;
                        }
                    }

                    if (score >= beta) {
                        beta = last_score + (beta - last_score) * CONFIG_GET(config, m_window_scale);
                        if (beta >= 40 * 100) {
                            beta = param::inf;
                        }
                    }

                    continue;
                }

                if (reached_depth != nullptr)
                    *reached_depth = depth;

                if (verbose) {
                    auto duration = m_timer.now() - start;
                    long nps = static_cast<long>(computer.m_searched) * 1000 / duration.count();

                    std::cout << "info"
                              << " depth " << depth
                              << " score " << get_score_uci(score)
                              << " nodes " << computer.m_searched
                              << " nps " << nps
                              << " time " << duration.count()
                              << " hashfull " << (int) (1000 * m_tt.occupied())
                              << " pv";

                    for (auto move: computer.m_line.get_moves()) {
                        std::cout << " " << move.display().c_str();
                    }
                    std::cout << std::endl;
                }

                if (new_score != nullptr)
                    *new_score = score;

                alpha = score - CONFIG_GET(config, m_window);
                beta = score + CONFIG_GET(config, m_window);
                best_move = computer.m_line.get_moves()[0];
                last_score = score;
                depth += 1;
            }

            if (verbose)
                std::cout << "info nodes " << computer.m_searched << std::endl;

            return best_move;
        }

        move search(pos pos, int ts, int *new_score, const std::vector<std::string> &weights, const computer_config &config, bool verbose, int max_depth = param::max_depth) {
            if (m_threads == 1) {
                return search_one(pos, ts, new_score, weights, config, verbose, nullptr, max_depth);
            }

            m_timer.start(ts);

            // setup
            std::vector<computer> computers;

            for (int i = 0; i < m_threads; ++i) {
                computer comp{pos, m_tt, m_timer, weights, config};
                comp.init();
                computers.emplace_back(std::move(comp));
            }
            int root_thread = 0;

            if (verbose)
                printf("searching with %d threads\n", m_threads);

            // search depth one fully using the first comp
            move null = move::null();
            int alpha = -param::inf;
            int beta = param::inf;
            int depth = 1;
            int score = computers[root_thread].negamax(depth, 0, alpha, beta, null);
            depth += 1;
            move best_move = computers[root_thread].m_line.get_moves()[0];
            if (new_score != nullptr)
                *new_score = score;

            int last_score = score;
            alpha = last_score - CONFIG_GET(config, m_window);
            beta = last_score + CONFIG_GET(config, m_window);

            std::vector<int> scores(m_threads);
            std::vector<std::vector<move>> best_moves(m_threads);
            std::vector<int> depths(m_threads);


            std::atomic<int> finished_tasks = 0;
            std::atomic<bool> is_finished = false;


            while (depth <= max_depth) {
                std::vector<bool> is_ok(m_threads, false);

                for (int i = 0; i < m_threads; ++i) {
                    computers[i].m_timer = m_timer;
                }

                // start helper search
                is_finished = false;
                finished_tasks = 0;
                for (int i = 0; i < m_threads; ++i) {
                    if (i == root_thread) {
                        continue;
                    }

                    lazysmp_thread_context context{
                            i,
                            computers[i],
                            alpha,
                            beta,
                            depth + (i % 2),
                            best_moves[i],
                            scores[i],
                            depths[i]};
                    int threads = m_threads;
                    m_pool.enqueue([context, &finished_tasks, &is_finished, &is_ok, threads, last_score, &config]() {
                        int last_score_ = last_score;
                        int alpha = context.alpha;
                        int beta = context.beta;
                        int depth = context.depth;
                        bool is_retry = false;

                        while (true) {
                            move null = move::null();
                            int score = context.computer.negamax(depth, 0, alpha, beta, null);

                            if (context.computer.m_timer.is_stopped()) {
                                if (!is_retry) {
                                    // dont update
                                    context.best_move = {};
                                    context.depths = 0;
                                    context.score = 0;
                                }

                                break;
                            }

                            // useless eval if outside the asp window, so why not research
                            if (score <= alpha || score >= beta) {
                                if (score <= alpha) {
                                    alpha = last_score_ + (alpha - last_score_) * CONFIG_GET(config, m_window_scale);
                                    if (alpha < -50 * 100) {
                                        alpha = -param::inf;
                                    }
                                }

                                if (score >= beta) {
                                    beta = last_score_ + (beta - last_score_) * CONFIG_GET(config, m_window_scale);
                                    if (beta > 50 * 100) {
                                        beta = param::inf;
                                    }
                                }

                                continue;
                            }

                            context.best_move = context.computer.m_line.get_moves();
                            context.depths = depth;
                            context.score = score;
                            is_ok[context.i] = true;

                            // early, so retry higher depth
                            depth += 1;
                            is_retry = true;
                            last_score_ = score;
                            alpha = last_score_ - CONFIG_GET(config, m_window);
                            beta = last_score_ + CONFIG_GET(config, m_window);
                        }


                        finished_tasks += 1;
                        if (finished_tasks == threads - 1) {
                            is_finished = true;
                            is_finished.notify_one();
                        }
                    });
                }

                // run root search
                while (true) {
                    lazysmp_thread_context context{
                            root_thread,
                            computers[root_thread],
                            alpha,
                            beta,
                            depth + (root_thread % 2),
                            best_moves[root_thread],
                            scores[root_thread],
                            depths[root_thread]};
                    int score = context.computer.negamax(context.depth, 0, context.alpha, context.beta, null);

                    if (context.computer.m_timer.is_stopped()) {
                        // dont update
                        context.best_move = {};
                        context.depths = 0;
                        context.score = 0;
                        break;
                    }

                    if (score <= alpha || score >= beta) {
                        if (score <= alpha) {
                            alpha = last_score + (alpha - last_score) * CONFIG_GET(config, m_window_scale);
                            if (alpha < -50 * 100) {
                                alpha = -param::inf;
                            }
                        }

                        if (score >= beta) {
                            beta = last_score + (beta - last_score) * CONFIG_GET(config, m_window_scale);
                            if (beta > 50 * 100) {
                                beta = param::inf;
                            }
                        }

                        continue;
                    }

                    context.best_move = context.computer.m_line.get_moves();
                    context.depths = context.depth;
                    context.score = score;
                    is_ok[root_thread] = true;
                    break;
                }

                // when root search stopped,
                // stop the other engines
                for (int i = 0; i < m_threads; ++i) {
                    computers[i].m_timer.stop();
                }

                // wait until threads finished
                if (m_threads > 1)
                    is_finished.wait(false);

                // check if timer exceeds
                m_timer.check();
                if (m_timer.is_stopped()) {
                    break;
                }

                int worse_score = param::inf;
                std::map<move, int> vote;

                for (int i = 0; i < m_threads; ++i) {
                    if (is_ok[i]) {
                        worse_score = std::min(worse_score, scores[i]);
                        vote[best_moves[i][0]] = 0;
                    }
                }

                for (int i = 0; i < m_threads; ++i) {
                    // compute thread value, favor depth than score
                    if (is_ok[i]) {
                        int threadvalue = thread_value(scores[i], worse_score, depths[i]);
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

                    if (!is_ok[i]) {
                        continue;
                    }

                    int current_score = scores[i];
                    int current_vote_score = vote[best_moves[i][0]];

                    // choose the fastest mate
                    if (std::abs(current_score) >= param::checkmate) {
                        if (current_score > best_score) {
                            best_thread = i;
                            best_score = current_score;
                            best_vote_score = current_vote_score;
                        }
                    } else if (current_vote_score > -param::checkmate) {
                        if (current_vote_score > best_vote_score || (current_vote_score == best_vote_score && (thread_value(current_score, worse_score, depths[i]) > thread_value(scores[best_thread], worse_score, depths[best_thread])))) {
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
                last_score = best_score;

                if (verbose) {
                    int nodes = 0;
                    for (int i = 0; i < m_threads; ++i) {
                        nodes += computers[i].m_searched;
                    }
                    printf("nodes %10d, depth %2d, value %7s (%5d), occ %.2lf%%", nodes, depth, get_score(best_score).c_str(), best_score, m_tt.occupied() * 100.0);

                    printf(", pv = [");
                    for (auto move: best_moves[root_thread]) {
                        printf("%s, ", move.display().c_str());
                    }
                    printf("]\n");
                }

                alpha = last_score - CONFIG_GET(config, m_window);
                beta = last_score + CONFIG_GET(config, m_window);

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

        explicit analysis(computer &&computer)
            : m_computer(std::move(computer)) {

            // auto [red, blue] = m_computer.init_nnue();
            throw "help";
            // m_computer.m_nnue.init(red, blue);

            m_computer.m_timer.start(10000);
        }

        int ponder() {
            if (m_computer.m_timer.is_stopped()) {
                return m_score;
            }

            int alpha = -param::inf;
            int beta = param::inf;
            move null = move::null();
            int score = m_computer.negamax(m_depth, 0, alpha, beta, null);
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
