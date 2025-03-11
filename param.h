//
// Created by terry on 4/03/25.
//

#ifndef FRECKER_PARAM_H
#define FRECKER_PARAM_H

namespace param {
    int checkmate = 9000000;
    int inf = 10000000;

    int exact_flag = 0;
    int alpha_flag = 1;
    int beta_flag = 2;

    int max_depth = 50;

    int pv_move_score = (1 << 30);
}


#endif //FRECKER_PARAM_H
