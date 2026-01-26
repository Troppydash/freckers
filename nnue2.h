#pragma once

#include "board.h"
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
    accumulator feature_weights[64 * 2];
    // QA quant, hidden_size -> hidden_size
    accumulator feature_bias;

    // QB quant, 2*hidden_size -> 1
    int16_t output_weights[2 * HIDDEN_SIZE];
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
    accumulator m_sides[2]{};

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

        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[0].vals[i] = m_sides[1].vals[i] = m_network.feature_bias.vals[i];
        }

        // red pieces
        board::mask m = position.m_players[board::RED];
        while (m > 0) {
            int i = __builtin_ctzll(m);
            m -= (1ull << i);

            add_feature(board::RED, i);
        }

        // blue pieces
        m = position.m_players[board::BLUE];
        while (m > 0) {
            int i = __builtin_ctzll(m);
            m -= (1ull << i);

            add_feature(board::BLUE, i ^ 56);
        }

        // lily
        m = position.m_lilypads;
        while (m > 0) {
            int i = __builtin_ctzll(m);
            m -= (1ull << i);

            add_feature(board::RED, 64 + i);
            add_feature(board::BLUE, 64 + (i ^ 56));
        }
    }

    int32_t evaluate(int side2move) const {
        const accumulator &us = m_sides[side2move];
        const accumulator &them = m_sides[side2move ^ 1];

        int32_t output = 0;

        // side2move -> output, output in QA * QA * QB
        // not-side2move -> output, output in QA * QA * QB
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            output += screlu(us.vals[i]) * m_network.output_weights[i];
            output += screlu(them.vals[i]) * m_network.output_weights[HIDDEN_SIZE + i];
        }

        // output in QA * QB
        output /= static_cast<int32_t>(QA);

        output += static_cast<int32_t>(m_network.output_bias);

        // output in [-SCALE, SCALE]
        output *= SCALE;
        output /= static_cast<int32_t>(QA) * static_cast<int32_t>(QB);

        return std::clamp(output, -SCALE, SCALE);
    }


    void add_feature(int side, int feature_idx) {
        const int16_t *__restrict weight = m_network.feature_weights[feature_idx].vals;
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[side].vals[i] += weight[i];
        }
    }

    void remove_feature(int side, int feature_idx) {
        const int16_t *__restrict weight = m_network.feature_weights[feature_idx].vals;
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[side].vals[i] -= weight[i];
        }
    }

    void move_feature(int side, int a, int b) {
        const int16_t *__restrict weight_a = m_network.feature_weights[a].vals;
        const int16_t *__restrict weight_b = m_network.feature_weights[b].vals;
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            m_sides[side].vals[i] = m_sides[side].vals[i] - weight_a[i] + weight_b[i];
        }
    }
};
