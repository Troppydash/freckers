#pragma once

#include "board.h"
#include "param.h"

#include <cinttypes>
#include <immintrin.h>

constexpr size_t HIDDEN_SIZE = 256;
constexpr int16_t QA = 255;
constexpr int16_t QB = 64;
constexpr int32_t SCALE = 40 * 100;

#pragma pack(push, 1)
struct alignas(64) accumulator {
    int16_t vals[HIDDEN_SIZE];
};

struct network {
    // QA quant, 64*3 -> hidden_size
    accumulator feature_weights[64 * 3];
    // QA quant, hidden_size -> hidden_size
    accumulator feature_bias;

    // QB quant, 2*hidden_size -> 1
    alignas(64) int16_t output_weights[2 * HIDDEN_SIZE];
    // QA * QB quant, 1 -> 1
    int16_t output_bias;
};
#pragma pack(pop)


inline int32_t screlu(int16_t x) {
    const int32_t val = std::clamp(x, static_cast<int16_t>(0), QA);
    return val * val;
}

struct nnue2 {
    network m_network{};

    // side = 0, [0, lily]
    // side = 1, [1, lily]
    accumulator m_sides[2][param::max_depth]{};
    int m_ply;

    explicit nnue2() = default;

    bool load_network(const std::string &path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);

        if (!file.is_open()) {
            std::cerr << "[cpp] failed to open " << path << std::endl;
            return false;
        }

        // Sanity check: Does the file size match our struct size?
        std::streamsize size = file.tellg();
        if (size != sizeof(network)) {
            std::cerr << "[cpp] size mismatch! File: " << size
                      << " bytes, Struct: " << sizeof(network) << " bytes." << std::endl;
            return false;
        }

        // Go back to the start and read the whole thing
        file.seekg(0, std::ios::beg);
        if (file.read(reinterpret_cast<char *>(&m_network), sizeof(network))) {
            return true;
        }

