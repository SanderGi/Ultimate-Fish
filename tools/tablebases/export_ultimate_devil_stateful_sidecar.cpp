/* Export searchable stateful Devil keys and WDL/DTW values, GPLv3 or later. */

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr std::size_t KeyBytes = 7;
constexpr std::size_t NodeBytes = 8;
constexpr std::size_t RecordBytes = 10;
// The top eight key bits are mostly fixed fields (alive, side, cooldown, and
// the no-secondary sentinel) for lone-Devil partitions.  That made the old
// 256-way split collapse almost the entire table into one multi-billion-row
// bucket.  Fifteen high bits include four bits of the king-pair rank while
// retaining contiguous key ranges, so independently sorted buckets can still
// be written directly at their final offsets without a merge pass.
constexpr std::size_t BucketBits = 15;
constexpr std::size_t BucketCount = std::size_t{1} << BucketBits;
constexpr std::size_t CompactKeyBytes = 7;

#pragma pack(push, 1)
struct Header {
    std::array<char, 8> magic{{'U', 'F', 'D', 'S', 'V', '1', '\0', '\0'}};
    std::uint32_t version = 1;
    std::uint32_t square = 0;
    std::uint64_t count = 0;
    std::uint32_t recordBytes = RecordBytes;
    std::uint32_t reserved = 0;
};
struct DiskRecord {
    std::array<std::uint8_t, KeyBytes> key{};
    std::uint8_t wdl = 0;
    std::uint16_t dtw = 0;
};
#pragma pack(pop)
static_assert(sizeof(Header) == 32);
static_assert(sizeof(DiskRecord) == RecordBytes);

std::uint64_t file_size(const std::string& path) {
    std::error_code error;
    const auto result = std::filesystem::file_size(path, error);
    if (error)
        throw std::runtime_error("cannot stat " + path + ": " + error.message());
    return result;
}

void read_exact(std::ifstream& stream, void* data, std::size_t bytes,
                const std::string& path) {
    stream.read(static_cast<char*>(data), static_cast<std::streamsize>(bytes));
    if (!stream)
        throw std::runtime_error("short read from " + path);
}

void pwrite_exact(int fd, const void* data, std::size_t bytes,
                  std::uint64_t offset, const std::string& path) {
    const auto* cursor = static_cast<const std::uint8_t*>(data);
    while (bytes) {
        const ssize_t written = ::pwrite(fd, cursor, bytes,
                                         static_cast<off_t>(offset));
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            throw std::runtime_error("short write to " + path);
        cursor += written;
        bytes -= static_cast<std::size_t>(written);
        offset += static_cast<std::uint64_t>(written);
    }
}

std::uint64_t decode_key(const std::array<std::uint8_t, KeyBytes>& bytes) {
    std::uint64_t value = 0;
    std::memcpy(&value, bytes.data(), KeyBytes);
    if (value >> 49)
        throw std::runtime_error("Devil key exceeds the 49-bit logical codec");
    return value;
}

constexpr std::uint64_t choose_devil(unsigned n, unsigned k) {
    if (k > n)
        return 0;
    if (k > n - k)
        k = n - k;
    std::uint64_t value = 1;
    for (unsigned item = 1; item <= k; ++item)
        value = value * (n - k + item) / item;
    return value;
}

std::uint64_t encode_minions(std::uint64_t low, std::uint16_t high) {
    const unsigned count = static_cast<unsigned>(__builtin_popcountll(low) +
                                                  __builtin_popcount(high));
    if (count > 5)
        throw std::runtime_error("legacy Devil key exceeds five Minions");
    std::uint64_t rank = 0;
    for (unsigned smaller = 0; smaller < count; ++smaller)
        rank += choose_devil(80, smaller);
    unsigned ordinal = 1;
    for (unsigned square = 0; square < 80; ++square) {
        const bool set = square < 64 ? ((low >> square) & 1ULL)
                                     : ((high >> (square - 64)) & 1U);
        if (set)
            rank += choose_devil(square, ordinal++);
    }
    if (rank >= (std::uint64_t{1} << 25))
        throw std::runtime_error("legacy Devil Minion rank overflow");
    return rank;
}

