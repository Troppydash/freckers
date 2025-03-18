//
// Created by terry on 11/03/25.
//

#ifndef FRECKER_BOARD_H
#define FRECKER_BOARD_H

#include <cinttypes>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
#include "colors.h"

namespace board {
    using mask = uint64_t;

    const int RED = 0;
    const int BLUE = 1;
    const int NONE = 2;
    const int DRAW = 3;

    const int DRAW_MOVES = 150;

    class bitboard {
    public:
        // statics
        const static int ROWS = 8;
        const static int COLS = 8;

        const static mask TOP = 0b11111111;
        const static mask BOTTOM = ((mask) 0b11111111) << ((ROWS - 1) * COLS);
        const static mask ALL = UINT64_MAX;
        const static mask LINE = 0b100000001000000010000000100000001000000010000000100000001;
        const static mask HLINE = 0b11111111;
        const static mask TOP_HALF = (HLINE) | (HLINE << (1 * COLS)) | (HLINE << (2 * COLS)) | (HLINE << (3 * COLS));
        const static mask BOTTOM_HALF =
                (HLINE << (4 * COLS)) | (HLINE << (5 * COLS)) | (HLINE << (6 * COLS)) | (HLINE << (7 * COLS));

        // A function to convert a vector of vectors of bits
        // to a bit mask
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

        //??
        static mask dilate(mask pos) {
            // Get the mask board where the left side of the board is zero
            mask no_left = ALL ^ LINE;
            // Get the mask board where the right side of the board is zero
            mask no_right = ALL ^ (LINE << (COLS - 1));

            // left right, bottom left, bottom, bottom right, top right, top, top left
            mask result = ((pos >> 1) & no_right) | ((pos << 1) & no_left) |
                          ((pos << (COLS - 1)) & no_right) | (pos << (COLS)) | ((pos << (COLS + 1)) & no_left) | ((pos >> (COLS - 1)) & no_left) | (pos >> (COLS)) | ((pos >> (COLS + 1)) & no_right);

            return result;
        }

        //??
        static mask dilate_down(mask pos) {
            // Get the mask board where the left side of the board is zero
            mask no_left = ALL ^ LINE;
            // Get the mask board where the right side of the board is zero
            mask no_right = ALL ^ (LINE << (COLS - 1));
            // left right, bottom left, bottom, bottom right
            mask result = ((pos >> 1) & no_right) | ((pos << 1) & no_left) |
                          ((pos << (COLS - 1)) & no_right) | (pos << (COLS)) | ((pos << (COLS + 1)) & no_left);
            return result;
        }

        static mask jump_down(mask pos, mask obst) {
            mask no_left = ALL ^ LINE;
            mask no_right = ALL ^ (LINE << (COLS - 1));

            // left right, bottom left, bottom, bottom right
            mask left = ((((pos >> 1) & no_right) & obst) >> 1) & no_right;
            mask right = ((((pos << 1) & no_left) & obst) << 1) & no_left;
            mask bottom_left = ((((pos << (COLS - 1)) & no_right) & obst) << (COLS - 1)) & no_right;
            mask bottom_right = ((((pos << (COLS + 1)) & no_left) & obst) << (COLS + 1)) & no_left;
            mask bottom = ((((pos << COLS)) & obst) << COLS);
            return left | right | bottom_left | bottom_right | bottom;
        }

        static mask jump_down_forward(mask pos, mask obst) {
            mask no_left = ALL ^ LINE;
            mask no_right = ALL ^ (LINE << (COLS - 1));

            // left right, bottom left, bottom, bottom right
            mask bottom_left = ((((pos << (COLS - 1)) & no_right) & obst) << (COLS - 1)) & no_right;
            mask bottom_right = ((((pos << (COLS + 1)) & no_left) & obst) << (COLS + 1)) & no_left;
            mask bottom = ((((pos << COLS)) & obst) << COLS);
            return bottom_left | bottom_right | bottom;
        }

