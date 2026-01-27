#pragma once
#include <cstdint>
#include <vector>

#include <libcmaes/cmaes.h>
using namespace libcmaes;

static thread_local std::mt19937 rng{std::random_device{}()};
static thread_local std::normal_distribution<double> normal(0.0, 1.0);

struct optimizer_param {
    double hidden_value;
    int32_t lower;
    int32_t upper;
    double scaler;

    optimizer_param(double value, int32_t lower, int32_t upper, double scaler)
        : hidden_value(value), lower(lower), upper(upper), scaler(scaler) {}

    int32_t get_value() {
        return static_cast<int32_t>(round(hidden_value));
    }

    // optimizer_param nudge() const {
    //     double amount = normal(rng) * std_nudge;
    //     if (hidden_value <= lower) {
    //         return {hidden_value + amount, lower, upper, std};
    //     }
    //     if (hidden_value + amount >= upper) {
    //         return {hidden_value + amount, lower, upper, std};
    //     }
    //
    //     return {hidden_value + amount, lower, upper, std};
    // }
    //
    // optimizer_param update(double grad, double lr) const {
    //     double new_value = static_cast<double>(hidden_value) - lr * grad;
    //     if (new_value >= upper) {
    //         new_value = upper;
    //     } else if (new_value <= lower) {
    //         new_value = lower;
    //     }
    //
    //     return {new_value, lower, upper, std};
    // }
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
                    25,
            });

        m_params.push_back({config.m_lmr_depth, 1, 10});
        m_params.push_back({config.m_lmr_move, 1, 10});
        m_params.push_back({config.m_tempo, 0, 200});
        m_params.push_back({config.m_static_null_move_margin, 100, 700});
        m_params.push_back({config.m_countermove, 0, 20});
        m_params.push_back({config.m_lily_min, 0, 10});
        m_params.push_back({config.m_lily_scale, 0, 20});
        m_params.push_back({config.m_window, 1, 10});
        m_params.push_back({config.m_window_scale, 2, 10});

        for (auto c: config.m_fut_margins) {
            m_params.push_back({c, 0, 700});
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

        for (int j = 0; j < config.m_fut_margins.size(); ++j) {
            config.m_fut_margins[j] = params[i].get_value();
            i++;
        }

        return config;
    }


    constexpr static double ERROR = 543534534;

    double compute_y(const board::pos &position, std::vector<optimizer_param> &x) {
        int max_depth = 9;

        board::pos dup_pos{position};
        int score = 0;
        engine::lazysmp lazysmp{1, 16};
        lazysmp.search_one(dup_pos, 200, &score, {"../weights/nnue.bin"}, to_config(x), false, max_depth);
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
        printf("\n");
    }

    void display_params(std::vector<double> &params) {
        for (int i = 0; i < params.size(); ++i) {
            printf("%5.2lf|", params[i]);
        }
        printf("\n");

        std::vector<optimizer_param> parameters;
        for (int i = 0; i < params.size(); ++i) {
            optimizer_param p = {params[i], m_params[i].lower, m_params[i].upper};
            parameters.push_back(p);
            // printf("%5d|", p.get_value());
        }

        to_config(parameters).display();
        printf("\n");
    }


    void optimize(int epochs = 5) {

        std::vector<double>
                initial(m_params.size());
        std::vector<double> lowerbounds(m_params.size());
        std::vector<double> upperbounds(m_params.size());
        std::vector<double> sigmas(m_params.size());

        for (int i = 0; i < m_params.size(); ++i) {
            initial[i] = m_params[i].hidden_value;
            lowerbounds[i] = m_params[i].lower;
            upperbounds[i] = m_params[i].upper;
            sigmas[i] = (upperbounds[i] - lowerbounds[i]) / 2;
        }

        GenoPheno<pwqBoundStrategy> gp(&lowerbounds[0], &upperbounds[0], m_params.size());
        // CMAParameters<GenoPheno<>> cmaparams(initial, sigmas, -1, lowerbounds, upperbounds);
        CMAParameters<GenoPheno<pwqBoundStrategy>> cmaparams(initial, 1, -1, 42, gp);
        cmaparams.set_algo(aCMAES);
        cmaparams.set_mt_feval(true);

        FitFunc evaluation = [&](const double *params, const int N) {
            double total = 0;
            std::vector<optimizer_param> p;
            for (int i = 0; i < N; ++i) {
                p.push_back({params[i], m_params[i].lower, m_params[i].upper});
            }
            int count = 0;
            for (int t = 0; t < m_targets.size(); t += 5) {
                double score = compute_y(m_targets[t].pos, p);
                if (score == ERROR)
                    return 1e9;
                total += (tanh(score / 4000) - m_targets[t].score) * (tanh(score / 4000) - m_targets[t].score);
                count += 1;
            }
            total /= count;

            std::cout << "[eval]" << " score " << total << "\n";
            display_params(p);

            return total;
        };

        ProgressFunc<CMAParameters<GenoPheno<pwqBoundStrategy>>, CMASolutions> progress_func =
                [&](
                        const CMAParameters<GenoPheno<pwqBoundStrategy>> &cmaparams,
                        const CMASolutions &cmasols) -> int {
            auto can = cmasols.get_best_seen_candidate();
            std::cout << "[prog]"
                      << " error " << can.get_fvalue()
                      << " candidates " << cmasols.size()
                      << " fevals " << cmasols.fevals()
                      << " niters " << cmasols.niter()
                      << " time " << cmasols.elapsed_time()
                      << "\n";

            if (std::isnan(can.get_fvalue()))
                return 0;

            auto x = gp.pheno(can.get_x_dvec());
            // auto x = can.get_x_dvec();
            std::vector<double> p{x.begin(), x.end()};
            display_params(p);

            return 0;
        };

        CMASolutions cmasols = cmaes<GenoPheno<pwqBoundStrategy>>(
                evaluation, cmaparams, progress_func);
        std::cout << "best solution: " << cmasols << std::endl;
        std::cout << "optimization took " << cmasols.elapsed_time() / 1000.0 << " seconds\n";
        std::cout << cmasols.run_status();// the optimization status, failed if < 0


        // baseline = 0.364256
        // first 0.362329
        // 0.35851
    }


    // for (int epoch = 0; epoch < epochs; ++epoch) {
    //     std::cout << "epoch " << epoch << "\n";
    //
    //     // sgd
    //     for (int t = 0; t < m_targets.size(); ++t) {
    //         printf("\r%5d of %lu,", t, m_targets.size());
    //         display_params(m_params);
    //         fflush(stdout);
    //
    //         auto &entry = m_targets[t];
    //
    //         double y = compute_y(entry.pos, m_params);
    //
    //         // for each parameter, nudge
    //         for (int i = 0; i < m_params.size(); ++i) {
    //             std::vector<optimizer_param> x_tick;
    //             for (int j = 0; j < m_params.size(); ++j) {
    //                 if (i == j) {
    //                     x_tick.push_back(m_params[j].nudge());
    //                 } else {
    //                     x_tick.push_back(m_params[j]);
    //                 }
    //             }
    //
    //             if (m_params[i].get_value() == x_tick[i].get_value())
    //                 continue;
    //
    //             double y_tick = compute_y(entry.pos, x_tick);
    //             if (y_tick == ERROR) {
    //                 std::cout << "\nerror compute ";
    //                 display_params(x_tick);
    //                 std::cout << std::endl;
    //                 continue;
    //             }
    //
    //
    //             // gradient update
    //             double scale = 40 * 100;
    //             double header = -2.0 * (entry.score - tanh(y / scale)) * (1 - tanh(y / scale) * tanh(y / scale)) / scale;
    //             double grad = header * (y_tick - y) / (x_tick[i].get_value() - m_params[i].get_value());
    //             // ignore large gradients
    //             if (fabsl(grad) >= 5.0) {
    //                 std::cout << "\nerror gradient ";
    //                 display_params(x_tick);
    //                 std::cout << std::endl;
    //                 continue;
    //             }
    //             m_params[i] = m_params[i].update(grad, m_lr);
    //         }
    //     }
    // }
};
