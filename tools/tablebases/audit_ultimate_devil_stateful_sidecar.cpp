/* Exhaustive outcome census for a searchable stateful Devil sidecar. GPLv3+. */

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr std::size_t HeaderBytes = 32;
constexpr std::size_t RecordBytes = 10;
constexpr unsigned MaxMinions = 5;

#pragma pack(push, 1)
struct Header {
    std::array<char, 8> magic{};
    std::uint32_t version = 0;
    std::uint32_t square = 0;
    std::uint64_t count = 0;
    std::uint32_t recordBytes = 0;
    std::uint32_t reserved = 0;
};
#pragma pack(pop)
static_assert(sizeof(Header) == HeaderBytes);

constexpr std::uint64_t choose(unsigned n, unsigned k) {
    if (k > n)
        return 0;
    k = std::min(k, n - k);
    std::uint64_t value = 1;
    for (unsigned item = 1; item <= k; ++item)
        value = value * (n - k + item) / item;
    return value;
}

unsigned minion_count(std::uint64_t key) {
    const std::uint64_t rank = key & ((std::uint64_t{1} << 25) - 1);
    std::uint64_t upper = 0;
    for (unsigned count = 0; count <= MaxMinions; ++count) {
        upper += choose(80, count);
        if (rank < upper)
            return count;
    }
    throw std::runtime_error("invalid Minion combination rank");
}

std::uint64_t logical_key(const std::uint8_t* record) {
    std::uint64_t value = 0;
    std::memcpy(&value, record, 7);
    return value;
}

struct Stats {
    std::array<std::uint64_t, 4> outcomes{};
    std::array<std::array<std::uint64_t, 4>, MaxMinions + 1> byMinions{};
    std::array<std::array<std::uint64_t, 4>, 2> bySide{};
    std::array<
      std::array<std::array<std::uint64_t, 4>, 2>, MaxMinions + 1>
      aliveByMinionsAndSide{};
    std::array<std::uint16_t, 4> maxDtw{};
    std::array<std::array<std::uint64_t, 4>, MaxMinions + 1> witness{};
    std::array<std::array<bool, 4>, MaxMinions + 1> haveWitness{};
    std::uint64_t sortedResidual = 0;
};

void merge(Stats& destination, const Stats& source) {
    destination.sortedResidual += source.sortedResidual;
    for (unsigned outcome = 1; outcome <= 3; ++outcome) {
        destination.outcomes[outcome] += source.outcomes[outcome];
        destination.maxDtw[outcome] = std::max(destination.maxDtw[outcome],
                                               source.maxDtw[outcome]);
        for (unsigned side = 0; side < 2; ++side)
            destination.bySide[side][outcome] += source.bySide[side][outcome];
        for (unsigned minions = 0; minions <= MaxMinions; ++minions) {
            destination.byMinions[minions][outcome] +=
              source.byMinions[minions][outcome];
            for (unsigned side = 0; side < 2; ++side)
                destination.aliveByMinionsAndSide[minions][side][outcome] +=
                  source.aliveByMinionsAndSide[minions][side][outcome];
            if (!destination.haveWitness[minions][outcome] &&
                source.haveWitness[minions][outcome]) {
                destination.haveWitness[minions][outcome] = true;
                destination.witness[minions][outcome] =
                  source.witness[minions][outcome];
            }
        }
    }
}

void print_outcomes(const std::array<std::uint64_t, 4>& counts) {
    std::cout << "{\"win\":" << counts[1] << ",\"loss\":" << counts[2]
              << ",\"draw\":" << counts[3] << '}';
}

