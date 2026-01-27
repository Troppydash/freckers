#pragma once
#include <cstdint>
#include <vector>


static thread_local std::mt19937 rng{std::random_device{}()};
static thread_local std::normal_distribution<double> normal(0.0, 1.0);

struct optimizer_param {
    double hidden_value;
    double std;
    double std_nudge = 1;
    int32_t lower;
    int32_t upper;

    bool val_cache = false;
    int32_t val;

    optimizer_param(int32_t value, int32_t lower, int32_t upper, double std)
        : hidden_value((double) value), std(std), lower(lower), upper(upper) {}

    optimizer_param(double hidden_value, int32_t lower, int32_t upper, double std)
        : hidden_value(hidden_value), std(std), lower(lower), upper(upper) {}

    int32_t get_value() {
        if (val_cache)
            return val;

        double value = normal(rng) * std + hidden_value;
        if (value <= lower)
            value = lower;
        else if (value >= upper)
            value = upper;

        val_cache = true;
        return val = static_cast<int32_t>(round(value));
    }

    optimizer_param nudge() const {
        double amount = normal(rng) * std_nudge;
        if (hidden_value <= lower) {
            return {hidden_value + amount, lower, upper, std};
        }
        if (hidden_value + amount >= upper) {
            return {hidden_value + amount, lower, upper, std};
        }

        return {hidden_value + amount, lower, upper, std};
    }

    optimizer_param update(double grad, double lr) const {
        double new_value = static_cast<double>(hidden_value) - lr * grad;
        if (new_value >= upper) {
            new_value = upper;
        } else if (new_value <= lower) {
            new_value = lower;
        }

        return {new_value, lower, upper, std};
    }
};

struct optimizer {
    double m_lr = 0.5;

    // current params
    std::vector<optimizer_param> m_params{};

    // target boards
    struct entry {
        board::pos pos;
        double score;
    };
    std::vector<entry> m_targets;


    void load_targets(const std::string &path) {
        std::ifstream infile{path};
        if (!infile) {
            std::cerr << "Error opening file\n";
            return;
        }

        std::string length;
        infile >> length;

        size_t lines = std::atoll(length.c_str());

        m_targets.clear();
        uint64_t lily, red, blue;
        int turn, moves;
        double value;
        for (int i = 0; i < lines; ++i) {
            infile >> lily >> red >> blue >> turn >> moves >> value;

            m_targets.push_back({board::pos{lily, red, blue, turn, moves},
                                 value});
        }
    }

    void load_config(engine::computer_config &config) {
        m_params.clear();
        for (auto c: config.m_lmp_margins)
            m_params.push_back({
                    c,
                    0,
                    40,
                    0.2,
            });

        m_params.push_back({config.m_lmr_depth, 1, 10, 0.2});
        m_params.push_back({config.m_lmr_move, 1, 10, 0.2});
        m_params.push_back({config.m_tempo, 0, 200, 0.2});
        m_params.push_back({config.m_static_null_move_margin, 100, 700, 0.2});
        m_params.push_back({config.m_countermove, 0, 20, 0.2});
        m_params.push_back({config.m_lily_min, 0, 10, 0.2});
        m_params.push_back({config.m_lily_scale, 0, 20, 0.2});
        m_params.push_back({config.m_window, 1, 10, 0.2});
        m_params.push_back({config.m_window_scale, 2, 10, 0.2});

        for (auto c: config.m_fut_margins) {
            m_params.push_back({c, 0, 700, 0.2});
        }
    }

    engine::computer_config to_config(std::vector<optimizer_param> &params) {
        engine::computer_config config;

        int i = 0;
        for (; i < config.m_lmp_margins.size(); ++i)
            config.m_lmp_margins[i] = params[i].get_value();

        config.m_lmr_depth = params[i++].get_value();
        config.m_lmr_move = params[i++].get_value();
        config.m_tempo = params[i++].get_value();
        config.m_static_null_move_margin = params[i++].get_value();
        config.m_countermove = params[i++].get_value();
        config.m_lily_min = params[i++].get_value();
        config.m_lily_scale = params[i++].get_value();
        config.m_window = params[i++].get_value();
        config.m_window_scale = params[i++].get_value();

        for (; i < config.m_fut_margins.size(); ++i)
            config.m_fut_margins[i] = params[i].get_value();

        return config;
    }


    constexpr static double ERROR = 543534534;

    double compute_y(const board::pos &position, std::vector<optimizer_param> &x) {
        int max_depth = 7;

        board::pos dup_pos{position};
        int score = 0;
        static engine::lazysmp lazysmp{1, 4};
        lazysmp.m_tt = engine::table{4};
        lazysmp.search_one(dup_pos, 100, &score, {"../weights/nnue.bin"}, to_config(x), false, max_depth);
        if (score == 0) {
            return ERROR;
        }

        double score_d = score;
        return score_d;
    }

    void display_params(std::vector<optimizer_param> &params) {
        for (int i = 0; i < params.size(); ++i) {
            printf("%5.2lf|", params[i].hidden_value);
        }
    }

    void optimize(int epochs = 5) {
        // gradient descent

        for (int epoch = 0; epoch < epochs; ++epoch) {
            std::cout << "epoch " << epoch << "\n";

            // sgd
            for (int t = 0; t < m_targets.size(); ++t) {
                printf("\r%5d of %lu,", t, m_targets.size());
                display_params(m_params);
                fflush(stdout);

                auto &entry = m_targets[t];

                double y = compute_y(entry.pos, m_params);

                // for each parameter, nudge
                for (int i = 0; i < m_params.size(); ++i) {
                    std::vector<optimizer_param> x_tick;
                    for (int j = 0; j < m_params.size(); ++j) {
                        if (i == j) {
                            x_tick.push_back(m_params[j].nudge());
                        } else {
                            x_tick.push_back(m_params[j]);
                        }
                    }

                    if (m_params[i].get_value() == x_tick[i].get_value())
                        continue;

                    double y_tick = compute_y(entry.pos, x_tick);
                    if (y_tick == ERROR) {
                        std::cout << "\nerror compute ";
                        display_params(x_tick);
                        std::cout << std::endl;
                        continue;
                    }


                    // gradient update
                    double scale = 40 * 100;
                    double header = -2.0 * (entry.score - tanh(y / scale)) * (1 - tanh(y / scale) * tanh(y / scale)) / scale;
                    double grad = header * (y_tick - y) / (x_tick[i].get_value() - m_params[i].get_value());
                    // ignore large gradients
                    if (fabsl(grad) >= 5.0) {
                        std::cout << "\nerror gradient ";
                        display_params(x_tick);
                        std::cout << std::endl;
                        continue;
                    }
                    m_params[i] = m_params[i].update(grad, m_lr);
                }
            }
        }
    }
};