std::uint64_t compact_legacy_key(std::uint64_t low, std::uint64_t high,
                                 std::uint32_t fixedSquare) {
    constexpr unsigned NoSquare = 80;
    constexpr int SideShift = 16, WhiteKingShift = 17, BlackKingShift = 24;
    constexpr int DevilSquareShift = 31, CooldownShift = 38, SecondaryShift = 40;
    if (high >> 47)
        throw std::runtime_error("legacy Devil high key overflow");
    const unsigned whiteKing = static_cast<unsigned>((high >> WhiteKingShift) & 0x7f);
    const unsigned blackKing = static_cast<unsigned>((high >> BlackKingShift) & 0x7f);
    const unsigned devilSquare = static_cast<unsigned>((high >> DevilSquareShift) & 0x7f);
    const unsigned secondary = static_cast<unsigned>((high >> SecondaryShift) & 0x7f);
    const unsigned cooldown = static_cast<unsigned>((high >> CooldownShift) & 3);
    if (whiteKing >= 80 || blackKing >= 80 || whiteKing == blackKing ||
        secondary > NoSquare ||
        (devilSquare != NoSquare && devilSquare != fixedSquare) ||
        (devilSquare == NoSquare && cooldown))
        throw std::runtime_error("legacy Devil key field residual");
    const std::uint64_t minions = encode_minions(
      low, static_cast<std::uint16_t>(high & 0xffff));
    const unsigned blackIndex = blackKing < whiteKing ? blackKing : blackKing - 1;
    const std::uint64_t kings = whiteKing * 79ULL + blackIndex;
    const std::uint64_t side = (high >> SideShift) & 1;
    const std::uint64_t alive = devilSquare != NoSquare;
    const std::uint64_t value = minions | (kings << 25) |
      (static_cast<std::uint64_t>(secondary) << 38) |
      (static_cast<std::uint64_t>(cooldown) << 45) | (side << 47) |
      (alive << 48);
    if (value >> 49)
        throw std::runtime_error("compacted Devil key overflow");
    return value;
}

std::string bucket_path(const std::string& output, std::size_t bucket) {
    return output + ".bucket-" + std::to_string(bucket) + ".tmp";
}