void audit_record(Stats& stats, const std::uint8_t* record,
                  std::uint64_t& previous, bool& havePrevious) {
    const std::uint64_t key = logical_key(record);
    const unsigned outcome = record[7];
    std::uint16_t dtw = 0;
    std::memcpy(&dtw, record + 8, sizeof(dtw));
    if (outcome < 1 || outcome > 3)
        throw std::runtime_error("invalid WDL byte");
    if (havePrevious && key <= previous)
        ++stats.sortedResidual;
    previous = key;
    havePrevious = true;
    const unsigned minions = minion_count(key);
    const unsigned side = static_cast<unsigned>((key >> 47) & 1);
    const bool alive = ((key >> 48) & 1) != 0;
    ++stats.outcomes[outcome];
    ++stats.byMinions[minions][outcome];
    ++stats.bySide[side][outcome];
    if (alive)
        ++stats.aliveByMinionsAndSide[minions][side][outcome];
    stats.maxDtw[outcome] = std::max(stats.maxDtw[outcome], dtw);
    if (!stats.haveWitness[minions][outcome]) {
        stats.haveWitness[minions][outcome] = true;
        stats.witness[minions][outcome] = key;
    }
}

Header read_header(std::istream& stream, unsigned expectedSquare) {
    Header header;
    stream.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (stream.gcount() != static_cast<std::streamsize>(sizeof(header)))
        throw std::runtime_error("truncated sidecar header");
    const std::array<char, 8> magic{{'U','F','D','S','V','1','\0','\0'}};
    if (header.magic != magic || header.version != 1 ||
        header.square != expectedSquare || header.recordBytes != RecordBytes ||
        header.reserved != 0)
        throw std::runtime_error("invalid stateful sidecar header");
    return header;
}

Stats audit_stream(std::istream& stream, const Header& header) {
    // Keep the working set below a MiB so the complete stateful class can be
    // consumed from a network pipe without materializing any UFDS partition.
    constexpr std::size_t RecordsPerBlock = 65536;
    std::array<std::uint8_t, RecordsPerBlock * RecordBytes> bytes{};
    Stats stats;
    std::uint64_t previous = 0;
    bool havePrevious = false;
    std::uint64_t completed = 0;
    while (completed < header.count) {
        const std::size_t records = static_cast<std::size_t>(
          std::min<std::uint64_t>(RecordsPerBlock, header.count - completed));
        const std::size_t wanted = records * RecordBytes;
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(wanted));
        if (stream.gcount() != static_cast<std::streamsize>(wanted))
            throw std::runtime_error("truncated stateful sidecar records");
        for (std::size_t index = 0; index < records; ++index)
            audit_record(stats, bytes.data() + index * RecordBytes,
                         previous, havePrevious);
        completed += records;
    }
    char extra = 0;
    if (stream.read(&extra, 1) || stream.gcount())
        throw std::runtime_error("stateful sidecar extent residual");
    return stats;
}

}  // namespace

