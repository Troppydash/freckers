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
                std::clamp((value - lower) * 10.0 / (upper - lower), 0.0, 10.0),
                lower,
                upper};
    }

    // ones that the engine sees
    int32_t get_value() const {
        return std::clamp(static_cast<int32_t>(round(hidden_value / scaler + lower)), lower, upper);
    }
};

namespace param_helpers {
    inline void append(std::vector<optimizer_param> &params, int32_t value, int32_t lower, int32_t upper) {
        params.push_back(optimizer_param::load_scaled(value, lower, upper));
    }

    template<int T>
    inline void append_list(std::vector<optimizer_param> &params, std::array<int32_t, T> &values, int32_t lower, int32_t upper) {
        for (auto &v: values) {
            append(params, v, lower, upper);
        }
    }

    template<int T>
    inline void append_incr_list(std::vector<optimizer_param> &params, std::array<int32_t, T> &values, int32_t lower, int32_t upper) {
        for (int i = 1; i < T; ++i) {
            append(params, values[i] - values[i - 1], lower, upper);
        }
    }

    inline void parse(int32_t &target, const std::vector<optimizer_param> &params, int &i) {
        target = params[i].get_value();
        i += 1;
    }

    template<int T>
    inline void parse_list(std::array<int32_t, T> &target, const std::vector<optimizer_param> &params, int &i, bool zero_centered = true) {
        for (int j = 0; j < target.size(); ++j) {
            parse(target[j], params, i);

            if (zero_centered) {
                if (j == 0) {
                    target[j] = 0;
                } else {
                    target[j] = std::max(target[j], target[j - 1]);
                }
            }
        }
    }

