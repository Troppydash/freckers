//
// Created by terry on 4/03/25.
//

#ifndef FRECKERS_NNUE_H
#define FRECKERS_NNUE_H

#include "aligned.h"
#include <cinttypes>
#include <cmath>
#include <cstring>
#include <fstream>
#include <immintrin.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace nnue {
    constexpr int16_t WEIGHT8_SCALE = static_cast<int16_t>(1 << 6);
    constexpr int16_t WEIGHT16_SCALE = static_cast<int16_t>(1 << 12);
    constexpr int16_t WEIGHT_ZERO = static_cast<int16_t>(0);

    int16_t clipped_relu(int16_t x) {
        return static_cast<int16_t>(std::max(WEIGHT_ZERO, std::min(WEIGHT16_SCALE, x)) >> 6);
    }

    template<typename T, std::size_t ALIGNMENT_IN_BYTES = 64>
    using AlignedVector = std::vector<T, AlignedAllocator<T, ALIGNMENT_IN_BYTES>>;


    class layer {
    public:
        // accumulator specifics
        bool m_accum;

        AlignedVector<int16_t> m_weights;
        AlignedVector<int16_t> m_biases;
        AlignedVector<int16_t> m_output;
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
                m_weights.push_back(static_cast<int16_t>(round(std::min(2.0, std::max(-2.0, tmp)) * WEIGHT8_SCALE)));
            }

            for (int i = 0; i < m_outputs; ++i) {
                double tmp = 0.0;
                file >> tmp;
                m_biases.push_back(static_cast<int16_t>(round(std::min(2.0, std::max(-2.0, tmp)) * WEIGHT16_SCALE)));
            }

            for (int i = 0; i < m_outputs; ++i) {
                m_output.push_back(0);
            }

            m_accum = false;
        }

        void forward(AlignedVector<int16_t> &x) {
            {
                //                for (int i = 0; i < m_outputs; ++i) {
                //                    m_output[i] = 0;
                //                }
                int i = 0;
                __m256i zero_vec = _mm256_setzero_si256();
                for (; i + 16 <= m_outputs; i += 16) {
                    _mm256_store_si256(reinterpret_cast<__m256i *>(m_output.data() + i), zero_vec);
                }
                for (; i < m_outputs; ++i) {
                    m_output[i] = 0;
                }
            }


            {
                //            for (int j = 0; j < m_inputs; ++j) {
                //                size_t offset = j * m_outputs;
                //                for (int i = 0; i < m_outputs; ++i) {
                //                    m_output[i] += static_cast<int16_t>(m_weights[offset + i]) * x[j];
                //                }
                //            }
                //

                for (int j = 0; j < m_inputs; ++j) {
                    size_t offset = j * m_outputs;
                    __m256i x_j = _mm256_set1_epi16(x[j]);

                    int i = 0;
                    for (; i + 16 <= m_outputs; i += 16) {
                        __m256i weights = _mm256_load_si256(reinterpret_cast<__m256i *>(m_weights.data() + offset + i));
                        weights = _mm256_mullo_epi16(weights, x_j);

                        __m256i result = _mm256_add_epi16(_mm256_load_si256(reinterpret_cast<__m256i *>(m_output.data() + i)),
                                                          weights);
                        _mm256_store_si256(
                                reinterpret_cast<__m256i *>(m_output.data() + i),
                                result);
                    }
                    for (; i < m_outputs; ++i) {
                        m_output[i] += static_cast<int16_t>(m_weights[offset + i]) * x[j];
                    }
                }
            }


            {
                // for (int i = 0; i < m_outputs; ++i) {
                //     m_output[i] += m_biases[i];
                // }
                int i = 0;
                for (; i + 16 <= m_outputs; i += 16) {
                    __m256i result = _mm256_adds_epi16(_mm256_load_si256(reinterpret_cast<__m256i *>(m_output.data() + i)),
                                                       _mm256_load_si256(reinterpret_cast<__m256i *>(m_biases.data() + i)));
                    _mm256_store_si256(
                            reinterpret_cast<__m256i *>(m_output.data() + i),
                            result);
                }

                for (; i < m_outputs; ++i) {
                    m_output[i] += m_biases[i];
                }
            }


            if (!m_accum) {
                //                for (int i = 0; i < m_outputs; ++i) {
                //                    m_output[i] = clipped_relu(m_output[i]);
                //                }

                __m256i zero = _mm256_set1_epi16(WEIGHT_ZERO);
                __m256i upper = _mm256_set1_epi16(WEIGHT16_SCALE);

                int i = 0;
                for (; i + 16 <= m_outputs; i += 16) {
                    __m256i result = _mm256_min_epi16(
                            _mm256_max_epi16(
                                    _mm256_load_si256(reinterpret_cast<__m256i *>(m_output.data() + i)),
                                    zero),
                            upper);

                    result = _mm256_srli_epi16(result, 6);
                    _mm256_store_si256(
                            reinterpret_cast<__m256i *>(m_output.data() + i),
                            result);
                }

                for (; i < m_outputs; ++i) {
                    m_output[i] = clipped_relu(m_output[i]);
                }
            }
        }

        void update_add(int idx) {
            //            for (int i = 0; i < m_outputs; ++i) {
            //                m_output[i] += static_cast<int16_t>(m_weights[idx * m_outputs + i]) * WEIGHT8_SCALE;
            //            }

            __m256i scalar = _mm256_set1_epi16(WEIGHT8_SCALE);
            int i = 0;
            for (; i + 16 <= m_outputs; i += 16) {
                __m256i weights = _mm256_load_si256(reinterpret_cast<__m256i *>(m_weights.data() + (idx * m_outputs) + i));
                weights = _mm256_mullo_epi16(weights, scalar);

                __m256i result = _mm256_add_epi16(_mm256_load_si256(reinterpret_cast<__m256i *>(m_output.data() + i)),
                                                  weights);

                _mm256_store_si256(
                        reinterpret_cast<__m256i *>(m_output.data() + i),
                        result);
            }
            for (; i < m_outputs; ++i) {
                m_output[i] += m_weights[idx * m_outputs + i] * WEIGHT8_SCALE;
            }
        }

        void update_sub(int idx) {
            //            for (int i = 0; i < m_outputs; ++i) {
            //                m_output[i] -= static_cast<int16_t>(m_weights[idx * m_outputs + i]) * WEIGHT8_SCALE;
            //            }

            __m256i scalar = _mm256_set1_epi16(WEIGHT8_SCALE);
            int i = 0;
            for (; i + 16 <= m_outputs; i += 16) {
                __m256i weights = _mm256_load_si256(reinterpret_cast<__m256i *>(m_weights.data() + (idx * m_outputs) + i));
                weights = _mm256_mullo_epi16(weights, scalar);

                __m256i result = _mm256_sub_epi16(_mm256_load_si256(reinterpret_cast<__m256i *>(m_output.data() + i)),
                                                  weights);

                _mm256_store_si256(
                        reinterpret_cast<__m256i *>(m_output.data() + i),
                        result);
            }
            for (; i < m_outputs; ++i) {
                m_output[i] -= m_weights[idx * m_outputs + i] * WEIGHT8_SCALE;
            }
        }
    };

    class seq {
    public:
        layer m_red_accum;
        layer m_blue_accum;
        std::vector<layer> m_layers;
        AlignedVector<int16_t> m_accum_output;

        // caching
        bool m_changed;
        int m_last_flip;


        explicit seq(std::vector<std::string> &weights)
            : m_red_accum{weights[0]}, m_blue_accum{weights[0]}, m_accum_output(m_red_accum.m_outputs + m_blue_accum.m_outputs, 0) {
            m_red_accum.m_accum = true;
            m_blue_accum.m_accum = true;

            for (int i = 1; i < weights.size(); ++i) {
                m_layers.emplace_back(weights[i]);
            }
            m_layers[m_layers.size() - 1].m_accum = true;

            m_changed = true;
            m_last_flip = -1;
            set_changed();
        }

        void init(std::vector<int16_t> &red, std::vector<int16_t> &blue) {
            for (auto &i: red) {
                i *= WEIGHT8_SCALE;
            }

            for (auto &i: blue) {
                i *= WEIGHT8_SCALE;
            }

            AlignedVector<int16_t> aligned_red(red.begin(), red.end());
            AlignedVector<int16_t> aligned_blue(blue.begin(), blue.end());
            m_red_accum.forward(aligned_red);
            m_blue_accum.forward(aligned_blue);
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
                        m_accum_output[i] = clipped_relu(m_blue_accum.m_output[i]);
                    }

                    for (int i = 0; i < m_red_accum.m_outputs; ++i) {
                        m_accum_output[offset + i] = clipped_relu(m_red_accum.m_output[i]);
                    }
                } else {
                    for (int i = 0; i < m_blue_accum.m_outputs; ++i) {
                        m_accum_output[offset + i] = clipped_relu(m_blue_accum.m_output[i]);
                    }

                    for (int i = 0; i < m_red_accum.m_outputs; ++i) {
                        m_accum_output[i] = clipped_relu(m_red_accum.m_output[i]);
                    }
                }


                m_layers[0].forward(m_accum_output);
                for (int i = 1; i < m_layers.size(); ++i) {
                    m_layers[i].forward(m_layers[i - 1].m_output);
                }

                set_unchanged(flip);
            }

            int eval = static_cast<int>(m_layers[m_layers.size() - 1].m_output[0]);
            return (eval * 40 * 100) / WEIGHT16_SCALE;
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
