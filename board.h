//
// Created by terry on 11/03/25.
//

#ifndef FRECKER_BOARD_H
#define FRECKER_BOARD_H

#include <cinttypes>
#include <vector>
#include <string>
#include <cstdio>

namespace board {
    using mask = uint64_t;

    const int RED = 0;
    const int BLUE = 1;
    const int NONE = 2;
    const int DRAW = 3;

    const int DRAW_MOVES = 1500;

    class bitboard {
    public:
        // statics
        const static int ROWS = 8;
        const static int COLS = 8;
        const static mask TOP = 0b11111111;
        const static mask BOTTOM = (mask) 0b11111111 << ((ROWS - 1) * COLS);
        const static mask ALL = UINT64_MAX;
        const static mask LINE = 0b100000001000000010000000100000001000000010000000100000001;

        static mask from_array(const std::vector<std::vector<int>> &bits) {
            mask mask = 0ull;
            int i = 0;
            for (auto &row: bits) {
                for (auto col: row) {
                    if (col == 1) {
                        mask |= 1ull << i;
                    }
                    i++;
                }
            }

            return mask;
        }


        static mask dilate_down(mask pos) {
            mask no_left = ALL ^ LINE;
            mask no_right = ALL ^ (LINE << (COLS - 1));
            // left right, bottom left, bottom, bottom right
            mask result = ((pos >> 1) & no_right) | ((pos << 1) & no_left) |
                          ((pos << (ROWS - 1)) & no_right) | (pos << (ROWS)) | ((pos << (ROWS + 1)) & no_left);
            return result;
        }

        static mask jump_down(mask pos, mask obst) {
            mask no_left = ALL ^ LINE;
            mask no_right = ALL ^ (LINE << (COLS - 1));

            // left right, bottom left, bottom, bottom right
            mask left = ((((pos >> 1) & no_right) & obst) >> 1) & no_right;
            mask right = ((((pos << 1) & no_left) & obst) << 1) & no_left;
            mask bottom_left = ((((pos << (ROWS - 1)) & no_right) & obst) << (ROWS - 1)) & no_right;
            mask bottom_right = ((((pos << (ROWS + 1)) & no_left) & obst) << (ROWS + 1)) & no_left;
            mask bottom = ((((pos << ROWS)) & obst) << ROWS);
            return left | right | bottom_left | bottom_right | bottom;
        }

        static mask dilate_up(mask pos) {
            mask no_left = ALL ^ LINE;
            mask no_right = ALL ^ (LINE << (COLS - 1));
            // left right, top right, top, top left
            mask result = ((pos >> 1) & no_right) | ((pos << 1) & no_left) |
                          ((pos >> (ROWS - 1)) & no_left) | (pos >> (ROWS)) | ((pos >> (ROWS + 1)) & no_right);
            return result;
        }

        static mask jump_up(mask pos, mask obst) {
            mask no_left = ALL ^ LINE;
            mask no_right = ALL ^ (LINE << (COLS - 1));

            // left right, top right, top, top left
            mask left = ((((pos >> 1) & no_right) & obst) >> 1) & no_right;
            mask right = ((((pos << 1) & no_left) & obst) << 1) & no_left;
            mask top_right = ((((pos >> (ROWS - 1)) & no_left) & obst) >> (ROWS - 1)) & no_left;
            mask top_left = ((((pos >> (ROWS + 1)) & no_right) & obst) >> (ROWS + 1)) & no_right;
            mask top = ((((pos >> ROWS)) & obst) >> ROWS);
            return left | right | top_left | top_right | top;
        }

        static std::pair<int, int> get_coord(mask pos) {
            int i = __builtin_ctzll(pos);
            return {i / COLS, i % COLS};
        }

        static std::string display(mask m) {
            std::string out = "";
            for (int row = 0; row < bitboard::ROWS; ++row) {
                out += std::to_string(row) + " ";

                for (int col = 0; col < bitboard::COLS; ++col) {
                    int i = row * bitboard::COLS + col;
                    mask mask = 1ull << i;
                    if (m & mask) {
                        out += "X";
                    } else {
                        out += ".";
                    }
                    out += " ";
                }
                out += "\n";
            }

            out += "  ";
            for (int i = 0; i < bitboard::COLS; ++i) {
                out += std::to_string(i) + " ";
            }

            return out;
        }

    };


    class move {
    public:
        // either a grow or a move/jump
        // if m_grow == ALL, then it is a jump

        mask m_grow;
        mask m_start;
        mask m_end;

        bool is_grow() const {
            return m_grow != bitboard::ALL;
        }

        bool is_null() const {
            return m_grow == bitboard::ALL - 1;
        }

        std::pair<int, int> get_coords() {
            auto start = bitboard::get_coord(m_start);
            auto end = bitboard::get_coord(m_end);
            return {start.first * bitboard::COLS + start.second, end.first * bitboard::COLS + end.second};
        }

        static move null() {
            return {bitboard::ALL - 1, 0, 0};
        }

        static void from_mask(mask pos, mask start, std::vector<move> &out) {
            while (pos > 0) {
                // get piece mask
                mask piece = 1ull << (bitboard::ROWS * bitboard::COLS - __builtin_clzll(pos) - 1);
                pos ^= piece;

                out.push_back(move{bitboard::ALL, start, piece});
            }
        }

