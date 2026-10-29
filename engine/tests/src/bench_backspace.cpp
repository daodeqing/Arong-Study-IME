// Reproduce raw-composition Backspace with the same pending-sequence API as
// Server's ApplyCompositionEditKey. Use an isolated copy of real dictionaries.
// CSV rows include request construction separately from full candidate refresh.
#include "core/input_session.h"
#include "core/sentence_association_options.h"
#include "schemes/quanpin_scheme.h"
#include "neural/rescore_worker.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
void sample(metasequoia::InputSession &session, QuanpinScheme &scheme, const char *mode, const std::string &raw,
            int round, size_t input_index)
{
    scheme.set_raw_input(raw, raw);
    const auto request_start = Clock::now();
    const auto request = scheme.build_request();
    const double request_ms = elapsed(request_start);
    const auto start = Clock::now();
    session.set_pinyin_sequence(raw);
    session.set_pinyin_sequence_with_cases(raw);
    session.recompute_candidates();
    const auto &items = session.get_candidates();
    const double total_ms = elapsed(start);
    // A stable full-list fingerprint exposes changes hidden by checking only top1.
    unsigned long long fingerprint = 14695981039346656037ULL;
    for (const auto &item : items)
        for (const auto *field : {&item.pinyin, &item.word})
        {
            for (unsigned char ch : *field)
                fingerprint = (fingerprint ^ ch) * 1099511628211ULL;
            fingerprint = (fingerprint ^ 0xff) * 1099511628211ULL;
        }
    std::printf("%d,%zu,%s,%zu,%.6f,%.6f,%zu,%llu\n", round, input_index, mode, raw.size(), request_ms, total_ms,
                items.size(), fingerprint);
    std::fflush(stdout);
}
} // namespace

int main(int argc, char **argv)
{
    const int repeats = argc > 1 ? (std::max)(1, std::atoi(argv[1])) : 3;
    SentenceAssociationOptions options;
    options.word_lattice = true;
    options.google = true;
    options.neural_keyboard = argc > 2 && std::string(argv[2]) == "neural";
    metasequoia::InputSession session(SchemeType::Quanpin);
    session.set_sentence_association(options);
    QuanpinScheme scheme;
    // Match the engine's default correction mask.
    scheme.set_autocorrect_types(session.quanpin_autocorrect_types());
    const std::vector<std::string> inputs = {
        "nihao",
        "zhongguo",
        "jintiantianqibucuo",
        "zhegewentiyinggaizenmejiejue",
        "womenjintianyiqiqutushuguanxuexizhongwen",
        "ni'hao'zhong'guo",
        "zhognuowoshiaingyao",
    };
    std::puts("round,input,operation,units,request_ms,total_ms,candidates,fingerprint");
    for (int round = 0; round < repeats; ++round)
        for (size_t index = 0; index < inputs.size(); ++index)
        {
            session.reset_state();
            session.reset_cache();
            std::string raw;
            for (char ch : inputs[index])
            {
                raw.push_back(ch);
                sample(session, scheme, "type", raw, round, index);
            }
            while (!raw.empty())
            {
                raw.pop_back();
                sample(session, scheme, "backspace", raw, round, index);
            }
        }
    neural::RescoreWorker::instance().shutdown();
}
