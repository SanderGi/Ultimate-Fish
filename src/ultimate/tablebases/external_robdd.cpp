/*
  Ultimate Fish - exact external-memory reduced ordered decision diagrams
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "external_robdd.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint64_t NodeBytes = 9;

[[noreturn]] void system_error(const std::string& operation,
                               const std::string& path) {
    throw std::runtime_error(operation + " " + path + ": " +
                             std::strerror(errno));
}

class Mapping {
  public:
    Mapping() = default;
    Mapping(const std::string& path, std::uint64_t bytes, bool create,
            bool zeroOnCreate) : path_(path), bytes_(bytes) {
        if (!bytes || bytes > static_cast<std::uint64_t>(
              std::numeric_limits<std::size_t>::max()))
            throw std::runtime_error("external ROBDD mapping size is invalid");
        int flags = O_RDWR;
        if (create)
            flags |= O_CREAT | O_TRUNC;
        descriptor_ = ::open(path.c_str(), flags, 0600);
        if (descriptor_ < 0)
            system_error("cannot open", path);
        if (create && ::ftruncate(descriptor_, static_cast<off_t>(bytes)) != 0)
            system_error("cannot size", path);
        struct stat status{};
        if (::fstat(descriptor_, &status) != 0 ||
            static_cast<std::uint64_t>(status.st_size) != bytes)
            system_error("cannot verify size of", path);
        data_ = static_cast<std::uint8_t*>(::mmap(
          nullptr, static_cast<std::size_t>(bytes), PROT_READ | PROT_WRITE,
          MAP_SHARED, descriptor_, 0));
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            system_error("cannot mmap", path);
        }
        // A newly truncated regular file reads as zero.  Touching multi-GiB
        // scratch just to memset it would defeat demand paging; this flag is
        // retained to make the requirement explicit at call sites.
        (void)zeroOnCreate;
    }
    ~Mapping() { close(); }
    Mapping(Mapping&& other) noexcept { swap(other); }
    Mapping& operator=(Mapping&& other) noexcept {
        if (this != &other) {
            close();
            swap(other);
        }
        return *this;
    }
    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;
    [[nodiscard]] std::uint8_t* data() { return data_; }
    [[nodiscard]] const std::uint8_t* data() const { return data_; }
    [[nodiscard]] std::uint64_t size() const { return bytes_; }
    void flush() {
        if (data_ && ::msync(data_, static_cast<std::size_t>(bytes_), MS_SYNC))
            system_error("cannot flush", path_);
    }
    void discard(std::uint64_t offset, std::uint64_t bytes) {
        if (!data_ || !bytes || offset >= bytes_)
            return;
        const std::uint64_t page = static_cast<std::uint64_t>(::getpagesize());
        const std::uint64_t begin = offset / page * page;
        const std::uint64_t requestedEnd = std::min(bytes_, offset + bytes);
        const std::uint64_t end = std::min(bytes_,
          (requestedEnd + page - 1) / page * page);
#ifdef MADV_DONTNEED
        if (::madvise(data_ + begin, static_cast<std::size_t>(end - begin),
                      MADV_DONTNEED) != 0 && errno != EINVAL && errno != ENOSYS)
            system_error("cannot discard pages for", path_);
#else
        (void)begin;
        (void)end;
#endif
    }
  private:
    void close() noexcept {
        if (data_)
            ::munmap(data_, static_cast<std::size_t>(bytes_));
        if (descriptor_ >= 0)
            ::close(descriptor_);
        data_ = nullptr;
        descriptor_ = -1;
        bytes_ = 0;
    }
    void swap(Mapping& other) noexcept {
        std::swap(path_, other.path_);
        std::swap(descriptor_, other.descriptor_);
        std::swap(data_, other.data_);
        std::swap(bytes_, other.bytes_);
    }
    std::string path_;
    int descriptor_ = -1;
    std::uint8_t* data_ = nullptr;
    std::uint64_t bytes_ = 0;
};

void store_u32(std::uint8_t* target, std::uint32_t value) {
    std::memcpy(target, &value, sizeof(value));
}

[[nodiscard]] std::uint32_t load_u32(const std::uint8_t* source) {
    std::uint32_t value = 0;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

[[nodiscard]] std::uint64_t mix64(std::uint64_t value) {
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

struct ApplyKey {
    std::uint32_t lhs = 0;
    std::uint32_t rhs = 0;
    std::uint8_t operation = 0;
    friend bool operator==(const ApplyKey& lhs, const ApplyKey& rhs) {
        return lhs.lhs == rhs.lhs && lhs.rhs == rhs.rhs &&
               lhs.operation == rhs.operation;
    }
};

struct ApplyHash {
    std::size_t operator()(const ApplyKey& key) const noexcept {
        return static_cast<std::size_t>(mix64(
          (std::uint64_t(key.lhs) << 32) ^ key.rhs ^
          (std::uint64_t(key.operation) << 61)));
    }
};

struct ComposeKey {
    std::uint64_t relation = 0;
    std::uint32_t root = 0;
    friend bool operator==(const ComposeKey& lhs, const ComposeKey& rhs) {
        return lhs.relation == rhs.relation && lhs.root == rhs.root;
    }
};

struct ComposeHash {
    std::size_t operator()(const ComposeKey& key) const noexcept {
        return static_cast<std::size_t>(mix64(
          key.relation ^ (std::uint64_t(key.root) << 32)));
    }
};

}  // namespace

struct ExternalRobdd::Impl {
    // The node arena is the authoritative collision-free key store.  Keeping
    // only its ID here cuts the 2^30-slot index from 16 GiB to 4 GiB, which is
    // essential on the 16 GiB development host.  A hash collision always
    // loads and compares the full 9-byte node before returning the ID.
    using UniqueSlot = std::uint32_t;

    [[nodiscard]] static Limits validated(Limits value) {
        if (!value.variables || value.variables > 80 ||
            value.maxNodes < value.variables + 2 ||
            value.uniqueSlots < 8 ||
            (value.uniqueSlots & (value.uniqueSlots - 1)) ||
            required_bytes(value) > value.budgetBytes)
            throw std::runtime_error(
              "external ROBDD limits exceed domain/budget constraints");
        return value;
    }

    Impl(const std::string& prefix, const Limits& requested, bool create)
      : limits(validated(requested)), nodes(prefix + ".nodes",
          std::uint64_t(limits.maxNodes) * NodeBytes, create, false),
        prefix(prefix) {
        if (create) {
            unique = Mapping(prefix + ".unique",
              limits.uniqueSlots * sizeof(UniqueSlot), true, true);
            slots = reinterpret_cast<UniqueSlot*>(unique.data());
            count = 0;
            append_raw({static_cast<std::uint8_t>(limits.variables), 0, 0});
            append_raw({static_cast<std::uint8_t>(limits.variables), 1, 1});
            variables.resize(limits.variables);
            for (std::uint32_t variable = 0; variable < limits.variables;
                 ++variable)
                variables[variable] = make(variable, False, True);
        }
        else {
            // Arenas are allocated to maxNodes up front, so their file size
            // cannot recover the logical node count.  Allocated tuples are
            // nevertheless append-only and dense; every unused tuple is the
            // all-zero sparse-file value, which is not a reduced ROBDD node
            // because its two children are equal.  Discover the exact prefix
            // sequentially and validate its structural ordering before any
            // persisted root is admitted.
            const auto raw = [&](Id id) {
                const std::uint8_t* source =
                  nodes.data() + std::uint64_t(id) * NodeBytes;
                return Node{source[0], load_u32(source + 1),
                            load_u32(source + 5)};
            };
            const Node falseNode = raw(False);
            const Node trueNode = raw(True);
            if (falseNode.variable != limits.variables || falseNode.low ||
                falseNode.high || trueNode.variable != limits.variables ||
                trueNode.low != True || trueNode.high != True)
                throw std::runtime_error(
                  "existing external ROBDD has invalid terminal nodes");
            std::uint32_t reopenedCount = 2;
            while (reopenedCount < limits.maxNodes) {
                const Node value = raw(reopenedCount);
                if (!value.variable && !value.low && !value.high)
                    break;
                if (value.variable >= limits.variables ||
                    value.low >= reopenedCount || value.high >= reopenedCount ||
                    value.low == value.high)
                    throw std::runtime_error(
                      "existing external ROBDD has an invalid dense tuple prefix");
                ++reopenedCount;
            }
            count.store(reopenedCount, std::memory_order_relaxed);
            if (reopenedCount < limits.variables + 2)
                throw std::runtime_error(
                  "existing external ROBDD lacks canonical variable nodes");

            // The dense node arena is authoritative; the open-addressed
            // unique table is a disposable acceleration index.  Long Ghost
            // fixed points used to become effectively serial when that index
            // approached 100% occupancy, yet increasing --unique-slots made
            // an otherwise valid checkpoint impossible to reopen because the
            // old index file had a different size.  Rebuild a larger index
            // beside the original and publish it with one atomic rename.  A
            // crash therefore leaves the old checkpoint untouched.  Until a
            // compaction writes a native index at the new size, reopen rebuilds
            // this disposable side index from the authoritative dense arena;
            // it never trusts a cache left by a different arena generation.
            const std::uint64_t requestedUniqueBytes =
              limits.uniqueSlots * sizeof(UniqueSlot);
            const std::string originalUniquePath = prefix + ".unique";
            struct stat uniqueStatus{};
            if (::stat(originalUniquePath.c_str(), &uniqueStatus) != 0)
                system_error("cannot stat", originalUniquePath);
            const std::uint64_t originalUniqueBytes =
              static_cast<std::uint64_t>(uniqueStatus.st_size);
            if (originalUniqueBytes == requestedUniqueBytes) {
                unique = Mapping(originalUniquePath, requestedUniqueBytes,
                                 false, false);
            }
            else {
                if (reopenedCount >= limits.uniqueSlots)
                    throw std::runtime_error(
                      "expanded external ROBDD unique table is still too small");
                const std::string expandedUniquePath = originalUniquePath +
                  ".slots-" + std::to_string(limits.uniqueSlots);
                const std::string temporary = expandedUniquePath +
                  ".tmp-" + std::to_string(static_cast<long long>(::getpid()));
                Mapping rebuilt(temporary, requestedUniqueBytes, true, true);
                auto* rebuiltSlots =
                  reinterpret_cast<UniqueSlot*>(rebuilt.data());
                const std::uint32_t rehashWorkers = std::max(1u,
                  std::min(32u, std::thread::hardware_concurrency()));
                std::atomic<Id> nextRehash{2};
                std::atomic<std::uint64_t> rehashed{0};
                std::atomic<bool> rehashFailed{false};
                std::exception_ptr rehashException;
                std::mutex rehashExceptionMutex;
                std::mutex rehashOutputMutex;
                constexpr Id RehashChunk = 4096;
                const auto rehash = [&] {
                    try {
                        while (!rehashFailed.load(std::memory_order_relaxed)) {
                            const Id begin = nextRehash.fetch_add(
                              RehashChunk, std::memory_order_relaxed);
                            if (begin >= reopenedCount)
                                break;
                            const Id end = std::min<Id>(
                              reopenedCount, begin + RehashChunk);
                            for (Id id = begin; id < end; ++id) {
                                const Node value = raw(id);
                                std::uint64_t slot = node_hash(
                                  value.variable, value.low, value.high) &
                                  (limits.uniqueSlots - 1);
                                for (;;) {
                                    std::lock_guard<std::mutex> lock(
                                      uniqueMutexes[slot &
                                        (UniqueMutexStripes - 1)]);
                                    UniqueSlot& entry = rebuiltSlots[slot];
                                    if (!entry) {
                                        entry = id;
                                        break;
                                    }
                                    const Node existing = raw(entry);
                                    if (existing.variable == value.variable &&
                                        existing.low == value.low &&
                                        existing.high == value.high)
                                        throw std::runtime_error(
                                          "external ROBDD dense arena contains duplicate nodes");
                                    slot = (slot + 1) &
                                      (limits.uniqueSlots - 1);
                                }
                            }
                            const std::uint64_t done = rehashed.fetch_add(
                              end - begin, std::memory_order_relaxed) +
                              (end - begin);
                            if (done / 10'000'000 !=
                                (done - (end - begin)) / 10'000'000) {
                                std::lock_guard<std::mutex> lock(
                                  rehashOutputMutex);
                                std::cout << "external_robdd_unique_rehash nodes "
                                          << done << '/' << reopenedCount
                                          << " workers " << rehashWorkers
                                          << '\n' << std::flush;
                            }
                        }
                    }
                    catch (...) {
                        rehashFailed.store(true, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(rehashExceptionMutex);
                        if (!rehashException)
                            rehashException = std::current_exception();
                    }
                };
                std::vector<std::thread> rehashThreads;
                rehashThreads.reserve(rehashWorkers);
                for (std::uint32_t worker = 0; worker < rehashWorkers; ++worker)
                    rehashThreads.emplace_back(rehash);
                for (std::thread& worker : rehashThreads)
                    worker.join();
                if (rehashException)
                    std::rethrow_exception(rehashException);
                rebuilt.flush();
                if (::rename(temporary.c_str(),
                             expandedUniquePath.c_str()) != 0)
                    system_error("cannot publish", expandedUniquePath);
                unique = std::move(rebuilt);
                std::cout << "external_robdd_unique_rehash_complete nodes "
                          << reopenedCount << " old_slots "
                          << originalUniqueBytes / sizeof(UniqueSlot)
                          << " new_slots " << limits.uniqueSlots << '\n'
                          << std::flush;
            }
            slots = reinterpret_cast<UniqueSlot*>(unique.data());
            variables.resize(limits.variables);
            for (std::uint32_t variable = 0; variable < limits.variables;
                 ++variable) {
                const Id id = variable + 2;
                const Node value = raw(id);
                if (value.variable != variable || value.low != False ||
                    value.high != True)
                    throw std::runtime_error(
                      "existing external ROBDD variable-node residual");
                variables[variable] = id;
            }
        }
    }

    struct ComputedCaches {
        std::uint64_t generation = 0;
        std::unordered_map<ApplyKey, Id, ApplyHash> apply;
        std::unordered_map<Id, Id> negate;
        std::unordered_map<ComposeKey, Id, ComposeHash> compose;
    };

    [[nodiscard]] ComputedCaches& caches() const {
        static thread_local ComputedCaches result;
        if (result.generation != generation) {
            result.apply.clear();
            result.negate.clear();
            result.compose.clear();
            result.apply.reserve(std::min<std::size_t>(
              limits.applyCacheEntries, 1'000'000));
            result.negate.reserve(std::min<std::size_t>(
              limits.unaryCacheEntries, 500'000));
            result.compose.reserve(std::min<std::size_t>(
              limits.composeCacheEntries, 500'000));
            result.generation = generation;
        }
        return result;
    }

    void clear_computed_caches() const {
        ComputedCaches& value = caches();
        value.apply.clear();
        value.negate.clear();
        value.compose.clear();
    }

    [[nodiscard]] static std::uint64_t required_bytes(const Limits& value) {
        if (value.uniqueSlots > std::numeric_limits<std::uint64_t>::max() /
                                  sizeof(UniqueSlot))
            return std::numeric_limits<std::uint64_t>::max();
        const std::uint64_t nodes = std::uint64_t(value.maxNodes) * NodeBytes;
        const std::uint64_t table = value.uniqueSlots * sizeof(UniqueSlot);
        return nodes > std::numeric_limits<std::uint64_t>::max() - table
             ? std::numeric_limits<std::uint64_t>::max() : nodes + table;
    }

    [[nodiscard]] Node get(Id id) const {
        if (id >= count.load(std::memory_order_acquire))
            throw std::runtime_error("external ROBDD node ID is out of range");
        const std::uint8_t* source = nodes.data() + std::uint64_t(id) * NodeBytes;
        return {source[0], load_u32(source + 1), load_u32(source + 5)};
    }

    Id append_raw(Node value) {
        std::uint32_t id = count.load(std::memory_order_relaxed);
        for (;;) {
            if (id >= limits.maxNodes)
                throw std::runtime_error(
                  "external ROBDD exhausted its exact node allocation");
            if (count.compare_exchange_weak(id, id + 1,
                  std::memory_order_acq_rel, std::memory_order_relaxed))
                break;
        }
        std::uint8_t* target = nodes.data() + std::uint64_t(id) * NodeBytes;
        target[0] = value.variable;
        store_u32(target + 1, value.low);
        store_u32(target + 5, value.high);
        return id;
    }

    [[nodiscard]] std::uint64_t node_hash(std::uint32_t variable, Id low,
                                          Id high) const {
        return mix64(std::uint64_t(variable) * 0x9e3779b97f4a7c15ULL ^
                     (std::uint64_t(low) << 32) ^ high);
    }

    [[nodiscard]] Id make(std::uint32_t variable, Id low, Id high) {
        if (low == high)
            return low;
        if (variable >= limits.variables ||
            low >= count.load(std::memory_order_acquire) ||
            high >= count.load(std::memory_order_acquire))
            throw std::runtime_error("invalid external ROBDD node tuple");
        const auto child_variable = [&](Id child) {
            return child <= True ? limits.variables : get(child).variable;
        };
        if (child_variable(low) <= variable || child_variable(high) <= variable)
            throw std::runtime_error("external ROBDD variable order violation");
        std::uint64_t slot = node_hash(variable, low, high) &
                             (limits.uniqueSlots - 1);
        for (std::uint64_t probes = 0; probes < limits.uniqueSlots; ++probes) {
            // Every slot read/write is protected by its stripe. A newly
            // reserved node tuple is fully written before its slot is
            // published and the stripe is released, preserving collision-
            // checked canonicity without serializing unrelated hashes.
            std::lock_guard<std::mutex> lock(
              uniqueMutexes[slot & (UniqueMutexStripes - 1)]);
            UniqueSlot& entry = slots[slot];
            if (!entry) {
                const Id id = append_raw(
                  {static_cast<std::uint8_t>(variable), low, high});
                entry = id;
                return entry;
            }
            const Node existing = get(entry);
            if (existing.variable == variable && existing.low == low &&
                existing.high == high)
                return entry;
            slot = (slot + 1) & (limits.uniqueSlots - 1);
        }
        throw std::runtime_error(
          "external ROBDD collision-checked unique table is full");
    }

    [[nodiscard]] std::uint32_t top(Id root) const {
        return root <= True ? limits.variables : get(root).variable;
    }

    [[nodiscard]] Id branch(Id root, std::uint32_t variable, bool high) const {
        if (top(root) != variable)
            return root;
        const Node value = get(root);
        return high ? value.high : value.low;
    }

    [[nodiscard]] Id apply(std::uint8_t operation, Id lhs, Id rhs) {
        if (lhs > rhs)
            std::swap(lhs, rhs);
        if (!operation) {
            if (lhs == False || rhs == False) return False;
            if (lhs == True) return rhs;
            if (lhs == rhs) return lhs;
        }
        else {
            if (rhs == True) return True;
            if (lhs == False) return rhs;
            if (lhs == rhs) return lhs;
        }
        const ApplyKey key{lhs, rhs, operation};
        ComputedCaches& cache = caches();
        if (const auto found = cache.apply.find(key);
            found != cache.apply.end())
            return found->second;
        const std::uint32_t variable = std::min(top(lhs), top(rhs));
        const Id result = make(variable,
          apply(operation, branch(lhs, variable, false),
                           branch(rhs, variable, false)),
          apply(operation, branch(lhs, variable, true),
                           branch(rhs, variable, true)));
        if (cache.apply.size() < limits.applyCacheEntries)
            cache.apply.emplace(key, result);
        return result;
    }

    [[nodiscard]] Id negate(Id root) {
        if (root == False) return True;
        if (root == True) return False;
        ComputedCaches& cache = caches();
        if (const auto found = cache.negate.find(root);
            found != cache.negate.end())
            return found->second;
        const Node value = get(root);
        const Id result = make(value.variable, negate(value.low),
                               negate(value.high));
        if (cache.negate.size() < limits.unaryCacheEntries)
            cache.negate.emplace(root, result);
        return result;
    }

    [[nodiscard]] bool implies(Id lhs, Id rhs) {
        if (lhs == False || rhs == True || lhs == rhs)
            return true;
        if (lhs == True || rhs == False)
            return false;
        const ApplyKey key{lhs, rhs, 2};
        ComputedCaches& cache = caches();
        if (const auto found = cache.apply.find(key);
            found != cache.apply.end())
            return found->second == True;
        const std::uint32_t variable = std::min(top(lhs), top(rhs));
        const bool result =
          implies(branch(lhs, variable, false),
                  branch(rhs, variable, false)) &&
          implies(branch(lhs, variable, true),
                  branch(rhs, variable, true));
        if (cache.apply.size() < limits.applyCacheEntries)
            cache.apply.emplace(key, result ? True : False);
        return result;
    }

    [[nodiscard]] Id compose(Id root, const std::vector<Id>& image,
                             std::uint64_t relation) {
        if (root <= True)
            return root;
        const ComposeKey key{relation, root};
        ComputedCaches& cache = caches();
        if (const auto found = cache.compose.find(key);
            found != cache.compose.end())
            return found->second;
        const Node value = get(root);
        const Id low = compose(value.low, image, relation);
        const Id high = compose(value.high, image, relation);
        const Id condition = image.at(value.variable);
        const Id result = apply(1, apply(0, condition, high),
          apply(0, negate(condition), low));
        if (cache.compose.size() < limits.composeCacheEntries)
            cache.compose.emplace(key, result);
        return result;
    }

    Limits limits;
    Mapping nodes;
    Mapping unique;
    UniqueSlot* slots = nullptr;
    std::string prefix;
    std::atomic<std::uint32_t> count{0};
    std::vector<Id> variables;
    static constexpr std::size_t UniqueMutexStripes = 4096;
    std::array<std::mutex, UniqueMutexStripes> uniqueMutexes;
    const std::uint64_t generation =
      nextGeneration.fetch_add(1, std::memory_order_relaxed);
    inline static std::atomic<std::uint64_t> nextGeneration{1};

    void seal_for_streaming_compaction() {
        clear_computed_caches();
        nodes.flush();
        // The source unique index is never consulted while copying an already
        // reduced arena.  Unmapping it before the replacement arena is opened
        // prevents two random-access unique indexes being resident together.
        unique = Mapping{};
        slots = nullptr;
    }
};

ExternalRobdd::ExternalRobdd(const std::string& prefix, const Limits& limits,
                             bool create)
  : impl_(std::make_unique<Impl>(prefix, limits, create)) {}
ExternalRobdd::~ExternalRobdd() = default;
ExternalRobdd::ExternalRobdd(ExternalRobdd&&) noexcept = default;
ExternalRobdd& ExternalRobdd::operator=(ExternalRobdd&&) noexcept = default;

ExternalRobdd::Id ExternalRobdd::variable(std::uint32_t variable) const {
    return impl_->variables.at(variable);
}
ExternalRobdd::Node ExternalRobdd::node(Id id) const { return impl_->get(id); }
std::uint32_t ExternalRobdd::variable_count() const {
    return impl_->limits.variables;
}
std::uint32_t ExternalRobdd::node_count() const {
    return impl_->count.load(std::memory_order_acquire);
}
const ExternalRobdd::Limits& ExternalRobdd::limits() const {
    return impl_->limits;
}
ExternalRobdd::Id ExternalRobdd::make(
  std::uint32_t variable, Id low, Id high) {
    if (!impl_->slots)
        throw std::runtime_error("cannot extend a sealed external ROBDD");
    return impl_->make(variable, low, high);
}
ExternalRobdd::Id ExternalRobdd::logical_not(Id root) {
    return impl_->negate(root);
}
ExternalRobdd::Id ExternalRobdd::logical_and(Id lhs, Id rhs) {
    return impl_->apply(0, lhs, rhs);
}
ExternalRobdd::Id ExternalRobdd::logical_or(Id lhs, Id rhs) {
    return impl_->apply(1, lhs, rhs);
}
ExternalRobdd::Id ExternalRobdd::ite(
  Id condition, Id whenTrue, Id whenFalse) {
    return logical_or(logical_and(condition, whenTrue),
                      logical_and(logical_not(condition), whenFalse));
}
bool ExternalRobdd::implies(Id lhs, Id rhs) {
    return impl_->implies(lhs, rhs);
}
ExternalRobdd::Id ExternalRobdd::any(
  std::uint64_t low, std::uint16_t high) {
    Id result = False;
    for (std::uint32_t variable = 0; variable < impl_->limits.variables;
         ++variable) {
        const bool set = variable < 64 ? (low >> variable) & 1u
                                       : (high >> (variable - 64)) & 1u;
        if (set)
            result = logical_or(result, this->variable(variable));
    }
    return result;
}
ExternalRobdd::Id ExternalRobdd::subset_of(
  std::uint64_t low, std::uint16_t high) {
    Id result = True;
    for (std::uint32_t variable = 0; variable < impl_->limits.variables;
         ++variable) {
        const bool set = variable < 64 ? (low >> variable) & 1u
                                       : (high >> (variable - 64)) & 1u;
        if (!set)
            result = logical_and(result,
                                 logical_not(this->variable(variable)));
    }
    return result;
}
ExternalRobdd::Id ExternalRobdd::compose(
  Id root, const std::vector<Id>& image, std::uint64_t relationId) {
    if (image.size() != impl_->limits.variables)
        throw std::runtime_error("external ROBDD compose image has wrong size");
    return impl_->compose(root, image, relationId);
}
bool ExternalRobdd::evaluate(
  Id root, std::uint64_t low, std::uint16_t high) const {
    while (root > True) {
        const Node value = impl_->get(root);
        const bool set = value.variable < 64
          ? (low >> value.variable) & 1u
          : (high >> (value.variable - 64)) & 1u;
        root = set ? value.high : value.low;
    }
    return root == True;
}

bool ExternalRobdd::is_upward_closed(
  Id root, std::uint64_t low, std::uint16_t high) {
    for (std::uint32_t variable = 0; variable < impl_->limits.variables;
         ++variable) {
        const bool active = variable < 64 ? (low >> variable) & 1u
                                          : (high >> (variable - 64)) & 1u;
        if (!active)
            continue;
        std::vector<Id> image(impl_->limits.variables);
        for (std::uint32_t item = 0; item < impl_->limits.variables; ++item)
            image[item] = item == variable ? False : this->variable(item);
        const Id lowRoot = compose(root, image,
          0x100000000ULL + variable * 2);
        image[variable] = True;
        const Id highRoot = compose(root, image,
          0x100000001ULL + variable * 2);
        if (!implies(lowRoot, highRoot))
            return false;
    }
    return true;
}

bool ExternalRobdd::is_downward_closed(
  Id root, std::uint64_t low, std::uint16_t high) {
    for (std::uint32_t variable = 0; variable < impl_->limits.variables;
         ++variable) {
        const bool active = variable < 64 ? (low >> variable) & 1u
                                          : (high >> (variable - 64)) & 1u;
        if (!active)
            continue;
        std::vector<Id> image(impl_->limits.variables);
        for (std::uint32_t item = 0; item < impl_->limits.variables; ++item)
            image[item] = item == variable ? False : this->variable(item);
        const Id lowRoot = compose(root, image,
          0x200000000ULL + variable * 2);
        image[variable] = True;
        const Id highRoot = compose(root, image,
          0x200000001ULL + variable * 2);
        if (!implies(highRoot, lowRoot))
            return false;
    }
    return true;
}

void ExternalRobdd::clear_computed_caches() {
    impl_->clear_computed_caches();
}
void ExternalRobdd::flush() {
    impl_->nodes.flush();
    impl_->unique.flush();
}

std::pair<std::unique_ptr<ExternalRobdd>, ExternalRobdd::CompactionCertificate>
ExternalRobdd::compact(const std::string& replacementPrefix,
                       const std::string& remapPath,
                       std::vector<Id>& roots,
                       std::uint32_t workers) {
    CompactionCertificate certificate;
    certificate.roots = roots.size();
    impl_->seal_for_streaming_compaction();
    const std::uint32_t sourceCount =
      impl_->count.load(std::memory_order_acquire);
    const std::uint64_t remapBytes = std::uint64_t(sourceCount) * sizeof(Id);
    Mapping remap(remapPath, remapBytes, true, true);
    auto* mapped = reinterpret_cast<Id*>(remap.data());
    const std::uint64_t markWords = (std::uint64_t(sourceCount) + 63) / 64;
    const std::uint64_t markBytes = markWords * sizeof(std::uint64_t);
    Mapping marks(remapPath + ".marks", markBytes, true, true);
    auto* markData = reinterpret_cast<std::uint64_t*>(marks.data());
    const auto marked = [&](Id id) {
        return (__atomic_load_n(&markData[id / 64], __ATOMIC_RELAXED) >>
                (id % 64)) & 1ULL;
    };
    const auto claim_mark = [&](Id id) {
        const std::uint64_t mask = std::uint64_t{1} << (id % 64);
        return (__atomic_fetch_or(&markData[id / 64], mask,
                                  __ATOMIC_RELAXED) & mask) == 0;
    };
    for (const Id root : roots)
        if (root >= sourceCount)
            throw std::runtime_error("compaction root is out of range");
    const std::uint32_t workerCount = std::max<std::uint32_t>(
      1, std::min<std::uint64_t>(workers, sourceCount));
    const auto parallel_chunks = [&](std::uint64_t items,
                                     std::uint64_t chunk, auto&& action) {
        std::atomic<std::uint64_t> next{0};
        std::atomic<bool> failed{false};
        std::exception_ptr failure;
        std::mutex failureMutex;
        const auto run = [&](std::uint32_t worker) {
            try {
                while (!failed.load(std::memory_order_relaxed)) {
                    const std::uint64_t begin = next.fetch_add(
                      chunk, std::memory_order_relaxed);
                    if (begin >= items)
                        break;
                    action(worker, begin, std::min(items, begin + chunk));
                }
            }
            catch (...) {
                failed.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(failureMutex);
                if (!failure)
                    failure = std::current_exception();
            }
        };
        if (workerCount == 1)
            run(0);
        else {
            std::vector<std::thread> tasks;
            tasks.reserve(workerCount);
            for (std::uint32_t worker = 0; worker < workerCount; ++worker)
                tasks.emplace_back(run, worker);
            for (auto& task : tasks)
                task.join();
        }
        if (failure)
            std::rethrow_exception(failure);
    };

    // A recursive mark rooted at millions of heavily overlapping BDDs becomes
    // badly load-imbalanced: one or two workers can inherit the last giant
    // subgraphs while every other assigned CPU goes idle.  Ordered BDD edges
    // always point from a lower variable to a higher one.  Bucket the existing
    // dense IDs by variable in the remap scratch, then propagate marks one
    // variable level at a time.  Each level is independent, exact, and fully
    // parallel; the scratch is reused for the final old-to-new ID map.
    std::vector<std::atomic<std::uint64_t>> levelCounts(
      impl_->limits.variables);
    for (auto& count : levelCounts)
        count.store(0, std::memory_order_relaxed);
    parallel_chunks(sourceCount > 2 ? sourceCount - 2 : 0, 1ULL << 20,
      [&](std::uint32_t, std::uint64_t begin, std::uint64_t end) {
          for (std::uint64_t offset = begin; offset < end; ++offset) {
              const Node value = impl_->get(static_cast<Id>(offset + 2));
              levelCounts[value.variable].fetch_add(
                1, std::memory_order_relaxed);
          }
      });
    std::vector<std::uint64_t> levelBegin(impl_->limits.variables + 1, 0);
    for (std::uint32_t variable = 0; variable < impl_->limits.variables;
         ++variable)
        levelBegin[variable + 1] = levelBegin[variable] +
          levelCounts[variable].load(std::memory_order_relaxed);
    if (levelBegin.back() != sourceCount - 2)
        throw std::runtime_error("compaction variable census residual");
    std::vector<std::atomic<std::uint64_t>> levelCursor(
      impl_->limits.variables);
    for (std::uint32_t variable = 0; variable < impl_->limits.variables;
         ++variable)
        levelCursor[variable].store(levelBegin[variable],
                                    std::memory_order_relaxed);
    parallel_chunks(sourceCount > 2 ? sourceCount - 2 : 0, 1ULL << 20,
      [&](std::uint32_t, std::uint64_t begin, std::uint64_t end) {
          for (std::uint64_t offset = begin; offset < end; ++offset) {
              const Id id = static_cast<Id>(offset + 2);
              const Node value = impl_->get(id);
              mapped[levelCursor[value.variable].fetch_add(
                1, std::memory_order_relaxed)] = id;
          }
      });

    std::atomic<std::uint64_t> markedNodes{0};
    parallel_chunks(roots.size(), 1ULL << 16,
      [&](std::uint32_t, std::uint64_t begin, std::uint64_t end) {
          for (std::uint64_t index = begin; index < end; ++index)
              if (claim_mark(roots[index]))
                  markedNodes.fetch_add(1, std::memory_order_relaxed);
      });
    for (std::uint32_t variable = 0; variable < impl_->limits.variables;
         ++variable) {
        const std::uint64_t count = levelBegin[variable + 1] -
                                    levelBegin[variable];
        parallel_chunks(count, 1ULL << 16,
          [&](std::uint32_t, std::uint64_t begin, std::uint64_t end) {
              for (std::uint64_t offset = begin; offset < end; ++offset) {
                  const Id id = mapped[levelBegin[variable] + offset];
                  if (!marked(id))
                      continue;
                  const Node value = impl_->get(id);
                  if (claim_mark(value.low))
                      markedNodes.fetch_add(1, std::memory_order_relaxed);
                  if (claim_mark(value.high))
                      markedNodes.fetch_add(1, std::memory_order_relaxed);
              }
          });
    }
    certificate.markedNodes = markedNodes.load(std::memory_order_relaxed);

    // Marking intentionally tolerates random source-node reads.  Drop those
    // clean pages before the ordered copy so the copy has a bounded streaming
    // source working set rather than retaining the full old arena.
    impl_->nodes.discard(0, std::uint64_t(sourceCount) * NodeBytes);

    mapped[False] = False;
    mapped[True] = True;
    const Id fixedNodes = impl_->limits.variables + 2;
    for (Id id = 2; id < fixedNodes; ++id)
        mapped[id] = id;
    constexpr std::uint64_t IdChunk = 1ULL << 20;
    const std::uint64_t compactable = sourceCount > fixedNodes
                                    ? sourceCount - fixedNodes : 0;
    const std::uint64_t chunkCount =
      (compactable + IdChunk - 1) / IdChunk;
    std::vector<std::uint64_t> chunkOffsets(chunkCount + 1, fixedNodes);
    parallel_chunks(chunkCount, 1,
      [&](std::uint32_t, std::uint64_t begin, std::uint64_t end) {
          for (std::uint64_t chunkIndex = begin; chunkIndex < end;
               ++chunkIndex) {
              const std::uint64_t first = fixedNodes + chunkIndex * IdChunk;
              const std::uint64_t last = std::min<std::uint64_t>(
                sourceCount, first + IdChunk);
              std::uint64_t count = 0;
              for (std::uint64_t id = first; id < last; ++id)
                  count += marked(static_cast<Id>(id));
              chunkOffsets[chunkIndex + 1] = count;
          }
      });
    for (std::uint64_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
        chunkOffsets[chunkIndex + 1] += chunkOffsets[chunkIndex];
    const std::uint64_t finalCount = chunkOffsets.back();
    if (finalCount > impl_->limits.maxNodes)
        throw std::runtime_error("compaction dense node count overflow");
    parallel_chunks(chunkCount, 1,
      [&](std::uint32_t, std::uint64_t begin, std::uint64_t end) {
          for (std::uint64_t chunkIndex = begin; chunkIndex < end;
               ++chunkIndex) {
              const std::uint64_t first = fixedNodes + chunkIndex * IdChunk;
              const std::uint64_t last = std::min<std::uint64_t>(
                sourceCount, first + IdChunk);
              Id output = static_cast<Id>(chunkOffsets[chunkIndex]);
              for (std::uint64_t id = first; id < last; ++id)
                  if (marked(static_cast<Id>(id)))
                      mapped[id] = output++;
              if (output != chunkOffsets[chunkIndex + 1])
                  throw std::runtime_error("compaction ID prefix residual");
          }
      });

    auto replacement = std::make_unique<ExternalRobdd>(
      replacementPrefix, impl_->limits, true);
    std::atomic<std::uint64_t> structuralResidual{0};
    parallel_chunks(compactable, IdChunk,
      [&](std::uint32_t, std::uint64_t begin, std::uint64_t end) {
          for (std::uint64_t offset = begin; offset < end; ++offset) {
              const Id oldId = static_cast<Id>(fixedNodes + offset);
              if (!marked(oldId))
                  continue;
              const Node old = impl_->get(oldId);
              if ((old.low > True && !mapped[old.low]) ||
                  (old.high > True && !mapped[old.high]))
                  throw std::runtime_error(
                    "compaction reachable child lacks a dense ID");
              const Id copied = mapped[oldId];
              std::uint8_t* target = replacement->impl_->nodes.data() +
                std::uint64_t(copied) * NodeBytes;
              target[0] = old.variable;
              store_u32(target + 1, mapped[old.low]);
              store_u32(target + 5, mapped[old.high]);
              structuralResidual.fetch_add(
                copied < fixedNodes || mapped[old.low] >= copied ||
                mapped[old.high] >= copied ||
                mapped[old.low] == mapped[old.high],
                std::memory_order_relaxed);
          }
      });
    replacement->impl_->count.store(static_cast<Id>(finalCount),
                                     std::memory_order_release);

    // The replacement constructor already bound the canonical variable
    // nodes. Bind the copied exact tuples in parallel without invoking make(),
    // whose append dependency forced the former serial copy. Since remapping
    // is order-preserving and injective, any duplicate tuple is a residual.
    std::atomic<std::uint64_t> duplicateResidual{0};
    parallel_chunks(finalCount - fixedNodes, IdChunk,
      [&](std::uint32_t, std::uint64_t begin, std::uint64_t end) {
          for (std::uint64_t offset = begin; offset < end; ++offset) {
              const Id id = static_cast<Id>(fixedNodes + offset);
              const Node value = replacement->impl_->get(id);
              std::uint64_t slot = replacement->impl_->node_hash(
                value.variable, value.low, value.high) &
                (replacement->impl_->limits.uniqueSlots - 1);
              bool bound = false;
              for (std::uint64_t probes = 0;
                   probes < replacement->impl_->limits.uniqueSlots;
                   ++probes) {
                  std::lock_guard<std::mutex> lock(
                    replacement->impl_->uniqueMutexes[
                      slot & (Impl::UniqueMutexStripes - 1)]);
                  Impl::UniqueSlot& entry = replacement->impl_->slots[slot];
                  if (!entry) {
                      entry = id;
                      bound = true;
                      break;
                  }
                  const Node existing = replacement->impl_->get(entry);
                  if (existing.variable == value.variable &&
                      existing.low == value.low &&
                      existing.high == value.high) {
                      duplicateResidual.fetch_add(1,
                        std::memory_order_relaxed);
                      bound = true;
                      break;
                  }
                  slot = (slot + 1) &
                    (replacement->impl_->limits.uniqueSlots - 1);
              }
              if (!bound)
                  throw std::runtime_error(
                    "compaction replacement unique table is full");
          }
      });
    certificate.structuralResidual =
      structuralResidual.load(std::memory_order_relaxed) +
      duplicateResidual.load(std::memory_order_relaxed);
    certificate.copiedNodes = certificate.markedNodes -
      marked(False) - marked(True);
    impl_->nodes.discard(0, std::uint64_t(sourceCount) * NodeBytes);
    for (Id& root : roots) {
        const Id old = root;
        root = mapped[old];
        certificate.rootResidual += old > True && root <= True;
    }
    if (certificate.structuralResidual || certificate.rootResidual)
        throw std::runtime_error("external ROBDD compaction residual is nonzero");
    replacement->flush();
    return {std::move(replacement), certificate};
}

std::uint64_t ExternalRobdd::required_bytes(const Limits& limits) {
    return Impl::required_bytes(limits);
}

}  // namespace Stockfish::Ultimate
