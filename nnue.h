//
// Created by terry on 4/03/25.
//

#ifndef FRECKERS_NNUE_H
#define FRECKERS_NNUE_H

#include <cinttypes>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <unordered_map>

namespace nnue {
    int32_t INT16_SCALE = (1<<15) - 1;

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
                m_weights.push_back(static_cast<int32_t> (round(std::min(1.0, std::max(-1.0, tmp)) * INT16_SCALE)));
            }

            for (int i = 0; i < m_outputs; ++i) {
                double tmp = 0.0;
                file >> tmp;
                m_biases.push_back(static_cast<int32_t> (round(std::min(1.0, std::max(-1.0, tmp)) * INT16_SCALE)));
            }

            for (int i = 0; i < m_outputs; ++i) {
                m_output.push_back(0);
            }

            m_accum = false;
        }

        void forward(std::vector<int32_t> &x) {
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
        std::vector<layer> m_layers;

        explicit seq(std::vector<std::string> &weights) {
            for (std::string &weight: weights) {
                m_layers.push_back(layer{weight});
            }

            m_layers[0].m_accum = true;
        }

        void init(std::vector<int32_t> &x) {
            for (int32_t &i: x) {
                i *= INT16_SCALE;
            }

            if (x.size() != m_layers[0].m_inputs) {
                printf("uh oh\n");
            }
            m_layers[0].forward(x);
        }

        double compute() {
            std::vector<int32_t> x = m_layers[0].m_output;
            for (int32_t &i: x) {
                i = std::max(0, std::min(i, INT16_SCALE));
            }

            m_layers[1].forward(x);
            for (int i = 2; i < m_layers.size(); ++i) {
                m_layers[i].forward(m_layers[i - 1].m_output);
            }

            return static_cast<double>(m_layers[m_layers.size() - 1].m_output[0]) / (double) INT16_SCALE;
        }

        void push(int idx) {
            m_layers[0].update_add(idx);
        }

        void pop(int idx) {
            m_layers[0].update_sub(idx);
        }
    };
}

#endif //FRECKERS_NNUE_H