        static mask dilate_up(mask pos) {
            mask no_left = ALL ^ LINE;
            mask no_right = ALL ^ (LINE << (COLS - 1));
            // left right, top right, top, top left
            mask result = ((pos >> 1) & no_right) | ((pos << 1) & no_left) |
                          ((pos >> (COLS - 1)) & no_left) | (pos >> (COLS)) | ((pos >> (COLS + 1)) & no_right);
            return result;
        }

        static mask jump_up(mask pos, mask obst) {
            mask no_left = ALL ^ LINE;
            mask no_right = ALL ^ (LINE << (COLS - 1));

            // left right, top right, top, top left
            mask left = ((((pos >> 1) & no_right) & obst) >> 1) & no_right;
            mask right = ((((pos << 1) & no_left) & obst) << 1) & no_left;
            mask top_right = ((((pos >> (COLS - 1)) & no_left) & obst) >> (COLS - 1)) & no_left;
            mask top_left = ((((pos >> (COLS + 1)) & no_right) & obst) >> (COLS + 1)) & no_right;
            mask top = ((((pos >> COLS)) & obst) >> COLS);
            return left | right | top_left | top_right | top;
        }

        static mask jump_up_forward(mask pos, mask obst) {
            mask no_left = ALL ^ LINE;
            mask no_right = ALL ^ (LINE << (COLS - 1));

            // left right, top right, top, top left
            mask top_right = ((((pos >> (COLS - 1)) & no_left) & obst) >> (COLS - 1)) & no_left;
            mask top_left = ((((pos >> (COLS + 1)) & no_right) & obst) >> (COLS + 1)) & no_right;
            mask top = ((((pos >> COLS)) & obst) >> COLS);
            return top_left | top_right | top;
        }

        static int get_index(mask pos) {
            return __builtin_ctzll(pos);
        }

        static std::pair<int, int> get_coord(mask pos) {
            int i = __builtin_ctzll(pos);
            return {i / COLS, i % COLS};
        }