        return false;
    }

    void initialize(const board::pos &position) {
        m_ply = 0;

#pragma omp simd
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[0][m_ply].vals[i] = m_sides[1][m_ply].vals[i] = m_network.feature_bias.vals[i];
        }

        // red pieces
        board::mask m = position.m_players[board::RED];
        while (m > 0) {
            int i = __builtin_ctzll(m);
            m -= (1ull << i);

            add_feature(board::RED, i);
            add_feature(board::BLUE, 64 + (i ^ 56));
        }

        // blue pieces
        m = position.m_players[board::BLUE];
        while (m > 0) {
            int i = __builtin_ctzll(m);
            m -= (1ull << i);

            add_feature(board::BLUE, i ^ 56);
            add_feature(board::RED, 64 + i);
        }

        // lily
        m = position.m_lilypads;
        while (m > 0) {
            int i = __builtin_ctzll(m);
            m -= (1ull << i);

            addlily_feature(64 * 2 + i, 64 * 2 + (i ^ 56));
        }
    }

    int32_t evaluate(int side2move) const {
        int32_t output = 0;

        // side2move -> output, output in QA * QA * QB
        // not-side2move -> output, output in QA * QA * QB
        // for (int i = 0; i < HIDDEN_SIZE; ++i) {
        //     auto &us = m_sides[side2move][m_ply];
        //     auto &them = m_sides[side2move ^ 1][m_ply];
        //     output += screlu(us.vals[i]) * m_network.output_weights[i];
        //     output += screlu(them.vals[i]) * m_network.output_weights[HIDDEN_SIZE + i];
        // }
        //
        //
        //

        // SIMD VECTORIZATION
        const __m256i vec_zero = _mm256_setzero_si256();
        const __m256i vec_qa = _mm256_set1_epi16(QA);
        __m256i sum = vec_zero;

        const int16_t *__restrict us_ptr = m_sides[side2move][m_ply].vals;
        const int16_t *__restrict them_ptr = m_sides[side2move ^ 1][m_ply].vals;
        const int16_t *__restrict weight_ptr = m_network.output_weights;
        const int16_t *__restrict weight_ptr2 = m_network.output_weights + HIDDEN_SIZE;

        for (int i = 0; i < HIDDEN_SIZE; i += 16) {
            const __m256i us_ = _mm256_load_si256((__m256i *) (us_ptr + i));
            const __m256i them_ = _mm256_load_si256((__m256i *) (them_ptr + i));
            const __m256i us_weights = _mm256_load_si256((__m256i *) (weight_ptr + i));
            const __m256i them_weights = _mm256_load_si256((__m256i *) (weight_ptr2 + i));

            const __m256i us_clamped = _mm256_min_epi16(_mm256_max_epi16(us_, vec_zero), vec_qa);
            const __m256i them_clamped = _mm256_min_epi16(_mm256_max_epi16(them_, vec_zero), vec_qa);

            // do (clamp*weight)*clamp
            const __m256i us_results = _mm256_madd_epi16(_mm256_mullo_epi16(us_weights, us_clamped), us_clamped);
            const __m256i them_results = _mm256_madd_epi16(_mm256_mullo_epi16(them_weights, them_clamped), them_clamped);

            sum = _mm256_add_epi32(sum, us_results);
            sum = _mm256_add_epi32(sum, them_results);
        }

        __m128i x128 = _mm_add_epi32(_mm256_extracti128_si256(sum, 1),
                                     _mm256_castsi256_si128(sum));
        __m128i x64 = _mm_add_epi32(x128, _mm_shuffle_epi32(x128, _MM_SHUFFLE(1, 0, 3, 2)));
        __m128i x32 = _mm_add_epi32(x64, _mm_shuffle_epi32(x64, _MM_SHUFFLE(1, 1, 1, 1)));
        output += _mm_cvtsi128_si32(x32);

        // output in QA * QB
        output /= static_cast<int32_t>(QA);
        output += static_cast<int32_t>(m_network.output_bias);

        // output in [-SCALE, SCALE]
        output *= SCALE;
        output /= static_cast<int32_t>(QA) * static_cast<int32_t>(QB);

        return std::clamp(output, -SCALE, SCALE);
    }

    void clone_ply() {
#pragma omp simd
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[board::RED][m_ply + 1].vals[i] = m_sides[board::RED][m_ply].vals[i];
            m_sides[board::BLUE][m_ply + 1].vals[i] = m_sides[board::BLUE][m_ply].vals[i];
        }
    }

    void add_feature(int side, int feature_idx) {
        const int16_t *__restrict weight = m_network.feature_weights[feature_idx].vals;
#pragma omp simd
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[side][m_ply].vals[i] += weight[i];
        }
    }

    void remove_feature(int side, int feature_idx) {
        const int16_t *__restrict weight = m_network.feature_weights[feature_idx].vals;
#pragma omp simd
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[side][m_ply].vals[i] -= weight[i];
        }
    }

    void move_feature(int side, int a, int b) {
        const int16_t *__restrict weight_a = m_network.feature_weights[a].vals;
        const int16_t *__restrict weight_b = m_network.feature_weights[b].vals;
#pragma omp simd
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[side][m_ply].vals[i] = m_sides[side][m_ply].vals[i] - weight_a[i] + weight_b[i];
        }
    }

    void addlily_feature(int a, int b) {
        const int16_t *__restrict weight_a = m_network.feature_weights[a].vals;
        const int16_t *__restrict weight_b = m_network.feature_weights[b].vals;
#pragma omp simd
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[board::RED][m_ply].vals[i] += weight_a[i];
            m_sides[board::BLUE][m_ply].vals[i] += weight_b[i];
        }
    }

    void sublily_feature(int a, int b) {
        const int16_t *__restrict weight_a = m_network.feature_weights[a].vals;
        const int16_t *__restrict weight_b = m_network.feature_weights[b].vals;
#pragma omp simd
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[board::RED][m_ply].vals[i] -= weight_a[i];
            m_sides[board::BLUE][m_ply].vals[i] -= weight_b[i];
        }
    }


    void subaddsub_feature(int side, int a, int b, int c) {
        const int16_t *__restrict weight_a = m_network.feature_weights[a].vals;
        const int16_t *__restrict weight_b = m_network.feature_weights[b].vals;
        const int16_t *__restrict weight_c = m_network.feature_weights[c].vals;
#pragma omp simd
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[side][m_ply].vals[i] = m_sides[side][m_ply].vals[i] - weight_a[i] + weight_b[i] - weight_c[i];
        }
    }

    void subaddadd_feature(int side, int a, int b, int c) {
        const int16_t *__restrict weight_a = m_network.feature_weights[a].vals;
        const int16_t *__restrict weight_b = m_network.feature_weights[b].vals;
        const int16_t *__restrict weight_c = m_network.feature_weights[c].vals;
#pragma omp simd
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[side][m_ply].vals[i] = m_sides[side][m_ply].vals[i] - weight_a[i] + weight_b[i] + weight_c[i];
        }
    }
};
