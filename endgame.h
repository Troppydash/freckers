//
// Created by terry on 26/03/25.
//

#ifndef FRECKER_ENDGAME_H
#define FRECKER_ENDGAME_H

#include "board.h"
#include <iostream>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace endgame {
    int heuristic_one(board::pos &pos, int side, board::mask piece) {
        if (piece & board::bitboard::ENDS[side]) {
            return 0;
        }

        for (auto m: pos.get_piece_moves(piece)) {
            if (m.m_end & board::bitboard::ENDS[side]) {
                return 1;
            }
        }

//        if (side == board::RED) {
//            int row = __builtin_ctzll(piece) / 8;
//            board::mask mask = board::bitboard::ALL << (8 * (row + 1));
//            if ((pos.m_players[side] & mask) == 0) {
//                return std::max(2, 7-row);
//            }
//        }

        return 2;
    }

    int heuristic(board::pos &pos, int side) {
        board::mask mask = pos.m_players[side];
        int total = 0;
        while (mask > 0) {
            board::mask piece = 1ull << __builtin_ctzll(mask);
            mask ^= piece;

            total += heuristic_one(pos, side, piece);
        }

        return total;
    }

    struct node {
        std::shared_ptr<node> parent;
        int side;
        int depth;
        board::pos pos;
        int eval;
        board::move move;

        node() {
            throw std::runtime_error("cannot create node");
        }

        node(int _side, int _depth, board::pos _pos, board::move _move, std::shared_ptr<node> _parent)
            : parent(std::move(_parent)), side(_side), depth(_depth), pos(_pos), eval(0), move(_move) {
            eval = heuristic(pos, side);
        }

        int priority() const {
            return depth + eval;
        }

        bool operator>(const node &other) const {
            return priority() > other.priority();
        }

        bool operator==(const node &other) const {
            return pos == other.pos;
        }
    };

    // override node hash
    struct node_hash {
        uint64_t cantor(uint64_t a, uint64_t b) const {
            return (a + b + 1) * (a + b) / 2 + b;
        }

        size_t operator()(const endgame::node &node) const {
            // only need to hash from the last piece up
            board::mask mask = node.pos.m_players[node.side];

            board::mask all;
            if (node.side == board::RED) {
                int row = __builtin_ctzll(mask) / 8;
                all = board::bitboard::ALL << (8 * row);
            } else {
                int row = 7 - (63 - __builtin_clzll(mask)) / 8;
                all = board::bitboard::ALL >> (8 * row);
            }
            return cantor(node.pos.m_lilypads & all, mask & all);
        }
    };


    class a_star {
    public:
        // side we are performing a* on
        int m_side;
        // maximum depth to search
        int m_depth_bound;
        // best depth by opponent, don't search after that
        int m_best_depth;
        // number of nodes searched
        int m_counter;

        // stores (node => (moves to win, next move)
        std::unordered_map<node, std::tuple<int, board::move>, node_hash> m_cache;

        explicit a_star(int side) : m_side(side), m_depth_bound(0), m_best_depth(150), m_counter(0), m_cache() {}

        void reset(int depth) {
            // reset best_depth by opp to inf
            m_best_depth = 150;
            // limit depth searched to depth
            m_depth_bound = depth - 5;
            // reset counter
            m_counter = 0;
        }

        int search(const board::pos &pos, board::move &best_move) {
            std::priority_queue<node, std::vector<node>, std::greater<>> queue;

            board::pos initial = pos;
            initial.m_turn = m_side;
            node initial_node{m_side, 0, initial, board::move::null(), nullptr};

            queue.push(initial_node);

            std::unordered_set<node, node_hash> visited;
            while (!queue.empty()) {
                m_counter += 1;

                // failsafe for infinite loop
                if (m_counter > 1e9) {
                    std::cout << "failsafe\n";
                    return -1;
                }

                node top = queue.top();
                queue.pop();

                if (visited.contains(top)) {
                    continue;
                }
                visited.insert(top);

                top.pos.m_turn = 1 - m_side;
                // error for draws
                if (top.pos.get_state() == board::DRAW) {
                    top.pos.m_turn = m_side;
                    return -1;
                }

                // handle wins
                if (top.pos.get_state() == m_side) {
                    top.pos.m_turn = m_side;

                    int depth = top.depth;

                    node &current = top;
                    while (current.depth > 1) {
                        current = *current.parent;
                    }
                    best_move = current.move;
                    return depth;
                }
                top.pos.m_turn = m_side;

                // early prune, if we can't find a score that is better than opp
                if (top.depth > m_best_depth) {
                    return 150;
                }

                // exceeded bounds, return error
                if (top.depth > m_depth_bound) {
                    return -1;
                }

                // visit all moves
                std::shared_ptr<node> parent = std::make_shared<node>(top);
                for (auto move: top.pos.get_moves()) {
                    if (move.is_grow() && move.m_grow == 0) {
                        continue;
                    }
                    board::pos new_pos = top.pos;
                    new_pos.push(move);
                    new_pos.m_turn = m_side;
                    node new_node = {m_side, top.depth + 1, new_pos, move, parent};
                    queue.push(new_node);
                }
            }

            // impossible
            return -1;
        }
    };
}// namespace endgame


#endif//FRECKER_ENDGAME_H
