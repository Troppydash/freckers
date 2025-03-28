//
// Created by terry on 4/03/25.
//

#ifndef FRECKERS_NNUE_H
#define FRECKERS_NNUE_H

#include <cinttypes>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace nnue {
    int INT16_SCALE_BIT = 14;
    int32_t INT16_SCALE = (1 << INT16_SCALE_BIT);

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
                size_t offset = j * m_outputs;
                for (int i = 0; i < m_outputs; ++i) {
                    m_output[i] += m_weights[offset + i] * x[j];
                }
            }

            for (int i = 0; i < m_outputs; ++i) {
                m_output[i] >>= INT16_SCALE_BIT;
            }


            for (int i = 0; i < m_outputs; ++i) {
                m_output[i] += m_biases[i];
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
        std::vector<int32_t> m_accum_output;

        // caching
        bool m_changed;
        int m_last_flip;


        explicit seq(std::vector<std::string> &weights)
            : m_red_accum{weights[0]}, m_blue_accum{weights[0]}, m_accum_output(m_red_accum.m_outputs + m_blue_accum.m_outputs, 0) {
            m_red_accum.m_accum = true;
            m_blue_accum.m_accum = true;

            for (int i = 1; i < weights.size(); ++i) {
                m_layers.push_back(layer{weights[i]});
            }
            m_layers[m_layers.size() - 1].m_accum = true;

            m_changed = true;
            m_last_flip = -1;
            set_changed();
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
            set_changed();
        }


        void set_changed() {
            m_changed = true;
            m_last_flip = -1;
        }

        void set_unchanged(int flip) {
            m_changed = false;
            m_last_flip = flip;
        }

        bool is_changed(int flip) {
            return m_changed || m_last_flip != flip;
        }

        int compute(int flip) {
            if (is_changed(flip)) {
                size_t offset = m_red_accum.m_outputs;
                if (flip) {
                    for (int i = 0; i < m_blue_accum.m_outputs; ++i) {
                        m_accum_output[i] = std::max(0, std::min(m_blue_accum.m_output[i], INT16_SCALE));
                    }

                    for (int i = 0; i < m_red_accum.m_outputs; ++i) {
                        m_accum_output[offset+i] = std::max(0, std::min(m_red_accum.m_output[i], INT16_SCALE));
                    }
                } else {
                    for (int i = 0; i < m_blue_accum.m_outputs; ++i) {
                        m_accum_output[offset+i] = std::max(0, std::min(m_blue_accum.m_output[i], INT16_SCALE));
                    }

                    for (int i = 0; i < m_red_accum.m_outputs; ++i) {
                        m_accum_output[i] = std::max(0, std::min(m_red_accum.m_output[i], INT16_SCALE));
                    }
                }


                m_layers[0].forward(m_accum_output);
                for (int i = 1; i < m_layers.size(); ++i) {
                    m_layers[i].forward(m_layers[i - 1].m_output);
                }

                set_unchanged(flip);
            }

            int64_t eval = m_layers[m_layers.size() - 1].m_output[0];
            return static_cast<int>((eval * 40 * 100) / INT16_SCALE);
        }

        void push_red(int idx) {
            set_changed();
            m_red_accum.update_add(idx);
        }

        void pop_red(int idx) {
            set_changed();
            m_red_accum.update_sub(idx);
        }

        void push_blue(int idx) {
            set_changed();
            m_blue_accum.update_add(idx);
        }

        void pop_blue(int idx) {
            set_changed();
            m_blue_accum.update_sub(idx);
        }
    };
}// namespace nnue

#endif//FRECKERS_NNUE_H