void export_sidecar(const std::string& keysPath, const std::string& nodesPath,
                    const std::string& outputPath, std::uint32_t square,
                    unsigned workers, unsigned keyRecordBytes) {
    const std::uint64_t nodeExtent = file_size(nodesPath);
    if (!nodeExtent || nodeExtent % NodeBytes)
        throw std::runtime_error("Devil node extent is not an exact eight-byte plane");
    const std::uint64_t count = nodeExtent / NodeBytes;
    if (keyRecordBytes != 7 && keyRecordBytes != 14 && keyRecordBytes != 16)
        throw std::runtime_error("unsupported Devil key record width");
    if (file_size(keysPath) < count * keyRecordBytes)
        throw std::runtime_error("Devil key plane is shorter than its node plane");
    if (square >= 24 || square % 8 >= 4)
        throw std::runtime_error("invalid canonical fixed Devil square");
    workers = std::max(1u, std::min(workers, 8u));

    std::ifstream keys(keysPath, std::ios::binary);
    std::ifstream nodes(nodesPath, std::ios::binary);
    if (!keys || !nodes)
        throw std::runtime_error("cannot open Devil primary planes");
    std::array<std::unique_ptr<std::ofstream>, BucketCount> buckets;
    std::array<std::uint64_t, BucketCount> counts{};
    for (std::size_t bucket = 0; bucket < BucketCount; ++bucket) {
        const std::string path = bucket_path(outputPath, bucket);
        std::filesystem::remove(path);
    }
    std::array<std::uint8_t, NodeBytes> node{};
    std::array<std::uint8_t, 16> rawKey{};
    DiskRecord record;
    for (std::uint64_t index = 0; index < count; ++index) {
        read_exact(keys, rawKey.data(), keyRecordBytes, keysPath);
        read_exact(nodes, node.data(), NodeBytes, nodesPath);
        std::uint64_t key = 0;
        if (keyRecordBytes == CompactKeyBytes) {
            std::memcpy(record.key.data(), rawKey.data(), CompactKeyBytes);
            key = decode_key(record.key);
        }
        else {
            std::uint64_t low = 0, high = 0;
            std::memcpy(&low, rawKey.data(), sizeof(low));
            std::memcpy(&high, rawKey.data() + sizeof(low), keyRecordBytes - sizeof(low));
            key = compact_legacy_key(low, high, square);
            std::memcpy(record.key.data(), &key, CompactKeyBytes);
        }
        record.wdl = node[0];
        std::memcpy(&record.dtw, node.data() + 2, sizeof(record.dtw));
        if (record.wdl < 1 || record.wdl > 3)
            throw std::runtime_error("unsolved WDL in final Devil node plane");
        const std::size_t bucket = static_cast<std::size_t>(key >> (49 - BucketBits));
        if (!buckets[bucket]) {
            const std::string path = bucket_path(outputPath, bucket);
            buckets[bucket] = std::make_unique<std::ofstream>(
              path, std::ios::binary | std::ios::trunc);
            if (!*buckets[bucket])
                throw std::runtime_error("cannot create sidecar bucket: " + path);
        }
        buckets[bucket]->write(reinterpret_cast<const char*>(&record), sizeof(record));
        if (!*buckets[bucket])
            throw std::runtime_error("cannot write Devil sidecar bucket");
        ++counts[bucket];
        if ((index + 1) % 100'000'000 == 0)
            std::cout << "devil_sidecar_partition " << index + 1 << '/' << count
                      << '\n' << std::flush;
    }
    for (auto& bucket : buckets)
        if (bucket)
            bucket->close();

    std::array<std::uint64_t, BucketCount + 1> offsets{};
    for (std::size_t bucket = 0; bucket < BucketCount; ++bucket)
        offsets[bucket + 1] = offsets[bucket] + counts[bucket];
    if (offsets.back() != count)
        throw std::runtime_error("Devil sidecar bucket conservation residual");

    const std::string temporary = outputPath + ".tmp";
    std::filesystem::remove(temporary);
    if (std::filesystem::exists(outputPath))
        throw std::runtime_error("Devil sidecar output already exists");
    const int output = ::open(temporary.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (output == -1)
        throw std::runtime_error("cannot create Devil sidecar output");
    Header header;
    header.square = square;
    header.count = count;
    pwrite_exact(output, &header, sizeof(header), 0, temporary);
    if (::ftruncate(output, static_cast<off_t>(sizeof(Header) + count * RecordBytes)) != 0)
        throw std::runtime_error("cannot size Devil sidecar output");

    std::atomic<std::size_t> nextBucket{0};
    std::atomic<bool> failed{false};
    std::exception_ptr failure;
    std::mutex failureMutex;
    std::vector<std::thread> tasks;
    for (unsigned worker = 0; worker < workers; ++worker)
        tasks.emplace_back([&] {
            try {
                while (!failed.load(std::memory_order_relaxed)) {
                    const std::size_t bucket = nextBucket.fetch_add(1);
                    if (bucket >= BucketCount)
                        break;
                    const std::string path = bucket_path(outputPath, bucket);
                    if (!counts[bucket])
                        continue;
                    std::ifstream input(path, std::ios::binary);
                    if (!input)
                        throw std::runtime_error("cannot reopen sidecar bucket");
                    std::vector<DiskRecord> values;
                    values.reserve(static_cast<std::size_t>(counts[bucket]));
                    DiskRecord item;
                    for (std::uint64_t index = 0; index < counts[bucket]; ++index) {
                        read_exact(input, &item, sizeof(item), path);
                        values.push_back(item);
                    }
                    std::sort(values.begin(), values.end(),
                              [](const auto& first, const auto& second) {
                                  return decode_key(first.key) < decode_key(second.key);
                              });
                    for (std::size_t index = 0; index < values.size(); ++index) {
                        if (index && values[index - 1].key == values[index].key)
                            throw std::runtime_error("duplicate Devil logical key");
                    }
                    pwrite_exact(output, values.data(), values.size() * sizeof(DiskRecord),
                                 sizeof(Header) + offsets[bucket] * RecordBytes,
                                 temporary);
                    std::filesystem::remove(path);
                    std::cout << "devil_sidecar_sort bucket " << bucket
                              << " records " << values.size() << '\n' << std::flush;
                }
            }
            catch (...) {
                failed.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(failureMutex);
                if (!failure)
                    failure = std::current_exception();
            }
        });
    for (auto& task : tasks)
        task.join();
    if (failure)
        std::rethrow_exception(failure);
    if (::fsync(output) != 0 || ::close(output) != 0)
        throw std::runtime_error("cannot sync Devil sidecar output");
    if (std::filesystem::file_size(temporary) != sizeof(Header) + count * RecordBytes)
        throw std::runtime_error("Devil sidecar final extent residual");
    std::filesystem::rename(temporary, outputPath);
    std::cout << "DEVIL_STATEFUL_SIDECAR_OK square " << square << " states "
              << count << " output " << outputPath << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    std::string keys, nodes, output;
    std::uint32_t square = std::numeric_limits<std::uint32_t>::max();
    unsigned workers = 1;
    unsigned keyRecordBytes = 7;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto value = [&]() -> std::string {
            if (++index >= argc)
                throw std::runtime_error("missing option value");
            return argv[index];
        };
        if (argument == "--keys") keys = value();
        else if (argument == "--nodes") nodes = value();
        else if (argument == "--output") output = value();
        else if (argument == "--square") square = std::stoul(value());
        else if (argument == "--workers") workers = std::stoul(value());
        else if (argument == "--key-record-bytes") keyRecordBytes = std::stoul(value());
        else throw std::runtime_error("unknown option: " + argument);
    }
    if (keys.empty() || nodes.empty() || output.empty())
        throw std::runtime_error("--keys, --nodes, and --output are required");
    export_sidecar(keys, nodes, output, square, workers, keyRecordBytes);
}
