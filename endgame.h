//
// Created by terry on 26/03/25.
//

#ifndef FRECKER_ENDGAME_H
#define FRECKER_ENDGAME_H

#include "board.h"
namespace endgame {
    class a_star {
    public:
        board::pos m_pos;

        explicit a_star(board::pos &pos) : m_pos(pos) {}




    };
}

#endif//FRECKER_ENDGAME_H
