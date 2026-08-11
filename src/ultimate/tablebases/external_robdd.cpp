/*
  Ultimate Fish - exact external-memory reduced ordered decision diagrams
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "external_robdd.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
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
        unique(prefix + ".unique",
          limits.uniqueSlots * sizeof(UniqueSlot), create, true),
        prefix(prefix) {
        slots = reinterpret_cast<UniqueSlot*>(unique.data());
        if (create) {
            count = 0;
            append_raw({static_cast<std::uint8_t>(limits.variables), 0, 0});
            append_raw({static_cast<std::uint8_t>(limits.variables), 1, 1});
            variables.resize(limits.variables);
            for (std::uint32_t variable = 0; variable < limits.variables;
                 ++variable)
                variables[variable] = make(variable, False, True);
        }
        else
            throw std::runtime_error(
              "opening an existing external ROBDD is not implemented");
        applyCache.reserve(std::min<std::size_t>(
          limits.applyCacheEntries, 1'000'000));
        notCache.reserve(std::min<std::size_t>(
          limits.unaryCacheEntries, 500'000));
        composeCache.reserve(std::min<std::size_t>(
          limits.composeCacheEntries, 500'000));
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
        if (id >= count)
            throw std::runtime_error("external ROBDD node ID is out of range");
        const std::uint8_t* source = nodes.data() + std::uint64_t(id) * NodeBytes;
        return {source[0], load_u32(source + 1), load_u32(source + 5)};
    }

    void append_raw(Node value) {
        if (count >= limits.maxNodes)
            throw std::runtime_error(
              "external ROBDD exhausted its exact node allocation");
        std::uint8_t* target = nodes.data() + std::uint64_t(count) * NodeBytes;
        target[0] = value.variable;
        store_u32(target + 1, value.low);
        store_u32(target + 5, value.high);
        ++count;
    }

    [[nodiscard]] std::uint64_t node_hash(std::uint32_t variable, Id low,
                                          Id high) const {
        return mix64(std::uint64_t(variable) * 0x9e3779b97f4a7c15ULL ^
                     (std::uint64_t(low) << 32) ^ high);
    }

    [[nodiscard]] Id make(std::uint32_t variable, Id low, Id high) {
        if (low == high)
            return low;
        if (variable >= limits.variables || low >= count || high >= count)
            throw std::runtime_error("invalid external ROBDD node tuple");
        const auto child_variable = [&](Id child) {
            return child <= True ? limits.variables : get(child).variable;
        };
        if (child_variable(low) <= variable || child_variable(high) <= variable)
            throw std::runtime_error("external ROBDD variable order violation");
        std::uint64_t slot = node_hash(variable, low, high) &
                             (limits.uniqueSlots - 1);
        for (std::uint64_t probes = 0; probes < limits.uniqueSlots; ++probes) {
            UniqueSlot& entry = slots[slot];
            if (!entry) {
                if (count >= limits.maxNodes)
                    throw std::runtime_error(
                      "external ROBDD exact node budget exhausted");
                const Id id = count;
                append_raw({static_cast<std::uint8_t>(variable), low, high});
                entry = id;
                return id;
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
        if (const auto found = applyCache.find(key); found != applyCache.end())
            return found->second;
        const std::uint32_t variable = std::min(top(lhs), top(rhs));
        const Id result = make(variable,
          apply(operation, branch(lhs, variable, false),
                           branch(rhs, variable, false)),
          apply(operation, branch(lhs, variable, true),
                           branch(rhs, variable, true)));
        if (applyCache.size() < limits.applyCacheEntries)
            applyCache.emplace(key, result);
        return result;
    }

    [[nodiscard]] Id negate(Id root) {
        if (root == False) return True;
        if (root == True) return False;
        if (const auto found = notCache.find(root); found != notCache.end())
            return found->second;
        const Node value = get(root);
        const Id result = make(value.variable, negate(value.low),
                               negate(value.high));
        if (notCache.size() < limits.unaryCacheEntries)
            notCache.emplace(root, result);
        return result;
    }

    [[nodiscard]] Id compose(Id root, const std::vector<Id>& image,
                             std::uint64_t relation) {
        if (root <= True)
            return root;
        const ComposeKey key{relation, root};
        if (const auto found = composeCache.find(key);
            found != composeCache.end())
            return found->second;
        const Node value = get(root);
        const Id low = compose(value.low, image, relation);
        const Id high = compose(value.high, image, relation);
        const Id condition = image.at(value.variable);
        const Id result = apply(1, apply(0, condition, high),
          apply(0, negate(condition), low));
        if (composeCache.size() < limits.composeCacheEntries)
            composeCache.emplace(key, result);
        return result;
    }

    Limits limits;
    Mapping nodes;
    Mapping unique;
    UniqueSlot* slots = nullptr;
    std::string prefix;
    std::uint32_t count = 0;
    std::vector<Id> variables;
    std::unordered_map<ApplyKey, Id, ApplyHash> applyCache;
    std::unordered_map<Id, Id> notCache;
    std::unordered_map<ComposeKey, Id, ComposeHash> composeCache;

    void seal_for_streaming_compaction() {
        applyCache.clear();
        notCache.clear();
        composeCache.clear();
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
std::uint32_t ExternalRobdd::node_count() const { return impl_->count; }
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
        if (logical_and(lowRoot, logical_not(highRoot)) != False)
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
        if (logical_and(highRoot, logical_not(lowRoot)) != False)
            return false;
    }
    return true;
}

void ExternalRobdd::clear_computed_caches() {
    impl_->applyCache.clear();
    impl_->notCache.clear();
    impl_->composeCache.clear();
}
void ExternalRobdd::flush() {
    impl_->nodes.flush();
    impl_->unique.flush();
}

std::pair<std::unique_ptr<ExternalRobdd>, ExternalRobdd::CompactionCertificate>
ExternalRobdd::compact(const std::string& replacementPrefix,
                       const std::string& remapPath,
                       std::vector<Id>& roots) {
    CompactionCertificate certificate;
    certificate.roots = roots.size();
    impl_->seal_for_streaming_compaction();
    const std::uint64_t remapBytes = std::uint64_t(impl_->count) * sizeof(Id);
    Mapping remap(remapPath, remapBytes, true, true);
    auto* mapped = reinterpret_cast<Id*>(remap.data());
    const std::uint64_t markBytes = (std::uint64_t(impl_->count) + 7) / 8;
    Mapping marks(remapPath + ".marks", markBytes, true, true);
    const auto marked = [&](Id id) {
        return (marks.data()[id / 8] >> (id % 8)) & 1u;
    };
    const auto set_mark = [&](Id id) {
        marks.data()[id / 8] |= static_cast<std::uint8_t>(1u << (id % 8));
    };
    std::vector<Id> stack;
    for (const Id root : roots) {
        if (root >= impl_->count)
            throw std::runtime_error("compaction root is out of range");
        stack.push_back(root);
        while (!stack.empty()) {
            const Id current = stack.back();
            stack.pop_back();
            if (marked(current))
                continue;
            set_mark(current);
            ++certificate.markedNodes;
            if (current > True) {
                const Node value = impl_->get(current);
                stack.push_back(value.low);
                stack.push_back(value.high);
            }
        }
    }

    // Marking intentionally tolerates random source-node reads.  Drop those
    // clean pages before the ordered copy so the copy has a bounded streaming
    // source working set rather than retaining the full old arena.
    impl_->nodes.discard(0, std::uint64_t(impl_->count) * NodeBytes);

    auto replacement = std::make_unique<ExternalRobdd>(
      replacementPrefix, impl_->limits, true);
    mapped[False] = False;
    mapped[True] = True;
    constexpr std::uint64_t DiscardChunkBytes = 64ULL << 20;
    std::uint64_t discardCursor = 0;
    for (Id id = 2; id < impl_->count; ++id) {
        if (!marked(id))
            continue;
        const Node old = impl_->get(id);
        const Id copied = replacement->make(
          old.variable, mapped[old.low], mapped[old.high]);
        mapped[id] = copied;
        const Node fresh = replacement->node(copied);
        certificate.structuralResidual +=
          fresh.variable != old.variable || fresh.low != mapped[old.low] ||
          fresh.high != mapped[old.high];
        ++certificate.copiedNodes;
        const std::uint64_t consumed = std::uint64_t(id + 1) * NodeBytes;
        if (consumed - discardCursor >= DiscardChunkBytes) {
            impl_->nodes.discard(discardCursor, consumed - discardCursor);
            discardCursor = consumed;
        }
    }
    if (discardCursor < std::uint64_t(impl_->count) * NodeBytes)
        impl_->nodes.discard(discardCursor,
          std::uint64_t(impl_->count) * NodeBytes - discardCursor);
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