int main(int argc, char** argv) try {
    std::string input;
    unsigned expectedSquare = std::numeric_limits<unsigned>::max();
    unsigned workers = 1;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto value = [&]() -> std::string {
            if (++index >= argc)
                throw std::runtime_error("missing option value");
            return argv[index];
        };
        if (argument == "--input") input = value();
        else if (argument == "--square") expectedSquare = std::stoul(value());
        else if (argument == "--workers") workers = std::stoul(value());
        else throw std::runtime_error("unknown option: " + argument);
    }
    if (input.empty() || expectedSquare == std::numeric_limits<unsigned>::max())
        throw std::runtime_error("--input and --square are required");
    if (!workers)
        throw std::runtime_error("--workers must be positive");

    Header header;
    Stats total;
    if (input == "-") {
        if (workers != 1)
            throw std::runtime_error("streaming input requires --workers 1");
        std::ios::sync_with_stdio(false);
        header = read_header(std::cin, expectedSquare);
        total = audit_stream(std::cin, header);
    }
    else {
        const int descriptor = ::open(input.c_str(), O_RDONLY);
        if (descriptor < 0)
            throw std::runtime_error("cannot open sidecar");
        const std::uint64_t extent = std::filesystem::file_size(input);
        if (extent < HeaderBytes)
            throw std::runtime_error("truncated sidecar header");
        void* mapped = ::mmap(nullptr, extent, PROT_READ, MAP_PRIVATE, descriptor, 0);
        if (mapped == MAP_FAILED)
            throw std::runtime_error("cannot map sidecar");
        const auto* bytes = static_cast<const std::uint8_t*>(mapped);
        std::memcpy(&header, bytes, sizeof(header));
        const std::array<char, 8> magic{{'U','F','D','S','V','1','\0','\0'}};
        if (header.magic != magic || header.version != 1 ||
            header.square != expectedSquare || header.recordBytes != RecordBytes ||
            header.reserved != 0)
            throw std::runtime_error("invalid stateful sidecar header");
        if (extent != HeaderBytes + header.count * RecordBytes)
            throw std::runtime_error("stateful sidecar extent residual");

        workers = std::min<std::uint64_t>(
          workers, std::max<std::uint64_t>(1, header.count));
        std::vector<Stats> partial(workers);
        std::vector<std::thread> threads;
        for (unsigned worker = 0; worker < workers; ++worker) {
            const std::uint64_t begin = header.count * worker / workers;
            const std::uint64_t end = header.count * (worker + 1) / workers;
            threads.emplace_back([&, worker, begin, end] {
                auto& stats = partial[worker];
                std::uint64_t previous = 0;
                bool havePrevious = false;
                for (std::uint64_t index = begin; index < end; ++index)
                    audit_record(stats, bytes + HeaderBytes + index * RecordBytes,
                                 previous, havePrevious);
            });
        }
        for (auto& thread : threads)
            thread.join();
        for (const auto& stats : partial)
            merge(total, stats);
        for (unsigned worker = 1; worker < workers; ++worker) {
            const std::uint64_t index = header.count * worker / workers;
            const auto left = logical_key(
              bytes + HeaderBytes + (index - 1) * RecordBytes);
            const auto right = logical_key(
              bytes + HeaderBytes + index * RecordBytes);
            if (right <= left)
                ++total.sortedResidual;
        }
        ::munmap(mapped, extent);
        ::close(descriptor);
    }

    const std::uint64_t conservation = total.outcomes[1] + total.outcomes[2] +
      total.outcomes[3];
    if (conservation != header.count || total.sortedResidual)
        throw std::runtime_error("stateful sidecar audit residual");
    std::cout << "{\"schema\":\"ultimate-devil-stateful-census-v1\","
              << "\"square\":" << expectedSquare << ",\"states\":"
              << header.count << ",\"outcomes\":";
    print_outcomes(total.outcomes);
    std::cout << ",\"by_side_to_move\":[";
    for (unsigned side = 0; side < 2; ++side) {
        if (side) std::cout << ',';
        print_outcomes(total.bySide[side]);
    }
    std::cout << "],\"by_minion_count\":[";
    for (unsigned minions = 0; minions <= MaxMinions; ++minions) {
        if (minions) std::cout << ',';
        std::cout << "{\"minions\":" << minions << ",\"outcomes\":";
        print_outcomes(total.byMinions[minions]);
        std::cout << ",\"alive_outcomes_by_side_to_move\":[";
        for (unsigned side = 0; side < 2; ++side) {
            if (side) std::cout << ',';
            print_outcomes(total.aliveByMinionsAndSide[minions][side]);
        }
        std::cout << ']';
        std::cout << ",\"witness_keys\":{";
        for (unsigned outcome = 1; outcome <= 3; ++outcome) {
            if (outcome > 1) std::cout << ',';
            const char* name = outcome == 1 ? "win" : outcome == 2 ? "loss" : "draw";
            std::cout << '\"' << name << "\":\"" << std::hex
                      << total.witness[minions][outcome] << std::dec << '\"';
        }
        std::cout << "}}";
    }
    std::cout << "],\"max_dtw\":{\"win\":" << total.maxDtw[1]
              << ",\"loss\":" << total.maxDtw[2] << ",\"draw\":"
              << total.maxDtw[3] << "},\"conservation_residual\":0,"
              << "\"sorted_key_residual\":0}\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "devil stateful census error: " << error.what() << '\n';
    return 1;
}