        bool operator==(const move &other) {
            return m_grow == other.m_grow && m_start == other.m_start && m_end == other.m_end;
        }

        std::string display() const {
            if (is_grow()) {
                return "grow";
            }

            auto start = bitboard::get_coord(m_start);
            auto end = bitboard::get_coord(m_end);

            std::string out;
            out += "(" + std::to_string(start.first) + ", " + std::to_string(start.second) + ") to (" +
                   std::to_string(end.first) + ", " + std::to_string(end.second) + ")";
            return out;
        }
    };

    class pos {
    public:
        mask m_lilypads;
        mask m_players[2]{};
        int m_turn;
        int m_moves = 0;

        pos() {
            m_lilypads = bitboard::from_array({
                                                      {1, 1, 1, 1, 1, 1, 1, 1},
                                                      {0, 1, 1, 1, 1, 1, 1, 0},
                                                      {0, 0, 0, 0, 0, 0, 0, 0},
                                                      {0, 0, 0, 0, 0, 0, 0, 0},
                                                      {0, 0, 0, 0, 0, 0, 0, 0},
                                                      {0, 0, 0, 0, 0, 0, 0, 0},
                                                      {0, 1, 1, 1, 1, 1, 1, 0},
                                                      {1, 1, 1, 1, 1, 1, 1, 1},
                                              });

            m_players[0] = bitboard::from_array({
                                                        {0, 1, 1, 1, 1, 1, 1, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                });

            m_players[1] = bitboard::from_array({
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 0, 0, 0, 0, 0, 0, 0},
                                                        {0, 1, 1, 1, 1, 1, 1, 0},
                                                });

            m_turn = RED;
            m_moves = 0;
        }


        std::vector<move> get_moves() const {
            std::vector<move> moves;

            // first handle grow
            mask player = m_players[m_turn];
            mask grown = bitboard::dilate_down(player) | bitboard::dilate_up(player);
            grown &= ~m_lilypads;
            moves.push_back(move{grown, {}});

            // then handle moves for each piece
            mask pieces = player;
            while (pieces > 0) {
                // get piece mask
                mask piece = 1ull << (bitboard::ROWS * bitboard::COLS - __builtin_clzll(pieces) - 1);
                pieces ^= piece;


                // direct moves
                mask direct = 0;
                if (m_turn == RED) {
                    direct = bitboard::dilate_down(piece);
                } else {
                    direct = bitboard::dilate_up(piece);
                }
                direct &= (~(m_players[0] | m_players[1])) & m_lilypads;

                move::from_mask(direct, piece, moves);


                // jumps
                mask obst = m_players[0] | m_players[1];
                mask all_jumps = piece;
                mask next_jumps = piece;
                while (next_jumps > 0) {
                    // broadcast

                    mask new_jumps = 0;
                    if (m_turn == RED) {
                        new_jumps = bitboard::jump_down(next_jumps, obst);
                    } else {
                        new_jumps = bitboard::jump_up(next_jumps, obst);
                    }

                    new_jumps &= m_lilypads & (~obst);
                    next_jumps = new_jumps & (~all_jumps);
                    all_jumps |= new_jumps;
                }


                all_jumps ^= piece;
                move::from_mask(all_jumps, piece, moves);
            }


            return moves;
        }

        void push(move &play) {
            if (play.is_null()) {}
            else if (play.is_grow()) {
                m_lilypads |= play.m_grow;
            } else {
                m_lilypads ^= play.m_start;
                m_players[m_turn] ^= play.m_start;
                m_players[m_turn] |= play.m_end;
            }

            m_turn = 1 - m_turn;
            m_moves += 1;
        }

        void pop(move &play) {
            m_turn = 1 - m_turn;
            m_moves -= 1;

            if (play.is_null()) {}
            else if (play.is_grow()) {
                m_lilypads ^= play.m_grow;
            } else {
                m_lilypads |= play.m_start;
                m_players[m_turn] ^= play.m_end;
                m_players[m_turn] |= play.m_start;
            }
        }

        int get_state() const {
            if ((m_players[RED] & bitboard::BOTTOM) == m_players[RED]) {
                return RED;
            }
            if ((m_players[BLUE] & bitboard::TOP) == m_players[BLUE]) {
                return BLUE;
            }

            if (m_moves == DRAW_MOVES) {
                return DRAW;
            }

            return NONE;
        }

        std::string display() const {
            std::string out = "";
            for (int row = 0; row < bitboard::ROWS; ++row) {
                out += std::to_string(row) + " ";

                for (int col = 0; col < bitboard::COLS; ++col) {
                    int i = row * bitboard::COLS + col;
                    mask mask = 1ull << i;
                    if (m_players[RED] & mask) {
                        out += "R";
                    } else if (m_players[BLUE] & mask) {
                        out += "B";
                    } else if (m_lilypads & mask) {
                        out += "_";
                    } else {
                        out += ".";
                    }
                    out += " ";
                }
                out += "\n";
            }

            out += "  ";
            for (int i = 0; i < bitboard::COLS; ++i) {
                out += std::to_string(i) + " ";
            }

            return out;
        }

        uint64_t hash() const {
            return m_players[0] ^ (m_players[1] << 8 | (m_players[1] & 0xff)) ^
                   ((m_lilypads << 16) | (m_lilypads & 0xffff));
        }
    };


}


#endif //FRECKER_BOARD_H