    template<int T>
    inline void parse_incr_list(std::array<int32_t, T> &target, const std::vector<optimizer_param> &params, int &i) {
        target[0] = 0;
        for (int j = 1; j < T; ++j) {
            parse(target[j], params, i);
            target[j] += target[j - 1];
        }
    }
}// namespace param_helpers

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

    void load_config(engine::computer_config &config, bool reset = false) {
        m_params.clear();

        using namespace param_helpers;

        append_incr_list<6>(m_params, config.m_lmp_margins, 0, 15);
        // append_list<6>(m_params, config.m_lmp_margins, 0, 45);
        append(m_params, config.m_lmr_depth, 1, 10);
        append(m_params, config.m_lmr_move, 1, 20);
        append(m_params, config.m_tempo, 1, 200);
        append(m_params, config.m_static_null_move_margin, 100, 1200);
        append(m_params, config.m_countermove, 0, 100);
        append(m_params, config.m_lily_min, 0, 10);

        append_incr_list<8>(m_params, config.m_lily_range, 0, 700);
        // append_list<6>(m_params, config.m_lily_range, 0, 1000);
        append_incr_list<7>(m_params, config.m_fut_margins, 0, 500);
        // append_list<7>(m_params, config.m_fut_margins, 0, 1000);
        append(m_params, config.m_razor_mult, 1, 6);
        append(m_params, config.m_razor_limit, 2, 6);
        append(m_params, config.m_nmr_const, 1, 6);
        append(m_params, config.m_nmr_depth, 1, 14);
        append(m_params, config.m_tempo_limit, 10, 1500);
        append(m_params, config.m_long_jump_end_move, 1, 80);
        append(m_params, config.m_short_jump_end_move, 1, 80);
        append(m_params, config.m_long_jump_vgap_mult, 0, 4);
        append(m_params, config.m_long_jump_hgap_mult, 0, 4);
        append(m_params, config.m_jump_endgame, 1, 8);

        append(m_params, config.m_nmr_min_depth, 1, 4);
        append(m_params, config.m_nmr_beta_mult, 0, 200);
        append(m_params, config.m_iid_depth, 4, 6);
        append(m_params, config.m_iid_depth_reduction, 2, 6);
        append(m_params, config.m_tt_eval_prop, 0, 100);

        if (reset) {
            for (auto &c: m_params) {
                c.hidden_value = 0.5;
            }
        }
    }

    engine::computer_config to_config(std::vector<optimizer_param> &params) {
        engine::computer_config config;

        int i = 0;

        using namespace param_helpers;

        parse_incr_list<6>(config.m_lmp_margins, params, i);
        // parse_list<6>(config.m_lmp_margins, params, i);
        parse(config.m_lmr_depth, params, i);
        parse(config.m_lmr_move, params, i);
        parse(config.m_tempo, params, i);
        parse(config.m_static_null_move_margin, params, i);
        parse(config.m_countermove, params, i);
        parse(config.m_lily_min, params, i);

        parse_incr_list<8>(config.m_lily_range, params, i);
        // parse_list<6>(config.m_lily_range, params, i);
        parse_incr_list<7>(config.m_fut_margins, params, i);
        // parse_list<7>(config.m_fut_margins, params, i);
        parse(config.m_razor_mult, params, i);
        parse(config.m_razor_limit, params, i);
        parse(config.m_nmr_const, params, i);
        parse(config.m_nmr_depth, params, i);
        parse(config.m_tempo_limit, params, i);
        parse(config.m_long_jump_end_move, params, i);
        parse(config.m_short_jump_end_move, params, i);
        parse(config.m_long_jump_vgap_mult, params, i);
        parse(config.m_long_jump_hgap_mult, params, i);
        parse(config.m_jump_endgame, params, i);

        parse(config.m_nmr_min_depth, params, i);
        parse(config.m_nmr_beta_mult, params, i);
        parse(config.m_iid_depth, params, i);
        parse(config.m_iid_depth_reduction, params, i);
        parse(config.m_tt_eval_prop, params, i);


        if (i != params.size()) {
            std::cerr << i << "," << params.size();
            throw std::runtime_error("mismatched param size when parsing");
        }

        return config;
    }


    std::pair<int, bool> compute_y(const board::pos &position, std::vector<optimizer_param> &x) {
        // required to search to *max_depth* in under *max_ts* ms, else fails entire eval
        int max_depth = 8;
        int max_ts = 300;

        board::pos dup_pos{position};
        int score = 0;
        int reached_depth = 0;
        engine::lazysmp lazysmp{1, 8};
        lazysmp.search_one(dup_pos, max_ts, &score, {"../weights/nnue.bin"}, to_config(x), false, &reached_depth, max_depth);

        if (reached_depth != max_depth) {
            std::cout << "[warning tle 0]\n";

            // re-search
            board::pos dup_pos{position};
            score = 0;
            reached_depth = 0;
            engine::lazysmp lazysmp{1, 8};
            lazysmp.search_one(dup_pos, max_ts, &score, {"../weights/nnue.bin"}, to_config(x), false, &reached_depth, max_depth);

            if (reached_depth != max_depth) {
                return {score, false};
            }
        }

        return {score, true};
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


    void optimize() {

        std::vector<double>
                initial(m_params.size());
        std::vector<double> lowerbounds(m_params.size());
        std::vector<double> upperbounds(m_params.size());

        for (int i = 0; i < m_params.size(); ++i) {
            initial[i] = m_params[i].hidden_value;
            lowerbounds[i] = 0;
            upperbounds[i] = 10;
        }

        double sigma = 1.5;
        GenoPheno<pwqBoundStrategy> gp(&lowerbounds[0], &upperbounds[0], m_params.size());
        CMAParameters<GenoPheno<pwqBoundStrategy>> cmaparams(initial, sigma, 20, 42, gp);
        cmaparams.set_algo(aCMAES);
        cmaparams.set_mt_feval(true);
        cmaparams.set_max_fevals(1000);

        FitFunc evaluation = [&](const double *params, const int N) {
            double total = 0;
            std::vector<optimizer_param> p;
            for (int i = 0; i < N; ++i) {
                p.push_back(optimizer_param::load_hidden(params[i], m_params[i]));
            }
            int count = 0;
            for (int t = 0; t < m_targets.size(); t += 1) {
                auto [score, ok] = compute_y(m_targets[t].pos, p);
                if (!ok) {
                    std::cout << "[warning tle]\n";
                    return 1e9;
                }

                double wdl = 0;
                if (score >= param::checkmate)
                    wdl = 1;
                else if (score <= -param::checkmate)
                    wdl = -1;
                else
                    wdl = tanh(static_cast<double>(score) / 4000.0);

                double diff = (wdl - m_targets[t].score);
                total += std::pow(std::abs(diff), 2.6);
                // total += (wdl - m_targets[t].score) * (wdl - m_targets[t].score);
                count += 1;
            }
            total /= count;

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
                      << " edm " << cmasols.edm()
                      << " sigma " << cmasols.sigma()
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
    }
};
