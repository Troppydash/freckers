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
                int row = __builtin_clzll(mask) / 8;
                all = board::bitboard::ALL << (8 * row);
            } else {
                int row = 63 - __builtin_clzll(mask) / 8;
                all = board::bitboard::ALL >> (8 * row);
            }
            return cantor(node.pos.m_lilypads & all, mask & all);
        }
    };


    class a_star {
    public:
        int m_side;
        int m_bound;
        int m_best_score;
        int m_counter;

        std::unordered_map<node, int, node_hash> m_cache;

        explicit a_star(int side, int bound, int best_score) : m_side(side), m_bound(bound), m_best_score(best_score), m_counter(0), m_cache() {}


        int search(const board::pos &pos, board::move &best_move) {
            std::priority_queue<node, std::vector<node>, std::greater<>> queue;

            board::pos initial = pos;
            initial.m_turn = m_side;
            node initial_node{m_side, 0, initial, board::move::null(), nullptr};
            queue.push(initial_node);

            std::unordered_set<node, node_hash> visited;
            while (!queue.empty()) {
                m_counter += 1;
                node top = queue.top();
                queue.pop();

                if (visited.contains(top)) {
                    continue;
                }
                visited.insert(top);

                if (top.depth >= m_best_score + 1) {
                    // early prune
                    return 1e9;
                }

                if (m_counter >= m_bound) {
                    return -1;
                }

                top.pos.m_turn = 1 - m_side;
                if (top.pos.get_state() == board::DRAW) {
                    // error for draws
                    return -1;
                }

                if (top.pos.get_state() == m_side) {
                    top.pos.m_turn = m_side;

                    int depth = top.depth;

                    node &current = top;
//                    int to_end = 0;
                    while (current.depth > 1) {
                        current = *current.parent;
//                        to_end += 1;
//                        m_cache[current] = to_end;
                    }
                    best_move = current.move;
                    return depth;
                }
                top.pos.m_turn = m_side;

                // create shared ptr
                std::shared_ptr<node> parent = std::make_shared<node>(top);

                // visit all moves
                for (auto move: top.pos.get_moves()) {
                    if (move.is_grow() && move.m_grow == 0) {
                        continue;
                    }
                    board::pos new_pos = top.pos;
                    new_pos.push(move);
                    new_pos.m_turn = m_side;
                    node new_node = {m_side, top.depth + 1, new_pos, move, parent};
//                    if (m_cache.contains(new_node)) {
//                        new_node.eval = m_cache[new_node];
//                    }
                    queue.push(new_node);
                }
            }

            // impossible
            return -1;
        }
    };
}// namespace endgame


#endif//FRECKER_ENDGAME_H
