// Bit-for-bit regression against the training circuit, including batch tails.
// GPL-3.0-or-later. Build: c++ -std=c++17 -O3 tests/fly_brain_parity.cpp -o /tmp/fly-parity
#define main fly_server_main
#include "../tools/fly/brain-server.cpp"
#undef main
#include <cstring>
#include <random>

int main(int argc, char** argv) {
    if (argc < 2 || brain_load(argv[1]) < 0) return 1;
    std::mt19937 rng(20260913);
    std::vector<Features> inputsToTest(17);
    for (auto& f : inputsToTest) {
        for (auto& v : f) v = float(rng() % 1000001) / 1000000;
        f[0] = 2 * f[0] - 1;
    }
    inputsToTest[0].fill(0);
    inputsToTest[1].fill(1);
    inputsToTest[2].fill(0); inputsToTest[2][0] = -1;
    inputsToTest[3].fill(-0.0f);
    MotorRows expected;
    std::vector<std::vector<float>> expectedActivity;
    std::vector<std::vector<std::vector<int>>> expectedFrames;
    for (const auto& f : inputsToTest) {
        brain_encode(f.data());
        expected.push_back(readout);
        expectedActivity.push_back(activity);
        expectedFrames.push_back(frames);
    }
    const std::vector<size_t> sizes = argc > 2 ? std::vector<size_t>{1,4,8,9}
        : std::vector<size_t>{1,2,3,4,5,7,8,9,10,11,12,15,16,17,31,255,256};
    for (const auto size : sizes) {
        std::vector<Features> rows;
        std::vector<size_t> indices;
        for (size_t i = 0; i < size; ++i) {
            indices.push_back((i * 7 + size) % inputsToTest.size());
            rows.push_back(inputsToTest[indices.back()]);
        }
        const auto result = encodeRows(rows);
        for (size_t i = 0; i < size; ++i)
            if (std::memcmp(result[i].data(), expected[indices[i]].data(),
                            outputs.size() * sizeof(float)) != 0) {
                std::cerr << "Motor mismatch: batch " << size << ", row " << i << '\n';
                return 2;
            }
        if (std::memcmp(activity.data(), expectedActivity[indices.back()].data(),
                        count * sizeof(float)) != 0 ||
            frames != expectedFrames[indices.back()] || readout != result.back()) {
            std::cerr << "Final activity/telemetry mismatch: batch " << size << '\n';
            return 3;
        }
        std::cout << "Exact motor outputs, full final state and frames: batch " << size << std::endl;
    }
}
