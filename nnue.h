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


    template<typename T, std::size_t ALIGNMENT_IN_BYTES = 64>
    using AlignedVector = std::vector<T, AlignedAllocator<T, ALIGNMENT_IN_BYTES>>;

    class crelu {
    public:
        uint64_t m_inputs;
        AlignedVector<int16_t> m_output;

        explicit crelu(uint64_t inputs) : m_inputs(inputs), m_output(inputs, 0) {}

        static int16_t clipped_relu(int16_t x) {
            return static_cast<int16_t>(std::max(WEIGHT_ZERO, std::min(WEIGHT8_SCALE, x)));
        }

        void forward(const AlignedVector<int16_t> &x) {
            for (int i = 0; i < m_inputs; ++i) {
                m_output[i] = clipped_relu(x[i]);
            }

            //            __m256i zero = _mm256_setzero_si256();
            //            __m256i upper = _mm256_set1_epi16(WEIGHT8_SCALE);
            //
            //            constexpr uint64_t register_width = 256 / 16;
            //            constexpr uint64_t num_chunks = N / register_width;
            //
            //
            //            for (int i = 0; i < num_chunks; ++i) {
            //                __m256i input = _mm256_load_si256(reinterpret_cast<__m256i *>(&x[i * register_width]));
            //                __m256i output = _mm256_min_epi16(
            //                        _mm256_max_epi16(
            //                                input,
            //                                zero),
            //                        upper);
            //                _mm256_store_si256(reinterpret_cast<__m256i *>(m_output.data() + i * register_width), output);
            //            }
        }
    };

    class layer {
    public:
        AlignedVector<int16_t> m_weights;
        AlignedVector<int16_t> m_biases;
        AlignedVector<int16_t> m_output;
        uint64_t m_inputs;
        uint64_t m_outputs;

        explicit layer(std::string &weights) {
            std::ifstream file(weights);
            if (file.is_open()) {
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
            } else {
                throw std::runtime_error("cannot open file");
            }
        }

        void forward(const AlignedVector<int16_t> &x) {
            {
                for (int i = 0; i < m_outputs; ++i) {
                    m_output[i] = 0;
                }
                //                int i = 0;
                //                __m256i zero_vec = _mm256_setzero_si256();
                //                for (; i + 16 <= m_outputs; i += 16) {
                //                    _mm256_store_si256(reinterpret_cast<__m256i *>(m_output.data() + i), zero_vec);
                //                }
                //                for (; i < m_outputs; ++i) {
                //                    m_output[i] = 0;
                //                }
            }


            {
                for (int j = 0; j < m_inputs; ++j) {
                    size_t offset = j * m_outputs;
                    for (int i = 0; i < m_outputs; ++i) {
                        m_output[i] += static_cast<int16_t>(m_weights[offset + i] * x[j]);
                    }
                }


                //                for (int j = 0; j < m_inputs; ++j) {
                //                    size_t offset = j * m_outputs;
                //                    __m256i x_j = _mm256_set1_epi16(x[j]);
                //
                //                    int i = 0;
                //                    for (; i + 16 <= m_outputs; i += 16) {
                //                        __m256i weights = _mm256_load_si256(reinterpret_cast<__m256i *>(m_weights.data() + offset + i));
                //                        weights = _mm256_mullo_epi16(weights, x_j);
                //
                //                        __m256i result = _mm256_add_epi16(_mm256_load_si256(reinterpret_cast<__m256i *>(m_output.data() + i)),
                //                                                          weights);
                //                        _mm256_store_si256(
                //                                reinterpret_cast<__m256i *>(m_output.data() + i),
                //                                result);
                //                    }
                //                    for (; i < m_outputs; ++i) {
                //                        m_output[i] += static_cast<int16_t>(m_weights[offset + i]) * x[j];
                //                    }
                //                }
            }


            {
                for (int i = 0; i < m_outputs; ++i) {
                    m_output[i] += m_biases[i];
                }
                //                int i = 0;
                //                for (; i + 16 <= m_outputs; i += 16) {
                //                    __m256i result = _mm256_adds_epi16(_mm256_load_si256(reinterpret_cast<__m256i *>(m_output.data() + i)),
                //                                                       _mm256_load_si256(reinterpret_cast<__m256i *>(m_biases.data() + i)));
                //                    _mm256_store_si256(
                //                            reinterpret_cast<__m256i *>(m_output.data() + i),
                //                            result);
                //                }
                //
                //                for (; i < m_outputs; ++i) {
                //                    m_output[i] += m_biases[i];
                //                }
            }

            for (int i = 0; i < m_outputs; ++i) {
                m_output[i] /= WEIGHT8_SCALE;
            }


            //            if (!m_accum) {
            //                for (int i = 0; i < m_outputs; ++i) {
            //                    m_output[i] = clipped_relu(m_output[i]);
            //                }

            //                __m256i zero = _mm256_set1_epi16(WEIGHT_ZERO);
            //                __m256i upper = _mm256_set1_epi16(WEIGHT16_SCALE);
            //
            //                int i = 0;
            //                for (; i + 16 <= m_outputs; i += 16) {
            //                    __m256i result = _mm256_min_epi16(
            //                            _mm256_max_epi16(
            //                                    _mm256_load_si256(reinterpret_cast<__m256i *>(m_output.data() + i)),
            //                                    zero),
            //                            upper);
            //
            //                    result = _mm256_srli_epi16(result, 6);
            //                    _mm256_store_si256(
            //                            reinterpret_cast<__m256i *>(m_output.data() + i),
            //                            result);
            //                }
            //
            //                for (; i < m_outputs; ++i) {
            //                    m_output[i] = clipped_relu(m_output[i]);
            //                }
            //            }
        }

        void update_add(int idx) {
            for (int i = 0; i < m_outputs; ++i) {
                m_output[i] += static_cast<int16_t>(m_weights[idx * m_outputs + i]);
            }

            //            constexpr uint64_t register_width = 256 / 16;
            //            constexpr uint64_t num_chunks = 64 / register_width;
            //            static __m256i regs[num_chunks];
            //
            //            for (int i = 0; i < num_chunks; ++i) {
            //                regs[i] = _mm256_load_si256(reinterpret_cast<__m256i *>(m_output.data() + i * register_width));
            //            }
            //
            //            for (int i = 0; i < num_chunks; ++i) {
            //                regs[i] = _mm256_add_epi16(regs[i], _mm256_load_si256(reinterpret_cast<__m256i *>(m_weights.data() + (idx * m_outputs) + i * register_width)));
            //            }
            //
            //            for (int i = 0; i < num_chunks; ++i) {
            //                _mm256_store_si256(reinterpret_cast<__m256i *>(m_output.data() + i * register_width), regs[i]);
            //            }
        }

        void update_sub(int idx) {
            for (int i = 0; i < m_outputs; ++i) {
                m_output[i] -= static_cast<int16_t>(m_weights[idx * m_outputs + i]);
            }

            //            constexpr uint64_t register_width = 256 / 16;
            //            constexpr uint64_t num_chunks = 64 / register_width;
            //            static __m256i regs[num_chunks];
            //
            //            for (int i = 0; i < num_chunks; ++i) {
            //                regs[i] = _mm256_load_si256(reinterpret_cast<__m256i *>(m_output.data() + i * register_width));
            //            }
            //
            //            for (int i = 0; i < num_chunks; ++i) {
            //                regs[i] = _mm256_sub_epi16(regs[i], _mm256_load_si256(reinterpret_cast<__m256i *>(m_weights.data() + (idx * m_outputs) + i * register_width)));
            //            }
            //
            //            for (int i = 0; i < num_chunks; ++i) {
            //                _mm256_store_si256(reinterpret_cast<__m256i *>(m_output.data() + i * register_width), regs[i]);
            //            }
        }
    };

    class seq {
    public:
        layer m_red_accum;
        layer m_blue_accum;
        crelu m_relu1;
        layer m_layer2;
        crelu m_relu2;
        layer m_layer3;
        crelu m_relu3;
        layer m_layer4;

        AlignedVector<int16_t> m_accum_output;

        // caching
        bool m_changed;
        int m_last_flip;


        explicit seq(std::vector<std::string> &weights)
            : m_red_accum{weights[0]}, m_blue_accum{weights[0]},
              m_relu1{m_red_accum.m_outputs + m_blue_accum.m_outputs},
              m_layer2{weights[1]},
              m_relu2{m_layer2.m_outputs},
              m_layer3{weights[2]},
              m_relu3{m_layer3.m_outputs},
              m_layer4{weights[3]},
              m_accum_output(m_red_accum.m_outputs + m_blue_accum.m_outputs, 0) {
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
        }

        void set_unchanged(int flip) {
            m_changed = false;
            m_last_flip = flip;
        }

        bool is_changed(int flip) const {
            return m_changed || m_last_flip != flip;
        }

        int compute(int flip) {
            if (is_changed(flip)) {
                size_t offset = m_red_accum.m_outputs;
                size_t blue_offset = (1 - flip) * offset;
                size_t red_offset = flip * offset;

                for (int i = 0; i < m_red_accum.m_outputs; ++i) {
                    m_accum_output[flip * offset + i] = m_red_accum.m_output[i];
                }
                //                {
                //                    constexpr uint64_t register_width = 256 / 16;
                //                    constexpr uint64_t num_chunks = 64 / register_width;
                //
                //                    for (int i = 0; i < num_chunks; ++i) {
                //                        _mm256_store_si256(
                //                                reinterpret_cast<__m256i *>(m_accum_output.data() + red_offset + i * register_width),
                //                                _mm256_load_si256(reinterpret_cast<__m256i *>(m_red_accum.m_output.data() + i * register_width)));
                //                    }
                //                }

                for (int i = 0; i < m_blue_accum.m_outputs; ++i) {
                    m_accum_output[blue_offset + i] = m_blue_accum.m_output[i];
                }
                //                {
                //                    constexpr uint64_t register_width = 256 / 16;
                //                    constexpr uint64_t num_chunks = 64 / register_width;
                //
                //                    for (int i = 0; i < num_chunks; ++i) {
                //                        _mm256_store_si256(
                //                                reinterpret_cast<__m256i *>(m_accum_output.data() + blue_offset + i * register_width),
                //                                _mm256_load_si256(reinterpret_cast<__m256i *>(m_blue_accum.m_output.data() + i * register_width)));
                //                    }
                //                }


                m_relu1.forward(m_accum_output);
                m_layer2.forward(m_relu1.m_output);
                m_relu2.forward(m_layer2.m_output);
                m_layer3.forward(m_relu2.m_output);
                m_relu3.forward(m_layer3.m_output);
                m_layer4.forward(m_relu3.m_output);

                set_unchanged(flip);
            }

            int eval = static_cast<int>(m_layer4.m_output[0]);
            return (eval * 40 * 100) / WEIGHT8_SCALE;
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
