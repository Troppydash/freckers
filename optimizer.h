#pragma once
#include <cstdint>
#include <vector>

#include <libcmaes/cmaes.h>
using namespace libcmaes;

// target is 0-10 range, 3 sigma
struct optimizer_param {
    // ones that the tuner sees
    double scaler;
    double hidden_value;
    int32_t lower;
    int32_t upper;

    // optimizer_param(double value, int32_t lower, int32_t upper)
    //     : scaler(10.0 / (upper - lower)), hidden_value((value - lower) * scaler), lower(lower), upper(upper) {}
    //
    // optimizer_param(int32_t value, int32_t lower, int32_t upper)
    //     : optimizer_param(static_cast<double>(value), lower, upper) {}

    optimizer_param(double scaler, double hidden_value, int32_t lower, int32_t upper)
        : scaler(scaler), hidden_value(hidden_value), lower(lower), upper(upper) {}

    static optimizer_param load_hidden(double hidden, const optimizer_param &src) {
        return optimizer_param{src.scaler, hidden, src.lower, src.upper};
    }

    static optimizer_param load_scaled(double value, int32_t lower, int32_t upper) {
        return optimizer_param{
                10.0 / (upper - lower),
                (value - lower) * 10.0 / (upper - lower),
                lower,
                upper};
    }

    // ones that the engine sees
    int32_t get_value() const {
        return static_cast<int32_t>(round(hidden_value / scaler + lower));
    }
};

struct optimizer {
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
            m_params.push_back(optimizer_param::load_scaled(c,
                                                            0,
                                                            25));

        m_params.push_back(optimizer_param::load_scaled(config.m_lmr_depth, 1, 10));
        m_params.push_back(optimizer_param::load_scaled(config.m_lmr_move, 1, 10));
        m_params.push_back(optimizer_param::load_scaled(config.m_tempo, 1, 200));
        m_params.push_back(optimizer_param::load_scaled(config.m_static_null_move_margin, 100, 700));
        m_params.push_back(optimizer_param::load_scaled(config.m_countermove, 0, 20));
        m_params.push_back(optimizer_param::load_scaled(config.m_lily_min, 1, 10));
        m_params.push_back(optimizer_param::load_scaled(config.m_lily_scale, 1, 20));
        m_params.push_back(optimizer_param::load_scaled(config.m_window, 100, 1000));
        m_params.push_back(optimizer_param::load_scaled(config.m_window_scale, 2, 10));

        for (auto c: config.m_fut_margins) {
            m_params.push_back(optimizer_param::load_scaled(c, 0, 600));
        }
    }

    engine::computer_config to_config(std::vector<optimizer_param> &params) {
        engine::computer_config config;

        int i = 0;
        for (; i < config.m_lmp_margins.size(); ++i) {
            config.m_lmp_margins[i] = params[i].get_value();
            if (i == 0)
                config.m_lmp_margins[0] = 0;
            else
                config.m_lmp_margins[i] = std::max(config.m_lmp_margins[i], config.m_lmp_margins[i - 1]);
        }

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

            if (j == 0)
                config.m_fut_margins[j] = 0;
            else
                config.m_fut_margins[j] = std::max(config.m_fut_margins[j], config.m_fut_margins[j - 1]);

            i++;
        }


        return config;
    }


    int compute_y(const board::pos &position, std::vector<optimizer_param> &x) {
        int max_depth = 7;

        board::pos dup_pos{position};
        int score = 0;
        int reached_depth = 0;
        engine::lazysmp lazysmp{1, 8};
        lazysmp.search_one(dup_pos, 200, &score, {"../weights/nnue.bin"}, to_config(x), false, &reached_depth, max_depth);

        if (reached_depth != max_depth) {
            return -8008123;
        }

        return score;
    }

    void display_params(std::vector<optimizer_param> &params) {
        for (int i = 0; i < params.size(); ++i) {
            printf("%5.2lf|", params[i].hidden_value);
        }
        printf("\n");

        to_config(params).display();
        printf("\n");
    }

    void display_params(std::vector<double> &params) {
        for (int i = 0; i < params.size(); ++i) {
            printf("%5.2lf|", params[i]);
        }
        printf("\n");

        std::vector<optimizer_param> parameters;
        for (int i = 0; i < params.size(); ++i) {
            optimizer_param p = optimizer_param::load_hidden(params[i], m_params[i]);
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

        for (int i = 0; i < m_params.size(); ++i) {
            initial[i] = m_params[i].hidden_value;
            lowerbounds[i] = 0;
            upperbounds[i] = 10;
        }

        GenoPheno<pwqBoundStrategy> gp(&lowerbounds[0], &upperbounds[0], m_params.size());
        CMAParameters<GenoPheno<pwqBoundStrategy>> cmaparams(initial, 1, -1, 42, gp);
        cmaparams.set_algo(sepaCMAES);
        cmaparams.set_mt_feval(true);
        cmaparams.set_max_fevals(1000);

        FitFunc evaluation = [&](const double *params, const int N) {
            double total = 0;
            std::vector<optimizer_param> p;
            for (int i = 0; i < N; ++i) {
                p.push_back(optimizer_param::load_hidden(params[i], m_params[i]));
            }
            int count = 0;
            for (int t = 0; t < m_targets.size(); t += 20) {
                int score = compute_y(m_targets[t].pos, p);
                if (score == -8008123) {
                    std::cout << "[skipped]\n";
                    return 1e9;
                }
                // if (score == ERROR) {
                //     std::cout << "[error::start] compute\n";
                //     display_params(p);
                //     std::cout << "[error::end] compute\n";
                //
                //     return 1e9;
                // }

                double wdl = 0;
                if (score >= param::checkmate)
                    wdl = 1;
                else if (score <= -param::checkmate)
                    wdl = -1;
                else
                    wdl = tanh(static_cast<double>(score) / 4000.0);

                total += (wdl - m_targets[t].score) * (wdl - m_targets[t].score);
                count += 1;
            }
            total /= count;

            // std::cout << "[eval]" << " score " << total << "\n";
            // display_params(p);

            return total;
        };

        ProgressFunc<CMAParameters<GenoPheno<pwqBoundStrategy>>, CMASolutions> progress_func =
                [&](
                        const CMAParameters<GenoPheno<pwqBoundStrategy>> &cmaparams,
                        const CMASolutions &cmasols) -> int {
            auto can = cmasols.get_best_seen_candidate();
            std::cout << "[prog::start]\n"
                      << "loss " << can.get_fvalue()
                      << " candidates " << cmasols.size()
                      << " fevals " << cmasols.fevals()
                      << " niters " << cmasols.niter()
                      << " time " << cmasols.elapsed_time()
                      << "\n";

            if (std::isnan(can.get_fvalue())) {
                std::cout << "[prog::end]\n";
                return 0;
            }

            auto x = gp.pheno(can.get_x_dvec());
            std::vector<double> p{x.begin(), x.end()};
            display_params(p);

            std::cout << "[prog::end]\n";

            return 0;
        };


        CMASolutions cmasols = cmaes<GenoPheno<pwqBoundStrategy>>(
                evaluation, cmaparams, progress_func);
        std::cout << "best solution: " << cmasols << std::endl;
        std::cout << "optimization took " << cmasols.elapsed_time() / 1000.0 << " seconds\n";
        std::cout << cmasols.run_status();// the optimization status, failed if < 0


        // baseline = 0.364256
        // first 0.362329
        // 0.35851, 0.005

        // baseline = 0.349008, target = 0.34326
        // baseline = 0.347841

        // 0.346912
    }
};
