//
// Created by terry on 4/03/25.
//

#ifndef FRECKERS_NNUE_H
#define FRECKERS_NNUE_H

#include <cinttypes>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace nnue {
    int32_t INT16_SCALE = (1 << 14) - 1;

    class layer {
    public:
        bool m_accum;
        std::vector<int32_t> m_weights;
        std::vector<int32_t> m_biases;
        std::vector<int32_t> m_output;
        uint64_t m_inputs;
        uint64_t m_outputs;

        explicit layer(std::string &weights) {
            std::ifstream file(weights);
            m_inputs = 0;
            m_outputs = 0;
            file >> m_inputs >> m_outputs;

            for (int i = 0; i < m_inputs * m_outputs; ++i) {
                double tmp = 0.0;
                file >> tmp;
                m_weights.push_back(static_cast<int32_t>(round(std::min(1.0, std::max(-1.0, tmp)) * INT16_SCALE)));
            }

            for (int i = 0; i < m_outputs; ++i) {
                double tmp = 0.0;
                file >> tmp;
                m_biases.push_back(static_cast<int32_t>(round(std::min(1.0, std::max(-1.0, tmp)) * INT16_SCALE)));
            }

            for (int i = 0; i < m_outputs; ++i) {
                m_output.push_back(0);
            }

            m_accum = false;
        }

        void forward(const std::vector<int32_t> &x) {
            for (int i = 0; i < m_outputs; ++i) {
                m_output[i] = 0;
            }

            for (int j = 0; j < m_inputs; ++j) {
                for (int i = 0; i < m_outputs; ++i) {
                    m_output[i] += m_weights[j * m_outputs + i] * x[j];
                }
            }

            for (int i = 0; i < m_outputs; ++i) {
                m_output[i] = (m_output[i] / INT16_SCALE) + m_biases[i];
            }

            if (!m_accum) {
                for (int i = 0; i < m_outputs; ++i) {
                    m_output[i] = std::max(0, std::min(m_output[i], INT16_SCALE));
                }
            }
        }

        void update_add(int idx) {
            for (int i = 0; i < m_outputs; ++i) {
                m_output[i] += m_weights[idx * m_outputs + i];
            }
        }

        void update_sub(int idx) {
            for (int i = 0; i < m_outputs; ++i) {
                m_output[i] -= m_weights[idx * m_outputs + i];
            }
        }
    };

    class seq {
    public:
        layer m_red_accum;
        layer m_blue_accum;
        std::vector<layer> m_layers;

        std::vector<int> m_temp;


        explicit seq(std::vector<std::string> &weights)
            : m_red_accum{weights[0]}, m_blue_accum{weights[0]}, m_temp(64*64*2, 0) {
            m_red_accum.m_accum = true;
            m_blue_accum.m_accum = true;

            for (int i = 1; i < weights.size(); ++i) {
                m_layers.push_back(layer{weights[i]});
            }
        }

        void init(std::vector<int32_t> &red, std::vector<int32_t> &blue) {
            for (int32_t &i: red) {
                i *= INT16_SCALE;
            }

            for (int32_t &i: blue) {
                i *= INT16_SCALE;
            }

            m_red_accum.forward(red);
            m_blue_accum.forward(blue);
        }

        double compute(int flip) {
            std::vector<int32_t> output;
            if (flip) {
                for (const int32_t &i: m_blue_accum.m_output) {
                    output.push_back(std::max(0, std::min(i, INT16_SCALE)));
                }

                for (const int32_t &i: m_red_accum.m_output) {
                    output.push_back(std::max(0, std::min(i, INT16_SCALE)));
                }


            } else {
                for (const int32_t &i: m_red_accum.m_output) {
                    output.push_back(std::max(0, std::min(i, INT16_SCALE)));
                }
                for (const int32_t &i: m_blue_accum.m_output) {
                    output.push_back(std::max(0, std::min(i, INT16_SCALE)));
                }
            }


            m_layers[0].forward(output);
            for (int i = 1; i < m_layers.size(); ++i) {
                m_layers[i].forward(m_layers[i - 1].m_output);
            }

            return static_cast<double>(m_layers[m_layers.size() - 1].m_output[0]) / (double) INT16_SCALE;
        }

        void push_red(int idx) {
            m_red_accum.update_add(idx);
//            if (m_temp[idx] == 1) {
//                printf("uh oh");
//            }
//            m_temp[idx] += 1;
        }

        void pop_red(int idx) {
            m_red_accum.update_sub(idx);
//            if (m_temp[idx] == 0) {
//                printf("uh oh");
//            }
//            m_temp[idx] -= 1;
        }

        void push_blue(int idx) {
            m_blue_accum.update_add(64 * 64 - idx - 1);
        }

        void pop_blue(int idx) {
            m_blue_accum.update_sub(64 * 64 - idx - 1);
        }
    };
}// namespace nnue

#endif//FRECKERS_NNUE_H
