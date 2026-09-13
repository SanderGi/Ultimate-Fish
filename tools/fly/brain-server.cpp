// Batched full-circuit inference server. GPL-3.0-or-later.
// brain.cpp remains the scalar reference used for training and recordings.
#include "brain.cpp"
#include <array>

static std::vector<uint32_t> incomingOffsets, sources;
static std::vector<float> incomingWeights;

static void prepareIncoming() {
    if (!incomingOffsets.empty()) return;
    incomingOffsets.assign(count + 1, 0);
    for (auto target : targets) ++incomingOffsets[target + 1];
    for (uint32_t i = 0; i < count; ++i)
        incomingOffsets[i + 1] += incomingOffsets[i];
    auto cursor = incomingOffsets;
    sources.resize(targets.size());
    incomingWeights.resize(weights.size());
    // Stable transpose: every destination receives contributions in exactly
    // the source/edge order of brain_encode, including parallel edges.
    for (uint32_t a = 0; a < count; ++a)
        for (uint32_t e = offsets[a]; e < offsets[a + 1]; ++e) {
            const auto slot = cursor[targets[e]]++;
            sources[slot] = a;
            incomingWeights[slot] = weights[e];
        }
}

using Features = std::array<float, 8>;
using MotorRows = std::vector<std::vector<float>>;

template<size_t Lanes>
static void encodeBlock(const std::vector<Features>& rows, size_t begin,
                        MotorRows& result) {
    using Lane = std::array<float, Lanes>;
    const size_t size = std::min(Lanes, rows.size() - begin);
    const bool record = begin + size == rows.size();
    std::vector<Lane> rates(count), stimulus(count), signals(count);
    for (size_t i = 0; i < inputs.size(); ++i)
        for (size_t k = 0; k < size; ++k)
            stimulus[inputs[i]][k] = channels[i] == 0 ? rows[begin + k][0]
                : 2 * rows[begin + k][channels[i]] - 1;
    if (record) frames.clear();
    for (int t = 0; t < 8; ++t) {
        // Separate previous-step signals preserve synchronous updates while
        // each destination's accumulator stays local. SIMD lanes are distinct
        // candidates, never a reordered reduction over a neuron's synapses.
        for (uint32_t a = 0; a < count; ++a)
            for (size_t k = 0; k < Lanes; ++k)
                signals[a][k] = 1.4f * rates[a][k];
        for (uint32_t a = 0; a < count; ++a) {
            Lane sum = stimulus[a];
            // At step zero every signal is exactly zero in the scalar circuit.
            if (t) for (uint32_t e = incomingOffsets[a]; e < incomingOffsets[a + 1]; ++e) {
                const auto& signal = signals[sources[e]];
                const float weight = incomingWeights[e];
                for (size_t k = 0; k < Lanes; ++k)
                    if (signal[k] != 0) sum[k] += weight * signal[k];
            }
            for (size_t k = 0; k < Lanes; ++k)
                rates[a][k] = 0.3f * rates[a][k]
                    + 0.7f * sum[k] / (1.0f + std::abs(sum[k]));
        }
        if (record) {
            std::vector<int> frame;
            frame.reserve((count + 7) / 8);
            for (uint32_t a = 0; a < count; a += 8)
                frame.push_back(int(std::abs(rates[a][size - 1]) * 255));
            frames.push_back(std::move(frame));
        }
    }
    for (size_t k = 0; k < size; ++k)
        for (size_t i = 0; i < outputs.size(); ++i)
            result[begin + k][i] = rates[outputs[i]][k];
    if (record) {
        for (uint32_t a = 0; a < count; ++a) activity[a] = rates[a][size - 1];
        readout = result.back();
    }
}

static MotorRows encodeRows(const std::vector<Features>& rows) {
    MotorRows result(rows.size(), std::vector<float>(outputs.size()));
    size_t begin = 0;
    while (begin < rows.size()) {
        const auto remaining = rows.size() - begin;
        if (remaining < 4) {
            // Small tails and telemetry requests use the unchanged reference.
            brain_encode(rows[begin].data());
            result[begin++] = readout;
        } else {
            prepareIncoming();
            if (remaining <= 4) {
                encodeBlock<4>(rows, begin, result);
                begin += 4;
            } else {
                encodeBlock<8>(rows, begin, result);
                begin += std::min(size_t(8), remaining);
            }
        }
    }
    return result;
}

int main(int argc, char** argv) {
    if (argc != 2 || brain_load(argv[1]) < 0) return 1;
    std::cout << std::setprecision(9) << "ready " << outputs.size() << std::endl;
    int n;
    while (std::cin >> n) {
        if (n == 0) {
            std::cout << "{\"frames\":[";
            for (size_t t = 0; t < frames.size(); ++t) {
                if (t) std::cout << ',';
                std::cout << '[';
                for (size_t i = 0; i < frames[t].size(); ++i) {
                    if (i) std::cout << ',';
                    std::cout << frames[t][i];
                }
                std::cout << ']';
            }
            int active = 0;
            for (float v : activity) if (std::abs(v) > 0.02f) ++active;
            std::cout << "],\"active\":" << active << "}" << std::endl;
            continue;
        }
        if (n < 1 || n > 256) return 2;
        std::vector<Features> rows(n);
        for (auto& f : rows) for (auto& v : f)
            if (!(std::cin >> v) || !std::isfinite(v) || v < -1 || v > 1) return 3;
        const auto result = encodeRows(rows);
        std::cout << '[';
        for (int j = 0; j < n; ++j) {
            if (j) std::cout << ',';
            std::cout << '[';
            for (size_t k = 0; k < outputs.size(); ++k) {
                if (k) std::cout << ',';
                std::cout << result[j][k];
            }
            std::cout << ']';
        }
        std::cout << ']' << std::endl;
    }
    return 0;
}
