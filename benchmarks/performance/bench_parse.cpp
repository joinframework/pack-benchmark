#include <benchmark/benchmark.h>

#include <sys/resource.h>

#include ADAPTER_HEADER
#include "common/utils.hpp"

static const std::string canada = load_file (DATA_DIR "/canada.msgpack");
static const std::string citm = load_file (DATA_DIR "/citm_catalog.msgpack");
static const std::string twitter = load_file (DATA_DIR "/twitter.msgpack");

static void set_minflt (benchmark::State& state, const rusage& before)
{
    rusage after;
    getrusage (RUSAGE_SELF, &after);
    state.counters["minflt"] = benchmark::Counter (static_cast<double> (after.ru_minflt - before.ru_minflt),
                                                   benchmark::Counter::kAvgIterations);
}

template <typename Adapter>
static void BM_Parse (benchmark::State& state, const std::string* msgpack)
{
    rusage before;
    getrusage (RUSAGE_SELF, &before);
    for (auto _ : state)
    {
        auto doc = Adapter::parse (*msgpack);
        benchmark::DoNotOptimize (doc);
        state.PauseTiming ();
        doc.reset ();
        state.ResumeTiming ();
    }
    set_minflt (state, before);
    state.SetBytesProcessed (state.iterations () * static_cast<int64_t> (msgpack->size ()));
    state.SetLabel (std::string (Adapter::name));
}

#define REGISTER_PARSE(Adapter, tag, file) \
    BENCHMARK_CAPTURE (BM_Parse<Adapter>, tag, &file)->Name ("Parse/" #tag "/" #Adapter)->Unit (benchmark::kMillisecond)

#define X(Adapter)                            \
    REGISTER_PARSE (Adapter, Canada, canada); \
    REGISTER_PARSE (Adapter, Citm, citm);     \
    REGISTER_PARSE (Adapter, Twitter, twitter);

BENCH_ADAPTERS

BENCHMARK_MAIN ();