        static std::string display(mask m) {
            std::string out;
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

    // A class to represent a move for a frog
    // The move could either be
    // a jump or a grow
    class move {
    public:
        // either the player choose a grow or a move/jump
        // if m_grow == ALL, then it is a jump

        mask m_grow;
        mask m_start;
        mask m_end;

        // Return an empty move
        static move null() {
            return {bitboard::ALL - 1, 0, 0};
        }

        // Return true if the current move is a grow
        bool is_grow() const {
            // Can't be a grow move if
            // the move is empty or
            //

            return !is_null() && m_grow != bitboard::ALL;
        }

        // Return true if move is "null"
        bool is_null() const {
            return m_grow == bitboard::ALL - 1;
        }

        // Get the coordinate from the starting and ending position
        std::pair<int, int> get_coords() const {
            auto start = bitboard::get_index(m_start);
            auto end = bitboard::get_index(m_end);
            return {start, end};
        }

        bool is_jump() const {
            auto [start_r, start_c] = bitboard::get_coord(m_start);
            auto [end_r, end_c] = bitboard::get_coord(m_end);
            return abs(start_r - end_r) >= 2 || abs(start_c - end_c) >= 2;
        }

        // A function to generate move from the starting position to the possible next position
        static void from_mask(mask pos, mask start, std::vector<move> &out) {
            while (pos > 0) {
                // get piece mask
                mask piece = 1ull << (bitboard::ROWS * bitboard::COLS - __builtin_clzll(pos) - 1);
                pos ^= piece;

                out.push_back(move{bitboard::ALL, start, piece});
            }
        }

        // A compare function
        bool operator==(const move &other) const {
            return m_grow == other.m_grow && m_start == other.m_start && m_end == other.m_end;
        }

        // Display a move
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
        // every position of the current lily pads on the board
        mask m_lilypads;
        // every position of the current red frogs and blue frogs on the board
        mask m_players[2]{};
        // turn indicator - whose turn is it
        int m_turn;
        // move counter
        int m_moves = 0;

        // Default Constructor
        pos() {
            // Initialise the initial position of the lily pads
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

            // Initialise the initial position of the red frog
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

            // Initialise the initial position of the blue frog
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

            // First turn is the red frog
            m_turn = RED;
            // Move counter
            m_moves = 0;
        }

        // Constructor
        pos(mask mLilypads, mask red, mask blue, int mTurn, int mMoves) : m_lilypads(mLilypads), m_players{red, blue},
                                                                          m_turn(mTurn), m_moves(mMoves) {
        }

        // A function that convert a position string to a position pos
        static pos from_string(const std::string &text, int turn) {
            mask red = 0;
            mask blue = 0;
            mask lilypads = 0;
            // ?? Not sure what is this for
            std::stringstream stream{text};
            std::string tmp;
            // For every row
            for (int row = 0; row < bitboard::ROWS; ++row) {
                // ?? Not sure what is this for
                stream >> tmp;
                // For every col
                for (int col = 0; col < bitboard::COLS; ++col) {
                    // Get the current number of the square on the board
                    int i = row * bitboard::COLS + col;
                    // ?? Not sure what is this for
                    stream >> tmp;
                    // Update the bitmask position of the lily pads
                    if (tmp == "_") {
                        lilypads |= (1ull << i);
                    }
                    // Update the bitmask position of the red frog
                    else if (tmp == "R") {
                        red |= (1ull << i);
                        lilypads |= (1ull << i);
                    }
                    // Update the bitmask position of the blue frog
                    else if (tmp == "B") {
                        blue |= (1ull << i);
                        lilypads |= (1ull << i);
                    }
                }
            }
            // Return the position of the board
            return pos{lilypads, red, blue, turn, 0};
        }

        bool has_jumps() const {
            return !get_jump_moves().empty();
            // TODO: fix this so that it detects any series of forward jumps not just immediate
            mask new_jumps = 0;
            // Get all the frogs on the board
            mask obst = m_players[0] | m_players[1];

            if (m_turn == RED) {
                new_jumps = bitboard::jump_down_forward(m_players[m_turn], obst);
            } else {
                new_jumps = bitboard::jump_up_forward(m_players[m_turn], obst);
            }


            new_jumps &= m_lilypads & (~obst);
            return new_jumps > 0;
        }

        //??
        std::vector<move> get_jump_moves() const {
            // Create a vector of moves
            std::vector<move> moves;
            // Get the current team
            //?? Seem like the player variable is useless here
            mask player = m_players[m_turn];
            // Store the current team as pieces
            mask pieces = player;

            // While there is a still member
            while (pieces > 0) {
                // get piece mask
                // Get the shift value for the left most member
                int i = (bitboard::ROWS * bitboard::COLS - 1) - __builtin_clzll(pieces);

                // Get the current member
                mask piece = 1ull << i;

                // Remove the current member from the team
                pieces ^= piece;

                // jumps
                // Get all the frogs on the board
                mask obst = m_players[0] | m_players[1];

                mask all_jumps = piece;
                mask next_jumps = piece;

                while (next_jumps > 0) {
                    // broadcast
                    mask new_jumps = 0;
                    // If the red team's turn
                    if (m_turn == RED) {
                        // Get the downward jump
                        new_jumps = bitboard::jump_down(next_jumps, obst);
                    } else {
                        // Get the upward jump
                        new_jumps = bitboard::jump_up(next_jumps, obst);
                    }

                    new_jumps &= m_lilypads & (~obst);
                    next_jumps = new_jumps & (~all_jumps);
                    all_jumps |= new_jumps;
                }

                int row = i / bitboard::COLS;

                all_jumps ^= piece;
                all_jumps &= ~(bitboard::HLINE << (row * bitboard::COLS));
                if (m_turn == board::RED) {
                    all_jumps &= ~(bitboard::HLINE << (std::min(7, row + 2) * bitboard::COLS));
                } else {
                    all_jumps &= ~(bitboard::HLINE << (std::max(0, row - 2) * bitboard::COLS));
                }
                move::from_mask(all_jumps, piece, moves);
            }

            // Return all the available jumping move
            return moves;
        }

        int num_unfinished_piece() {
            return 6 - __builtin_popcountll(m_players[RED] & bitboard::BOTTOM) + 6 -
                   __builtin_popcountll(m_players[BLUE] & bitboard::TOP);
        }

        //??
        std::vector<move> get_moves() const {

            std::vector<move> moves;

            // first handle grow
            mask player = m_players[m_turn];
            mask grown = bitboard::dilate(player);
            grown &= ~m_lilypads;
            moves.push_back(move{grown, 0, 0});

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


        // A function to do the move
        void push(move &play) {
            if (play.is_null()) {
                // If the move is empty
                // Do nothing
                m_moves -= 1;
            } else if (play.is_grow()) {
                // If the move is a grow move
                // Add all the growing lily pad to the current lily pad
                m_lilypads |= play.m_grow;
            } else {
                // If the move is a jump
                // Remove the lily pad that frog was currently on
                m_lilypads ^= play.m_start;
                // Remove the current frog from the team it from where it was on
                m_players[m_turn] ^= play.m_start;
                // Add the current frog back to the team on a new lily pad
                m_players[m_turn] |= play.m_end;
            }

            // Switch turn
            m_turn = 1 - m_turn;
            // Increase the move counter
            m_moves += 1;
        }

        // A function to undo the move
        void pop(move &play) {
            // Switch back to the current frog
            m_turn = 1 - m_turn;
            // Decrease the move counter
            m_moves -= 1;

            if (play.is_null()) {
                // If the move is empty
                // Do nothing
                m_moves += 1;
            } else if (play.is_grow()) {
                // If the move WAS a grow move
                // Remove all the growing lily pad from the current lily pad
                m_lilypads ^= play.m_grow;
            } else {
                // If the move is a jump
                // Add the lily pad back to where the frog was on
                m_lilypads |= play.m_start;
                // Remove the current frog  on a new lily pad
                m_players[m_turn] ^= play.m_end;
                // Add the current frog from the team it was on
                m_players[m_turn] |= play.m_start;
            }
        }

        // A function to check the current state of the game
        int get_state() const {
            // Store the winning sides respect to the team
            mask side[2] = {bitboard::BOTTOM, bitboard::TOP};
            // Check if the other team has won and return the winning team
            if ((m_players[1 - m_turn] & side[1 - m_turn]) == m_players[1 - m_turn]) {
                // If all the other team is on their winning side
                // return the other team
                return 1 - m_turn;
            }

            // If the other hasn't won yet
            // Check if the number of move exceed or equal to the draw move
            if (m_moves == DRAW_MOVES) {
                // Get the number of red frogs on the half of the board from its winning side
                int red_frogs = __builtin_popcountll(m_players[board::RED] & bitboard::BOTTOM_HALF);
                // Get the number of blue frogs on the half of the board from its winning side
                int blue_frogs = __builtin_popcountll(m_players[board::BLUE] & bitboard::TOP_HALF);

                // Return the winning team
                if (red_frogs > blue_frogs) {
                    return board::RED;
                } else if (blue_frogs > red_frogs) {
                    return board::BLUE;
                }

                return DRAW;
            }

            return NONE;
        }

        // A function to print the board
        std::string display() const {
            std::string out;
            for (int row = 0; row < bitboard::ROWS; ++row) {
                out += std::to_string(row) + " ";

                for (int col = 0; col < bitboard::COLS; ++col) {
                    int i = row * bitboard::COLS + col;
                    mask mask = 1ull << i;
                    if (m_players[RED] & mask) {
                        out += FRED("R");
                    } else if (m_players[BLUE] & mask) {
                        out += FBLU("B");
                    } else if (m_lilypads & mask) {
                        out += FGRN("_");
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

        uint64_t cantor(uint64_t a, uint64_t b) const {
            return (a + b + 1) * (a + b) / 2 + b;
        }

        uint64_t hash() const {
            return cantor(m_turn, cantor(m_players[0], cantor(m_players[1], m_lilypads)));
        }
    };
}// namespace board


#endif//FRECKER_BOARD_H
