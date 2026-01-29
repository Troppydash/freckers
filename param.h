//
// Created by terry on 4/03/25.
//

#ifndef FRECKER_PARAM_H
#define FRECKER_PARAM_H

namespace param
{
constexpr int checkmate = 9000000;
constexpr int inf = 10000000;

constexpr int exact_flag = 0;
constexpr int alpha_flag = 1;
constexpr int beta_flag = 2;

constexpr int max_depth = 100;

constexpr int base_score = (1 << 30);

constexpr int pv_move_score = 400;
constexpr int killer_move_score = -10;
constexpr int killer_move_score2 = -20;
}


#endif //FRECKER_PARAM_H
