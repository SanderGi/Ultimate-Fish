/*
  Ultimate Fish - exact K+Jester+Ghost versus K information solver
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.

  The solver deliberately lives outside the normal build until its very large
  proof has been resource-gated.  It is nevertheless a complete proof kernel:
  transition certificates are exhaustive, the belief domain is the full
  correlated 160-bit powerset, and every lower-material edge is delegated to a
  SHA-bound exact oracle.  No belief cap or horizon exists in this file.
*/

#include "jester_ghost_information_solver.h"

#include "external_robdd.h"
#include "ghost_information_probe.h"
#include "information.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace Stockfish::Ultimate::JesterGhostInformation {
namespace {

constexpr std::uint8_t Squares = Position::BoardSquares;
constexpr std::uint32_t NoIndex = std::numeric_limits<std::uint32_t>::max();
constexpr ProductRobdd::Id LeafTag = ProductRobdd::Id{1} << 63;
constexpr ProductRobdd::Id LeafPayload = LeafTag - 1;
constexpr char TransitionMagic[8] = {'U','F','J','G','T','2','\0','\0'};
constexpr char OverlayMagic[8] = {'U','F','I','W','2','\0','\0','\0'};
constexpr std::uint32_t TransitionVersion = 2;

[[noreturn]] void system_error(const std::string& operation,
                               const std::string& path) {
    throw std::runtime_error(operation + " " + path + ": " +
                             std::strerror(errno));
}

[[nodiscard]] bool valid_sha256(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(),
      [](unsigned char character) {
          return (character >= '0' && character <= '9') ||
                 (character >= 'a' && character <= 'f');
      });
}

void require_hash(const std::string& value, const char* name) {
    if (!valid_sha256(value))
        throw std::invalid_argument(std::string(name) +
                                    " is not lowercase SHA-256");
}

template<typename Value>
void write_value(std::ostream& output, const Value& value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

template<typename Value>
[[nodiscard]] Value read_value(std::istream& input) {
    Value result{};
    input.read(reinterpret_cast<char*>(&result), sizeof(result));
    if (!input)
        throw std::runtime_error("truncated exact Jester/Ghost certificate");
    return result;
}

[[nodiscard]] std::uint64_t file_bytes(const std::string& path) {
    struct stat status{};
    if (::stat(path.c_str(), &status))
        system_error("cannot stat", path);
    return static_cast<std::uint64_t>(status.st_size);
}

// Small self-contained SHA-256, copied structurally from the already audited
// tablebase generators so the standalone artifact has no shell/tooling trust
// dependency.
class Sha256 {
  public:
    void update(const void* bytes, std::size_t count) {
        const auto* source = static_cast<const std::uint8_t*>(bytes);
        total_ += count;
        while (count) {
            const std::size_t take = std::min(count, block_.size() - used_);
            std::memcpy(block_.data() + used_, source, take);
            source += take;
            count -= take;
            used_ += take;
            if (used_ == block_.size()) {
                transform(block_.data());
                used_ = 0;
            }
        }
    }

    [[nodiscard]] std::array<std::uint8_t, 32> finish() {
        const std::uint64_t bits = total_ * 8;
        block_[used_++] = 0x80;
        if (used_ > 56) {
            std::fill(block_.begin() + used_, block_.end(), 0);
            transform(block_.data());
            used_ = 0;
        }
        std::fill(block_.begin() + used_, block_.begin() + 56, 0);
        for (unsigned byte = 0; byte < 8; ++byte)
            block_[63 - byte] = static_cast<std::uint8_t>(bits >> (8 * byte));
        transform(block_.data());
        std::array<std::uint8_t, 32> result{};
        for (unsigned word = 0; word < state_.size(); ++word)
            for (unsigned byte = 0; byte < 4; ++byte)
                result[word * 4 + byte] = static_cast<std::uint8_t>(
                  state_[word] >> (24 - 8 * byte));
        return result;
    }

  private:
    static std::uint32_t rotate(std::uint32_t value, unsigned count) {
        return (value >> count) | (value << (32 - count));
    }
    void transform(const std::uint8_t* block) {
        static constexpr std::array<std::uint32_t, 64> constants{{
          0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
          0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
          0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
          0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
          0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
          0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
          0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
          0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2}};
        std::array<std::uint32_t, 64> words{};
        for (unsigned index = 0; index < 16; ++index)
            words[index] = (std::uint32_t(block[index * 4]) << 24) |
              (std::uint32_t(block[index * 4 + 1]) << 16) |
              (std::uint32_t(block[index * 4 + 2]) << 8) |
              block[index * 4 + 3];
        for (unsigned index = 16; index < 64; ++index) {
            const std::uint32_t s0 = rotate(words[index - 15], 7) ^
              rotate(words[index - 15], 18) ^ (words[index - 15] >> 3);
            const std::uint32_t s1 = rotate(words[index - 2], 17) ^
              rotate(words[index - 2], 19) ^ (words[index - 2] >> 10);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }
        auto [a,b,c,d,e,f,g,h] = state_;
        for (unsigned index = 0; index < 64; ++index) {
            const std::uint32_t s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
            const std::uint32_t choice = (e & f) ^ (~e & g);
            const std::uint32_t first = h + s1 + choice + constants[index] + words[index];
            const std::uint32_t s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t second = s0 + majority;
            h = g; g = f; f = e; e = d + first;
            d = c; c = b; b = a; a = first + second;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }
    std::array<std::uint32_t, 8> state_{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    std::array<std::uint8_t, 64> block_{};
    std::size_t used_ = 0;
    std::uint64_t total_ = 0;
};

[[nodiscard]] std::string hex_digest(
  const std::array<std::uint8_t, 32>& digest) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::uint8_t byte : digest)
        output << std::setw(2) << unsigned(byte);
    return output.str();
}

[[nodiscard]] std::string sha256_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot hash " + path);
    Sha256 hash;
    std::array<char, 1 << 20> buffer{};
    while (input) {
        input.read(buffer.data(), buffer.size());
        if (input.gcount() > 0)
            hash.update(buffer.data(), static_cast<std::size_t>(input.gcount()));
    }
    return hex_digest(hash.finish());
}

[[nodiscard]] std::string sha256_range(const std::string& path,
                                       std::uint64_t offset,
                                       std::uint64_t count) {
    std::ifstream input(path, std::ios::binary);
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) throw std::runtime_error("cannot seek while hashing " + path);
    Sha256 hash;
    std::array<char, 1 << 20> buffer{};
    while (count) {
        const std::size_t take = static_cast<std::size_t>(
          std::min<std::uint64_t>(count, buffer.size()));
        input.read(buffer.data(), static_cast<std::streamsize>(take));
        if (input.gcount() != static_cast<std::streamsize>(take))
            throw std::runtime_error("truncated hash range " + path);
        hash.update(buffer.data(), take);
        count -= take;
    }
    return hex_digest(hash.finish());
}

void copy_digest(std::array<char,64>& output, const std::string& digest,
                 const char* label) {
    require_hash(digest, label);
    std::copy(digest.begin(), digest.end(), output.begin());
}

[[nodiscard]] std::array<char,64> arbitrary_semantics() {
    std::array<char,64> value{};
    constexpr char text[] = "king-jester-x-hidden-ghost-correlated-v1";
    std::copy(std::begin(text), std::end(text) - 1, value.begin());
    return value;
}

[[nodiscard]] std::uint64_t mix64(std::uint64_t value) {
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

template<typename Value>
class MmapFile {
  public:
    MmapFile() = default;
    MmapFile(const std::string& path, std::uint64_t count, bool create,
             bool readOnly = false) {
        open(path, count, create, readOnly);
    }
    ~MmapFile() { close(); }
    MmapFile(MmapFile&& other) noexcept { swap(other); }
    MmapFile& operator=(MmapFile&& other) noexcept {
        if (this != &other) { close(); swap(other); }
        return *this;
    }
    MmapFile(const MmapFile&) = delete;
    MmapFile& operator=(const MmapFile&) = delete;

    void open(const std::string& path, std::uint64_t count, bool create,
              bool readOnly = false) {
        close();
        if (!count || count > std::numeric_limits<std::size_t>::max() /
                              sizeof(Value))
            throw std::runtime_error("invalid mmap extent for " + path);
        path_ = path;
        count_ = count;
        bytes_ = count * sizeof(Value);
        int flags = readOnly ? O_RDONLY : O_RDWR;
        if (create) flags |= O_CREAT | O_TRUNC;
        descriptor_ = ::open(path.c_str(), flags, 0600);
        if (descriptor_ < 0) system_error("cannot open", path);
        if (create && ::ftruncate(descriptor_, static_cast<off_t>(bytes_)))
            system_error("cannot size", path);
        struct stat status{};
        if (::fstat(descriptor_, &status) ||
            static_cast<std::uint64_t>(status.st_size) != bytes_)
            throw std::runtime_error("mmap extent mismatch for " + path);
        const int protection = readOnly ? PROT_READ : PROT_READ | PROT_WRITE;
        data_ = static_cast<Value*>(::mmap(nullptr, bytes_, protection,
          MAP_SHARED, descriptor_, 0));
        if (data_ == MAP_FAILED) { data_ = nullptr; system_error("cannot mmap", path); }
    }
    [[nodiscard]] Value& operator[](std::uint64_t index) {
        if (index >= count_) throw std::out_of_range("mmap index");
        return data_[index];
    }
    [[nodiscard]] const Value& operator[](std::uint64_t index) const {
        if (index >= count_) throw std::out_of_range("mmap index");
        return data_[index];
    }
    [[nodiscard]] Value* begin() { return data_; }
    [[nodiscard]] Value* end() { return data_ + count_; }
    [[nodiscard]] std::uint64_t size() const { return count_; }
    void fill(Value value) { std::fill(begin(), end(), value); }
    void flush() {
        if (data_ && ::msync(data_, bytes_, MS_SYNC))
            system_error("cannot flush", path_);
    }
  private:
    void close() noexcept {
        if (data_) ::munmap(data_, bytes_);
        if (descriptor_ >= 0) ::close(descriptor_);
        data_ = nullptr; descriptor_ = -1; bytes_ = count_ = 0;
    }
    void swap(MmapFile& other) noexcept {
        std::swap(path_, other.path_); std::swap(descriptor_, other.descriptor_);
        std::swap(data_, other.data_); std::swap(bytes_, other.bytes_);
        std::swap(count_, other.count_);
    }
    std::string path_;
    int descriptor_ = -1;
    Value* data_ = nullptr;
    std::uint64_t bytes_ = 0;
    std::uint64_t count_ = 0;
};

#pragma pack(push, 1)
struct UpperNode {
    std::uint8_t variable = 0;
    ProductRobdd::Id low = 0;
    ProductRobdd::Id high = 0;
};
#pragma pack(pop)
static_assert(sizeof(UpperNode) == 17);

struct ApplyKey {
    std::uint8_t operation = 0;
    ProductRobdd::Id lhs = 0;
    ProductRobdd::Id rhs = 0;
    friend bool operator==(const ApplyKey& a, const ApplyKey& b) {
        return a.operation == b.operation && a.lhs == b.lhs && a.rhs == b.rhs;
    }
};
struct ApplyHash {
    std::size_t operator()(const ApplyKey& key) const {
        return static_cast<std::size_t>(mix64(key.lhs ^
          (mix64(key.rhs) << 1) ^ key.operation));
    }
};
struct UnaryKey {
    ProductRobdd::Id root = 0;
    std::uint64_t relation = 0;
    friend bool operator==(const UnaryKey& a, const UnaryKey& b) {
        return a.root == b.root && a.relation == b.relation;
    }
};
struct UnaryHash {
    std::size_t operator()(const UnaryKey& key) const {
        return static_cast<std::size_t>(mix64(key.root ^ mix64(key.relation)));
    }
};

[[nodiscard]] ExternalRobdd::Limits lower_limits(
  const ProductRobdd::Limits& limits, const std::string& prefix) {
    (void)prefix;
    ExternalRobdd::Limits result;
    result.variables = Squares;
    result.maxNodes = limits.lowerMaxNodes;
    result.uniqueSlots = limits.lowerUniqueSlots;
    result.applyCacheEntries = limits.lowerApplyCacheEntries;
    result.unaryCacheEntries = limits.lowerUnaryCacheEntries;
    result.composeCacheEntries = limits.lowerComposeCacheEntries;
    result.budgetBytes = limits.budgetBytes;
    return result;
}

} // namespace

class ProductRobdd::Impl {
  public:
    Impl(const std::string& prefix, Limits limits, bool create)
      : prefix(std::move(prefix)), limits(limits),
        nodes(this->prefix + ".upper.nodes", limits.maxUpperNodes, create),
        unique(this->prefix + ".upper.unique", limits.upperUniqueSlots, create),
        lower(std::make_unique<ExternalRobdd>(this->prefix + ".lower",
          lower_limits(limits, this->prefix), create)) {
        if (!create)
            throw std::runtime_error(
              "ProductRobdd reload requires a checkpoint root manifest");
        if (!limits.maxUpperNodes || !limits.upperUniqueSlots ||
            (limits.upperUniqueSlots & (limits.upperUniqueSlots - 1)) ||
            !limits.lowerMaxNodes || !limits.lowerUniqueSlots)
            throw std::invalid_argument("invalid ProductRobdd limits");
        unique.fill(Invalid);
        apply.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
          limits.upperCacheEntries, 1'000'000)));
        unary.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
          limits.upperCacheEntries, 1'000'000)));
    }

    [[nodiscard]] static bool leaf(Id id) { return (id & LeafTag) != 0; }
    [[nodiscard]] static Id make_leaf(ExternalRobdd::Id id) {
        return LeafTag | id;
    }
    [[nodiscard]] static ExternalRobdd::Id leaf_id(Id id) {
        if (!leaf(id) || (id & LeafPayload) >
              std::numeric_limits<ExternalRobdd::Id>::max())
            throw std::runtime_error("invalid ProductRobdd leaf");
        return static_cast<ExternalRobdd::Id>(id & LeafPayload);
    }
    [[nodiscard]] const UpperNode& node(Id id) const {
        if (leaf(id) || id >= count)
            throw std::runtime_error("invalid ProductRobdd upper node");
        return nodes[id];
    }
    [[nodiscard]] std::uint8_t top(Id id) const {
        return leaf(id) ? Squares : node(id).variable;
    }
    [[nodiscard]] Id make(std::uint8_t variable, Id low, Id high) {
        if (variable >= Squares)
            throw std::out_of_range("upper ProductRobdd variable");
        if (low == high) return low;
        const std::uint64_t hash = mix64(std::uint64_t(variable) ^
          mix64(low) ^ (mix64(high) << 1));
        const std::uint64_t mask = limits.upperUniqueSlots - 1;
        for (std::uint64_t probe = 0; probe < limits.upperUniqueSlots; ++probe) {
            Id& slot = unique[(hash + probe) & mask];
            if (slot == Invalid) {
                if (count >= limits.maxUpperNodes)
                    throw std::runtime_error("ProductRobdd upper node gate exceeded");
                nodes[count] = {variable, low, high};
                slot = count;
                return count++;
            }
            const UpperNode& existing = node(slot);
            if (existing.variable == variable && existing.low == low &&
                existing.high == high)
                return slot;
        }
        throw std::runtime_error("ProductRobdd upper unique table is full");
    }
    [[nodiscard]] Id binary(std::uint8_t operation, Id lhs, Id rhs) {
        if (operation == 0) {
            if (lhs == make_leaf(ExternalRobdd::False) ||
                rhs == make_leaf(ExternalRobdd::False))
                return make_leaf(ExternalRobdd::False);
            if (lhs == make_leaf(ExternalRobdd::True)) return rhs;
            if (rhs == make_leaf(ExternalRobdd::True)) return lhs;
        } else {
            if (lhs == make_leaf(ExternalRobdd::True) ||
                rhs == make_leaf(ExternalRobdd::True))
                return make_leaf(ExternalRobdd::True);
            if (lhs == make_leaf(ExternalRobdd::False)) return rhs;
            if (rhs == make_leaf(ExternalRobdd::False)) return lhs;
        }
        if (lhs == rhs) return lhs;
        if (lhs > rhs) std::swap(lhs, rhs);
        const ApplyKey key{operation, lhs, rhs};
        if (const auto found = apply.find(key); found != apply.end())
            return found->second;
        Id result;
        if (leaf(lhs) && leaf(rhs)) {
            const ExternalRobdd::Id low = leaf_id(lhs), high = leaf_id(rhs);
            result = make_leaf(operation == 0
              ? lower->logical_and(low, high) : lower->logical_or(low, high));
        } else {
            const std::uint8_t variable = std::min(top(lhs), top(rhs));
            const auto split = [&](Id root, bool high) {
                if (top(root) != variable) return root;
                return high ? node(root).high : node(root).low;
            };
            result = make(variable,
              binary(operation, split(lhs, false), split(rhs, false)),
              binary(operation, split(lhs, true), split(rhs, true)));
        }
        if (apply.size() < limits.upperCacheEntries)
            apply.emplace(key, result);
        return result;
    }
    [[nodiscard]] Id negate(Id root) {
        const UnaryKey key{root, 0};
        if (const auto found = unary.find(key); found != unary.end())
            return found->second;
        const Id result = leaf(root)
          ? make_leaf(lower->logical_not(leaf_id(root)))
          : make(node(root).variable, negate(node(root).low),
                 negate(node(root).high));
        if (unary.size() < limits.upperCacheEntries)
            unary.emplace(key, result);
        return result;
    }
    [[nodiscard]] Id ternary(Id condition, Id yes, Id no) {
        return binary(1, binary(0, condition, yes),
                         binary(0, negate(condition), no));
    }
    [[nodiscard]] Id compose_lower(ExternalRobdd::Id root,
      const std::array<Id, ProductVariables>& image,
      std::unordered_map<ExternalRobdd::Id, Id>& memo) {
        if (root <= ExternalRobdd::True) return make_leaf(root);
        if (const auto found = memo.find(root); found != memo.end())
            return found->second;
        const ExternalRobdd::Node source = lower->node(root);
        const Id result = ternary(image[Squares + source.variable],
          compose_lower(source.high, image, memo),
          compose_lower(source.low, image, memo));
        memo.emplace(root, result);
        return result;
    }
    [[nodiscard]] Id compose(Id root,
      const std::array<Id, ProductVariables>& image,
      std::uint64_t relation) {
        const UnaryKey key{root, relation + 2};
        if (const auto found = unary.find(key); found != unary.end())
            return found->second;
        Id result;
        if (leaf(root)) {
            std::unordered_map<ExternalRobdd::Id, Id> memo;
            result = compose_lower(leaf_id(root), image, memo);
        } else {
            const UpperNode source = node(root);
            result = ternary(image[source.variable],
              compose(source.high, image, relation),
              compose(source.low, image, relation));
        }
        if (unary.size() < limits.upperCacheEntries)
            unary.emplace(key, result);
        return result;
    }
    [[nodiscard]] bool eval(Id root, const ProductMask& assignment) const {
        while (!leaf(root)) {
            const UpperNode source = node(root);
            root = assignment.test(source.variable) ? source.high : source.low;
        }
        const std::uint64_t suffixLow = (assignment.words[1] >> 16) |
                                        (assignment.words[2] << 48);
        const std::uint16_t suffixHigh = static_cast<std::uint16_t>(
          assignment.words[2] >> 16);
        return lower->evaluate(leaf_id(root), suffixLow, suffixHigh);
    }

    std::string prefix;
    Limits limits;
    MmapFile<UpperNode> nodes;
    MmapFile<Id> unique;
    std::unique_ptr<ExternalRobdd> lower;
    std::uint64_t count = 0;
    std::unordered_map<ApplyKey, Id, ApplyHash> apply;
    std::unordered_map<UnaryKey, Id, UnaryHash> unary;
};

ProductRobdd::ProductRobdd(const std::string& prefix, Limits limits,
                           bool create)
  : impl_(new Impl(prefix, limits, create)) {}
ProductRobdd::~ProductRobdd() { delete impl_; }
ProductRobdd::ProductRobdd(ProductRobdd&& other) noexcept
  : impl_(std::exchange(other.impl_, nullptr)) {}
ProductRobdd& ProductRobdd::operator=(ProductRobdd&& other) noexcept {
    if (this != &other) { delete impl_; impl_ = std::exchange(other.impl_, nullptr); }
    return *this;
}
ProductRobdd::Id ProductRobdd::constant(bool value) const {
    return Impl::make_leaf(value ? ExternalRobdd::True : ExternalRobdd::False);
}
ProductRobdd::Id ProductRobdd::variable(unsigned variableIndex) {
    if (variableIndex >= ProductVariables)
        throw std::out_of_range("ProductRobdd variable");
    if (variableIndex < Squares)
        return impl_->make(static_cast<std::uint8_t>(variableIndex),
          constant(false), constant(true));
    return Impl::make_leaf(impl_->lower->variable(variableIndex - Squares));
}
ProductRobdd::Id ProductRobdd::logical_not(Id root) { return impl_->negate(root); }
ProductRobdd::Id ProductRobdd::logical_and(Id a, Id b) { return impl_->binary(0,a,b); }
ProductRobdd::Id ProductRobdd::logical_or(Id a, Id b) { return impl_->binary(1,a,b); }
ProductRobdd::Id ProductRobdd::ite(Id c, Id y, Id n) { return impl_->ternary(c,y,n); }
ProductRobdd::Id ProductRobdd::any(const ProductMask& variables) {
    Id result = constant(false);
    for (unsigned variableIndex = 0; variableIndex < ProductVariables; ++variableIndex)
        if (variables.test(variableIndex))
            result = logical_or(result, variable(variableIndex));
    return result;
}
ProductRobdd::Id ProductRobdd::subset_of(const ProductMask& variables) {
    Id result = constant(true);
    for (unsigned variableIndex = 0; variableIndex < ProductVariables; ++variableIndex)
        if (!variables.test(variableIndex))
            result = logical_and(result, logical_not(variable(variableIndex)));
    return result;
}
ProductRobdd::Id ProductRobdd::compose(
  Id root, const std::array<Id, ProductVariables>& image,
  std::uint64_t relationId) {
    return impl_->compose(root, image, relationId);
}
std::vector<ProductRobdd::Id> ProductRobdd::import_suffix(
  const std::vector<SuffixNode>& nodes) {
    if (nodes.size() < 2 || nodes[0].variable != Squares ||
        nodes[1].variable != Squares || nodes[0].low || nodes[0].high ||
        nodes[1].low != 1 || nodes[1].high != 1)
        throw std::invalid_argument("invalid suffix ROBDD terminals");
    std::vector<Id> result(nodes.size());
    result[0] = constant(false); result[1] = constant(true);
    for (std::uint32_t id = 2; id < nodes.size(); ++id) {
        const SuffixNode& source = nodes[id];
        if (source.variable >= Squares || source.low >= id || source.high >= id ||
            source.low == source.high)
            throw std::invalid_argument("invalid suffix ROBDD tuple");
        const ExternalRobdd::Id low = Impl::leaf_id(result[source.low]);
        const ExternalRobdd::Id high = Impl::leaf_id(result[source.high]);
        const ExternalRobdd::Id copied = impl_->lower->make(
          source.variable, low, high);
        const ExternalRobdd::Node check = impl_->lower->node(copied);
        if (check.variable != source.variable || check.low != low ||
            check.high != high)
            throw std::runtime_error("suffix ROBDD import residual");
        result[id] = Impl::make_leaf(copied);
    }
    return result;
}
bool ProductRobdd::evaluate(Id root, const ProductMask& assignment) const {
    return impl_->eval(root, assignment);
}
bool ProductRobdd::is_upward_closed(Id root, const ProductMask& allowed) {
    std::unordered_map<Id, bool> memo;
    std::function<bool(Id)> check = [&](Id current) -> bool {
        if (Impl::leaf(current)) {
            const std::uint64_t lowAllowed = (allowed.words[1] >> 16) |
                                             (allowed.words[2] << 48);
            const std::uint16_t highAllowed = static_cast<std::uint16_t>(
              allowed.words[2] >> 16);
            return impl_->lower->is_upward_closed(
              Impl::leaf_id(current), lowAllowed, highAllowed);
        }
        if (const auto found = memo.find(current); found != memo.end())
            return found->second;
        const UpperNode node = impl_->node(current);
        bool result = check(node.low) && check(node.high);
        if (result && allowed.test(node.variable))
            result = logical_and(node.low, logical_not(node.high)) == constant(false);
        memo.emplace(current, result);
        return result;
    };
    return check(root);
}
bool ProductRobdd::is_downward_closed(Id root) {
    std::unordered_map<Id, bool> memo;
    std::function<bool(Id)> check = [&](Id current) -> bool {
        if (Impl::leaf(current))
            return impl_->lower->is_downward_closed(
              Impl::leaf_id(current), ~std::uint64_t{0}, 0xffff);
        if (const auto found = memo.find(current); found != memo.end())
            return found->second;
        const UpperNode node = impl_->node(current);
        const bool result = check(node.low) && check(node.high) &&
          logical_and(node.high, logical_not(node.low)) == constant(false);
        memo.emplace(current, result);
        return result;
    };
    return check(root);
}
std::uint64_t ProductRobdd::upper_node_count() const { return impl_->count; }
std::uint32_t ProductRobdd::lower_node_count() const {
    return impl_->lower->node_count();
}
ProductRobdd::UpperNodeRecord ProductRobdd::upper_node_record(Id id) const {
    const UpperNode& node = impl_->node(id);
    return {node.variable, node.low, node.high};
}
ProductRobdd::SuffixNode ProductRobdd::lower_node_record(
  std::uint32_t id) const {
    const ExternalRobdd::Node node = impl_->lower->node(id);
    return {node.variable, node.low, node.high};
}

std::uint64_t ProductRobdd::required_bytes(const Limits& limits) {
    const std::uint64_t upper = limits.maxUpperNodes * sizeof(UpperNode) +
      limits.upperUniqueSlots * sizeof(Id);
    return upper + ExternalRobdd::required_bytes(lower_limits(limits, {}));
}

std::pair<ProductRobdd, ProductRobdd::CompactionCertificate>
ProductRobdd::compact(const std::string& replacementPrefix,
                      const std::string& remapPath,
                      std::vector<Id>& roots) {
    ProductRobdd target(replacementPrefix, impl_->limits, true);
    MmapFile<Id> upperRemap(remapPath + ".upper", std::max<std::uint64_t>(1,
      impl_->count), true);
    upperRemap.fill(Invalid);
    MmapFile<std::uint32_t> lowerRemap(remapPath + ".lower",
      std::max<std::uint32_t>(1, impl_->lower->node_count()), true);
    lowerRemap.fill(std::numeric_limits<std::uint32_t>::max());
    lowerRemap[0] = 0; lowerRemap[1] = 1;
    std::uint64_t structural = 0;
    std::function<ExternalRobdd::Id(ExternalRobdd::Id)> copyLower =
      [&](ExternalRobdd::Id source) -> ExternalRobdd::Id {
        if (lowerRemap[source] != std::numeric_limits<std::uint32_t>::max())
            return lowerRemap[source];
        const ExternalRobdd::Node node = impl_->lower->node(source);
        const ExternalRobdd::Id result = target.impl_->lower->make(node.variable,
          copyLower(node.low), copyLower(node.high));
        lowerRemap[source] = result;
        const ExternalRobdd::Node copied = target.impl_->lower->node(result);
        structural += copied.variable != node.variable ||
          copied.low != lowerRemap[node.low] || copied.high != lowerRemap[node.high];
        return result;
      };
    std::function<Id(Id)> copy = [&](Id source) -> Id {
        if (Impl::leaf(source)) return Impl::make_leaf(copyLower(Impl::leaf_id(source)));
        if (upperRemap[source] != Invalid) return upperRemap[source];
        const UpperNode node = impl_->node(source);
        const Id result = target.impl_->make(node.variable,
          copy(node.low), copy(node.high));
        upperRemap[source] = result;
        const UpperNode& copied = target.impl_->node(result);
        structural += copied.variable != node.variable ||
          copied.low != copy(node.low) || copied.high != copy(node.high);
        return result;
    };
    for (Id& root : roots) root = copy(root);
    upperRemap.flush(); lowerRemap.flush();
    CompactionCertificate certificate;
    certificate.oldUpperNodes = impl_->count;
    certificate.newUpperNodes = target.impl_->count;
    certificate.oldLowerNodes = impl_->lower->node_count();
    certificate.newLowerNodes = target.impl_->lower->node_count();
    certificate.structuralResidual = structural;
    certificate.rootResidual = 0;
    if (structural)
        throw std::runtime_error("ProductRobdd compaction structural residual");
    return {std::move(target), certificate};
}

namespace {

constexpr std::uint32_t PairCount = (Squares - 1) * (Squares - 2) / 2;
constexpr std::uint32_t VisibilityCount = Squares - 2;

[[nodiscard]] std::uint8_t rank_excluding_three(
  std::uint8_t square, std::array<std::uint8_t, 3> occupied) {
    std::sort(occupied.begin(), occupied.end());
    std::uint8_t rank = square;
    for (std::uint8_t used : occupied) rank -= used < square;
    return rank;
}

[[nodiscard]] std::uint8_t unrank_excluding(
  std::uint32_t rank, const std::vector<std::uint8_t>& occupied) {
    for (std::uint8_t square = 0; square < Squares; ++square)
        if (std::find(occupied.begin(), occupied.end(), square) == occupied.end() &&
            rank-- == 0)
            return square;
    throw std::runtime_error("public geometry rank is invalid");
}

[[nodiscard]] std::uint32_t pair_rank(std::uint32_t first,
                                      std::uint32_t second,
                                      std::uint32_t count) {
    if (first >= second || second >= count)
        throw std::invalid_argument("invalid public royal pair rank");
    return first * (2 * count - first - 1) / 2 + second - first - 1;
}

[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> pair_unrank(
  std::uint32_t rank, std::uint32_t count) {
    for (std::uint32_t first = 0; first + 1 < count; ++first) {
        const std::uint32_t width = count - first - 1;
        if (rank < width) return {first, first + 1 + rank};
        rank -= width;
    }
    throw std::runtime_error("public royal pair index is invalid");
}

[[nodiscard]] std::uint32_t encode_geometry(const PublicFrame& frame) {
    if (frame.blackKing >= Squares || frame.royalFirst >= frame.royalSecond ||
        frame.royalSecond >= Squares || frame.blackKing == frame.royalFirst ||
        frame.blackKing == frame.royalSecond)
        throw std::invalid_argument("invalid public Jester/Ghost geometry");
    const std::uint32_t first = frame.royalFirst -
      (frame.royalFirst > frame.blackKing ? 1u : 0u);
    const std::uint32_t second = frame.royalSecond -
      (frame.royalSecond > frame.blackKing ? 1u : 0u);
    std::uint32_t visibility = 0;
    if (frame.visibleGhost) {
        if (*frame.visibleGhost == frame.blackKing ||
            *frame.visibleGhost == frame.royalFirst ||
            *frame.visibleGhost == frame.royalSecond)
            throw std::invalid_argument("visible Ghost overlaps public geometry");
        visibility = 1 + rank_excluding_three(*frame.visibleGhost,
          {frame.blackKing, frame.royalFirst, frame.royalSecond});
    }
    return (((static_cast<std::uint32_t>(frame.side) * Squares +
              frame.blackKing) * PairCount + pair_rank(first, second, Squares - 1)) *
            VisibilityCount + visibility);
}

[[nodiscard]] PublicFrame decode_geometry(std::uint32_t index) {
    if (index >= RawGeometryCount)
        throw std::out_of_range("public Jester/Ghost geometry index");
    const std::uint32_t visibility = index % VisibilityCount;
    index /= VisibilityCount;
    const auto [firstRank, secondRank] = pair_unrank(index % PairCount,
                                                     Squares - 1);
    index /= PairCount;
    const std::uint8_t black = static_cast<std::uint8_t>(index % Squares);
    const Color side = static_cast<Color>(index / Squares);
    const std::uint8_t first = unrank_excluding(firstRank, {black});
    const std::uint8_t second = unrank_excluding(secondRank, {black});
    PublicFrame frame{side, black, std::min(first, second),
                      std::max(first, second), {}};
    if (visibility)
        frame.visibleGhost = unrank_excluding(visibility - 1,
          {black, frame.royalFirst, frame.royalSecond});
    if (encode_geometry(frame) !=
          (((static_cast<std::uint32_t>(side) * Squares + black) * PairCount +
             pair_rank(firstRank, secondRank, Squares - 1)) * VisibilityCount +
           visibility))
        throw std::runtime_error("public geometry codec involution failed");
    return frame;
}

[[nodiscard]] std::pair<std::uint32_t, RectangleTransform>
canonical_geometry(const PublicFrame& frame) {
    std::uint32_t best = encode_geometry(frame);
    RectangleTransform bestTransform = RectangleTransform::Identity;
    ProductMask seed;
    const std::vector<ProductWorld> worlds = geometric_worlds(frame);
    seed.set(product_variable(frame, worlds.front()));
    for (std::uint8_t raw = 1; raw < 4; ++raw) {
        const RectangleTransform transform = static_cast<RectangleTransform>(raw);
        const ProductSet mapped = transform_set(frame, seed, transform);
        const std::uint32_t candidate = encode_geometry(mapped.frame);
        if (candidate < best) { best = candidate; bestTransform = transform; }
    }
    return {best, bestTransform};
}

[[nodiscard]] std::uint8_t terminal_flags(const Position& position) {
    if (!position.game_over())
        throw std::runtime_error("terminal force requested for live position");
    const std::optional<Color> winner = position.winner();
    return !winner ? 0 : *winner == Color::White ? 1 : 2;
}

struct TransitionHeaderDisk {
    std::array<char, 8> magic{};
    std::uint32_t version = TransitionVersion;
    std::uint32_t headerBytes = sizeof(TransitionHeaderDisk);
    std::uint32_t rawBegin = 0;
    std::uint32_t rawCount = 0;
    std::uint32_t rawDomain = RawGeometryCount;
    std::uint32_t complete = 0;
    std::uint64_t geometries = 0;
    std::uint64_t worlds = 0;
    std::uint64_t liveWorlds = 0;
    std::uint64_t ownerRoots = 0;
    std::uint64_t admitted = 0;
    std::uint64_t terminal = 0;
    std::uint64_t actions = 0;
    std::uint64_t observations = 0;
    std::uint64_t edges = 0;
    std::uint64_t codecChecks = 0;
    std::uint64_t actionChecks = 0;
    std::uint64_t decisionChecks = 0;
    std::uint64_t transitionChecks = 0;
    std::uint64_t symmetryChecks = 0;
    std::uint64_t strata = 0;
    std::uint64_t blockBytes = 0;
    std::array<char, 64> sourceSha{};
    std::array<char, 64> modelSha{};
    std::array<char, 64> observationSha{};
    std::array<char, 64> payloadSha{};
};

struct VerifiedDisk {
    std::array<char,8> magic{{'U','F','J','G','V','2','\0','\0'}};
    std::uint32_t version=2;
    std::uint32_t bytes=0;
    std::uint32_t rawBegin=0;
    std::uint32_t rawCount=0;
    std::array<char,64> sourceSha{};
    std::array<char,64> modelSha{};
    std::array<char,64> observationSha{};
    std::array<char,64> payloadSha{};
    std::array<char,64> headerSha{};
};

struct GeometryDisk {
    std::uint32_t raw = 0;
    std::uint32_t reserved = 0;
    std::uint64_t ownerBase = 0;
    std::uint8_t liveCount = 0;
    std::array<std::uint8_t, ProductVariables> ownerOrdinal{};
    std::array<std::uint8_t, 7> alignment{};
    ProductMask live;
    ProductMask terminal;
    ProductMask terminalWhite;
    ProductMask terminalBlack;
    ProductMask admittedFresh;
    std::uint32_t stratumBase = 0;
    std::uint32_t stratumCount = 0;
    std::array<std::uint32_t, ProductVariables> actualStratum{};
};

// The permanent catalog deliberately omits the dense ownerOrdinal and
// actualStratum arrays: owner rank is popcount(live below actual), and the
// decision-cell ordinal is recovered by scanning this geometry's disjoint
// strata. This saves several GiB without losing any information.
struct ArbitraryGeometryDisk {
    std::uint32_t raw = 0;
    std::uint32_t reserved = 0;
    std::uint64_t ownerBase = 0;
    std::uint64_t stratumBase = 0;
    std::uint32_t stratumCount = 0;
    std::uint8_t liveCount = 0;
    std::array<std::uint8_t,3> alignment{};
    ProductMask live;
};

struct ArbitraryUpperNodeDisk {
    std::uint8_t variable = Squares;
    std::array<std::uint8_t,7> reserved{};
    ProductRobdd::Id low = ProductRobdd::Invalid;
    ProductRobdd::Id high = ProductRobdd::Invalid;
};

struct ArbitraryLowerNodeDisk {
    std::uint8_t variable = Squares;
    std::array<std::uint8_t,3> reserved{};
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};

struct ArbitraryHeaderDisk {
    std::array<char,8> magic{{'U','F','J','G','1','\0','\0','\0'}};
    std::uint32_t version = 1;
    std::uint32_t headerBytes = sizeof(ArbitraryHeaderDisk);
    std::uint32_t endian = 0x01020304;
    std::uint32_t primary = static_cast<std::uint32_t>(PieceType::Jester);
    std::uint32_t secondary = static_cast<std::uint32_t>(PieceType::Ghost);
    std::uint32_t owner = static_cast<std::uint32_t>(Color::White);
    std::uint32_t files = Position::BoardFiles;
    std::uint32_t ranks = Position::BoardRanks;
    std::uint32_t squares = Squares;
    std::uint32_t variables = ProductVariables;
    std::uint32_t stateCount = StateCount;
    std::uint32_t upperNodeBytes = sizeof(ArbitraryUpperNodeDisk);
    std::uint32_t lowerNodeBytes = sizeof(ArbitraryLowerNodeDisk);
    std::uint32_t geometryBytes = sizeof(ArbitraryGeometryDisk);
    std::uint32_t maskBytes = sizeof(ProductMask);
    std::uint32_t rootBytes = sizeof(ProductRobdd::Id);
    std::uint32_t reserved = 0;
    std::uint32_t alignment = 0;
    std::uint64_t upperNodes = 0;
    std::uint64_t lowerNodes = 0;
    std::uint64_t geometries = 0;
    std::uint64_t strata = 0;
    std::uint64_t ownerRoots = 0;
    std::uint64_t upperOffset = 0;
    std::uint64_t lowerOffset = 0;
    std::uint64_t geometryOffset = 0;
    std::uint64_t stratumOffset = 0;
    std::uint64_t ownerOffset = 0;
    std::uint64_t observerOffset = 0;
    std::uint64_t payloadBytes = 0;
    std::array<char,64> sourceSha{};
    std::array<char,64> modelSha{};
    std::array<char,64> observationSha{};
    std::array<char,64> transitionPayloadSha{};
    std::array<char,64> transitionHeaderSha{};
    std::array<char,64> transitionMarkerSha{};
    std::array<char,64> lowerJesterOverlaySha{};
    std::array<char,64> lowerGhostSidecarSha{};
    std::array<char,64> payloadSha{};
    std::array<char,64> semantics{};
};
static_assert(sizeof(ArbitraryUpperNodeDisk) == 24);
static_assert(offsetof(ArbitraryUpperNodeDisk, low) == 8);
static_assert(sizeof(ArbitraryLowerNodeDisk) == 12);
static_assert(offsetof(ArbitraryLowerNodeDisk, low) == 4);
static_assert(sizeof(ArbitraryGeometryDisk) == 56);
static_assert(offsetof(ArbitraryGeometryDisk, live) == 32);
static_assert(sizeof(ArbitraryHeaderDisk) == 816);
static_assert(offsetof(ArbitraryHeaderDisk, upperOffset) == 120);
static_assert(offsetof(ArbitraryHeaderDisk, payloadBytes) == 168);
static_assert(offsetof(ArbitraryHeaderDisk, sourceSha) == 176);

struct ActionDisk {
    ActionKey key;
};

struct EdgeDisk {
    CompiledEdge edge;
    std::uint16_t sourceVariable = 0;
    std::uint16_t reserved = 0;
};

struct GeometryBlockHeader {
    std::array<std::uint32_t, ProductVariables + 1> edgeOffsets{};
    std::uint32_t actionCount = 0;
    std::uint32_t edgeCount = 0;
};

[[nodiscard]] std::string combined_payload_sha(const std::string& prefix) {
    Sha256 hash;
    for (const char* suffix : {".meta", ".strata", ".index", ".blocks"}) {
        std::ifstream input(prefix + suffix, std::ios::binary);
        if (!input) throw std::runtime_error("cannot hash transition payload");
        std::array<char, 1 << 20> buffer{};
        while (input) {
            input.read(buffer.data(), buffer.size());
            if (input.gcount() > 0)
                hash.update(buffer.data(), static_cast<std::size_t>(input.gcount()));
        }
    }
    return hex_digest(hash.finish());
}

void write_verified_marker(const std::string& prefix,
                           const TransitionHeaderDisk& header) {
    VerifiedDisk marker;
    marker.bytes=sizeof(marker);marker.rawBegin=header.rawBegin;
    marker.rawCount=header.rawCount;marker.sourceSha=header.sourceSha;
    marker.modelSha=header.modelSha;marker.observationSha=header.observationSha;
    marker.payloadSha=header.payloadSha;
    const std::string headerSha=sha256_file(prefix+".header");
    std::copy(headerSha.begin(),headerSha.end(),marker.headerSha.begin());
    std::ofstream output(prefix+".verified",std::ios::binary|std::ios::trunc);
    write_value(output,marker);
    if(!output)throw std::runtime_error("failed writing exhaustive transition marker");
}

[[nodiscard]] VerifiedDisk read_verified_marker(
  const std::string& prefix,const TransitionHeaderDisk& header) {
    std::ifstream input(prefix+".verified",std::ios::binary);
    const VerifiedDisk marker=read_value<VerifiedDisk>(input);
    if(marker.magic!=std::array<char,8>{'U','F','J','G','V','2',0,0}||
       marker.version!=2||marker.bytes!=sizeof(marker)||
       marker.rawBegin!=header.rawBegin||marker.rawCount!=header.rawCount||
       marker.sourceSha!=header.sourceSha||marker.modelSha!=header.modelSha||
       marker.observationSha!=header.observationSha||
       marker.payloadSha!=header.payloadSha||
       std::string(marker.headerSha.data(),64)!=sha256_file(prefix+".header")||
       input.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("transition shard lacks a valid exhaustive-regeneration marker");
    return marker;
}

void verify_transition_storage(const std::string&prefix,
                               const TransitionHeaderDisk&header){
    if(combined_payload_sha(prefix)!=std::string(header.payloadSha.data(),64))
        throw std::runtime_error("transition payload SHA-256 mismatch");
    if(file_bytes(prefix+".meta")!=header.geometries*sizeof(GeometryDisk)||
       file_bytes(prefix+".strata")!=header.strata*sizeof(ProductMask)||
       file_bytes(prefix+".index")!=(header.geometries+1)*sizeof(std::uint64_t)||
       file_bytes(prefix+".blocks")!=header.blockBytes)
        throw std::runtime_error("transition database extent mismatch");
    std::ifstream indices(prefix+".index",std::ios::binary);
    std::uint64_t previous=read_value<std::uint64_t>(indices);
    if(previous)throw std::runtime_error("transition index does not start at zero");
    for(std::uint64_t id=0;id<header.geometries;++id){const std::uint64_t next=read_value<std::uint64_t>(indices);
        if(next<previous||next>header.blockBytes)throw std::runtime_error("transition index is not monotonic");previous=next;}
    if(previous!=header.blockBytes||indices.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("transition index coverage residual");
}

[[nodiscard]] TransitionHeaderDisk authenticate_transition_database(
  const std::string&prefix,const std::string&sourceSha,
  const std::string&modelSha,const std::string&observationSha,
  bool requireComplete){
    std::ifstream input(prefix+".header",std::ios::binary);
    const TransitionHeaderDisk header=read_value<TransitionHeaderDisk>(input);
    if(header.magic!=std::array<char,8>{'U','F','J','G','T','2',0,0}||
       header.version!=TransitionVersion||header.headerBytes!=sizeof(header)||
       header.rawDomain!=RawGeometryCount||
       (requireComplete&&(!header.complete||header.rawBegin||header.rawCount!=RawGeometryCount))||
       std::string(header.sourceSha.data(),64)!=sourceSha||
       std::string(header.modelSha.data(),64)!=modelSha||
       std::string(header.observationSha.data(),64)!=observationSha||
       header.codecChecks != 4 * (header.worlds + header.geometries) ||
       header.actionChecks != 4 * header.worlds ||
       header.decisionChecks < 4 * (header.strata + header.geometries) ||
       header.transitionChecks != 4 * header.edges ||
       header.symmetryChecks != 4 * header.geometries)
        throw std::runtime_error("transition authenticated header mismatch");
    verify_transition_storage(prefix,header);
    (void)read_verified_marker(prefix,header);
    return header;
}

[[nodiscard]] TransitionCertificate transition_certificate(
  const TransitionHeaderDisk&header){TransitionCertificate result;
    result.rawGeometries=header.rawCount;result.canonicalGeometries=header.geometries;
    result.worlds=header.worlds;result.liveWorlds=header.liveWorlds;
    result.admittedFreshWorlds=header.admitted;result.terminalWorlds=header.terminal;
    result.actions=header.actions;result.observations=header.observations;
    result.edges=header.edges;result.payloadSha256.assign(header.payloadSha.data(),64);
    result.codecChecks=header.codecChecks;result.actionChecks=header.actionChecks;
    result.decisionChecks=header.decisionChecks;
    result.transitionChecks=header.transitionChecks;
    result.symmetryChecks=header.symmetryChecks;
    return result;}

[[nodiscard]] std::optional<Move> find_action(const Position& position,
                                              const ActionKey& action) {
    std::optional<Move> result;
    for (const Move& move : position.legal_moves())
        if (action_key(move) == action) {
            if (result)
                throw std::runtime_error("complete action is not collision-free");
            result = move;
        }
    return result;
}

[[nodiscard]] CompiledEdge encode_child(const Position& child,
  const std::optional<FramedWorld>& physical) {
    const ClassifiedChild classified = classify_child(child);
    CompiledEdge result;
    result.childConcrete = classified.index;
    switch (classified.domain) {
      case ChildDomain::SameClass: {
        if (!physical)
            throw std::runtime_error("same-class child lacks physical product");
        const auto [raw, transform] = canonical_geometry(physical->frame);
        const FramedWorld mapped = transform_world(physical->frame,
          physical->world, transform);
        result.domain = CompiledChildDomain::SameClass;
        result.childGeometry = raw;
        result.childActual = static_cast<std::uint8_t>(
          product_variable(mapped.frame, mapped.world));
        break;
      }
      case ChildDomain::LowerJester:
        result.domain = CompiledChildDomain::LowerJester;
        break;
      case ChildDomain::LowerGhost:
        result.domain = CompiledChildDomain::LowerGhost;
        break;
      case ChildDomain::ExactTerminal:
        result.domain = CompiledChildDomain::ExactTerminal;
        result.terminalForces = terminal_flags(child);
        break;
      default:
        throw std::runtime_error(
          "Jester/Ghost transition escaped all exact closed domains");
    }
    return result;
}

struct BuiltBlock {
    GeometryDisk meta;
    std::vector<ProductMask> strata;
    GeometryBlockHeader header;
    std::vector<ActionDisk> actions;
    std::vector<EdgeDisk> edges;
    std::uint64_t observationCount = 0;
};

[[nodiscard]] ProductMask transform_mask_allow_empty(
  const PublicFrame& frame, const ProductMask& mask,
  RectangleTransform transform, const PublicFrame& expectedFrame) {
    ProductMask result;
    for (unsigned variable = 0; variable < ProductVariables; ++variable) {
        if (!mask.test(variable)) continue;
        const ProductWorld world = decode_product_variable(frame, variable);
        const FramedWorld mapped = transform_world(frame, world, transform);
        if (!(mapped.frame == expectedFrame))
            throw std::runtime_error("D2 mask escaped its transformed frame");
        result.set(product_variable(expectedFrame, mapped.world));
    }
    if (result.count() != mask.count())
        throw std::runtime_error("D2 mask transform is not bijective");
    return result;
}

[[nodiscard]] bool mask_less(const ProductMask& first,
                             const ProductMask& second) {
    return first.words < second.words;
}

[[nodiscard]] BuiltBlock build_block(const PublicFrame& frame,
                                     bool /*symmetryCertificate*/,
                                     TransitionCertificate& certificate) {
    BuiltBlock result;
    result.meta.raw = encode_geometry(frame);
    result.meta.actualStratum.fill(NoIndex);
    result.meta.ownerOrdinal.fill(0xff);
    const std::vector<ProductWorld> worlds = geometric_worlds(frame);
    std::vector<ProductWorld> liveWorlds;
    for (const ProductWorld& world : worlds) {
        const unsigned variable = product_variable(frame, world);
        const Position position = make_position(frame, world);
        if (position.game_over()) {
            result.meta.terminal.set(variable);
            const std::uint8_t flags = terminal_flags(position);
            if (flags & 1) result.meta.terminalWhite.set(variable);
            if (flags & 2) result.meta.terminalBlack.set(variable);
            ++certificate.terminalWorlds;
        }
        else {
            result.meta.live.set(variable);
            liveWorlds.push_back(world);
        }
        if (fresh_world_admission(frame, world) == AdmissionVerdict::Admit) {
            result.meta.admittedFresh.set(variable);
            ++certificate.admittedFreshWorlds;
        }
    }
    for (unsigned variable = 0; variable < ProductVariables; ++variable)
        if (result.meta.live.test(variable))
            result.meta.ownerOrdinal[variable] = result.meta.liveCount++;
    if (result.meta.liveCount != result.meta.live.count())
        throw std::runtime_error("compact owner-root ordinal residual");

    const std::vector<DecisionBucket> decisions = liveWorlds.empty()
      ? std::vector<DecisionBucket>{}
      : decision_partition(frame, liveWorlds);
    for (const DecisionBucket& decision : decisions) {
        if (frame.side == Color::White) {
            // Owner-private cells are singleton but the observer belief before
            // a White action is the complete public set. Use one exact domain
            // stratum; actual ownership is carried by owner roots.
            continue;
        }
        const std::uint32_t stratum = static_cast<std::uint32_t>(result.strata.size());
        result.strata.push_back(decision.worlds);
        for (unsigned variable = 0; variable < ProductVariables; ++variable)
            if (decision.worlds.test(variable))
                result.meta.actualStratum[variable] = stratum;
    }
    if (frame.side == Color::White) {
        const ProductMask nonterminal{
          {result.meta.live.words[0], result.meta.live.words[1],
           result.meta.live.words[2]}};
        result.strata.push_back(nonterminal);
        for (unsigned variable = 0; variable < ProductVariables; ++variable)
            if (nonterminal.test(variable)) result.meta.actualStratum[variable] = 0;
    }
    result.meta.stratumCount = static_cast<std::uint32_t>(result.strata.size());

    std::map<ActionKey, std::vector<ProductWorld>> byAction;
    for (const ProductWorld& world : worlds) {
        const unsigned variable = product_variable(frame, world);
        if (result.meta.terminal.test(variable)) continue;
        const Position position = make_position(frame, world);
        const std::vector<ActionKey> actions = legal_actions(position);
        for (const ActionKey& action : actions) byAction[action].push_back(world);
    }
    std::uint32_t actionId = 0;
    // Relations are keyed by what Black observes, never by White's private
    // complete action. Distinct hidden-Ghost ActionKeys that emit the same
    // observation therefore contribute to one exact image relation. Black
    // cannot condition its successor belief on which private action occurred.
    struct RelationBinding {
        std::uint32_t id = 0;
        CompiledChildDomain domain = CompiledChildDomain::ExactTerminal;
        std::uint32_t childFrame = 0;
    };
    std::map<std::string, RelationBinding> relationIds;
    std::array<std::vector<EdgeDisk>, ProductVariables> sourceEdges;
    for (const auto& [action, legalWorlds] : byAction) {
        result.actions.push_back({action});
        std::map<std::string, std::vector<TransitionWorld>> partitions;
        for (const ProductWorld& sourceWorld : legalWorlds) {
            Position before = make_position(frame, sourceWorld);
            const std::optional<Move> move = find_action(before, action);
            if (!move) throw std::runtime_error("action map contains illegal world");
            Position child = before; Undo undo;
            if (!child.make_move(*move, undo))
                throw std::runtime_error("action map move failed");
            std::string blackObservation = transition_observation_key(
              before, *move, child, {Color::Black, false});
            // The dots are a private pre-decision observation, not public
            // transition data.  They refine only Black's next information
            // cell, but that refined cell is exactly the belief on which its
            // uniform action is chosen.
            if (!child.game_over() && child.side_to_move() == Color::Black)
                blackObservation += decision_observation_key(
                  child, {Color::Black, false});
            const std::string whiteObservation = transition_observation_key(
              before, *move, child, {Color::White, false});
            if (whiteObservation.empty() || blackObservation.empty())
                throw std::runtime_error("empty complete transition observation");
            const ClassifiedChild classified = classify_child(child);
            std::optional<FramedWorld> physical;
            if (classified.domain == ChildDomain::SameClass)
                physical = same_class_product(child);
            partitions[blackObservation].push_back(
              {sourceWorld, blackObservation, classified, physical});
        }
        for (const auto& [partitionKey, observation] : partitions) {
            if (observation.empty())
                throw std::runtime_error("empty transition observation");
            std::optional<CompiledChildDomain> domain;
            std::uint32_t childFrame = 0;
            std::vector<std::pair<unsigned, CompiledEdge>> encoded;
            for (const TransitionWorld& transition : observation) {
                const unsigned source = product_variable(frame, transition.source);
                Position before = make_position(frame, transition.source);
                const std::optional<Move> move = find_action(before, action);
                if (!move) throw std::runtime_error("partition retained illegal action");
                Position child = before; Undo undo;
                if (!child.make_move(*move, undo))
                    throw std::runtime_error("certified action failed to apply");
                CompiledEdge edge = encode_child(child, transition.sameClassProduct);
                if (!domain) { domain = edge.domain; childFrame = edge.childGeometry; }
                else if (*domain != edge.domain ||
                         (edge.domain == CompiledChildDomain::SameClass &&
                          edge.childGeometry != childFrame))
                    throw std::runtime_error(
                      "one public observation mixes child material/frames");
                encoded.emplace_back(source, edge);
            }
            const auto [binding, inserted] = relationIds.emplace(partitionKey,
              RelationBinding{static_cast<std::uint32_t>(relationIds.size()),
                              *domain, childFrame});
            if (!inserted && (binding->second.domain != *domain ||
                binding->second.childFrame != childFrame))
                throw std::runtime_error(
                  "one complete Black observation mixes child domains");
            const std::uint32_t relation = binding->second.id;
            for (auto [source, edge] : encoded) {
                edge.action = actionId;
                edge.relation = relation;
                sourceEdges[source].push_back({edge,
                  static_cast<std::uint16_t>(source)});
                switch (edge.domain) {
                  case CompiledChildDomain::SameClass: ++certificate.sameClass; break;
                  case CompiledChildDomain::LowerJester: ++certificate.lowerJester; break;
                  case CompiledChildDomain::LowerGhost: ++certificate.lowerGhost; break;
                  case CompiledChildDomain::ExactTerminal: ++certificate.exactTerminal; break;
                }
            }
        }
        ++actionId;
    }
    result.observationCount = relationIds.size();
    result.header.actionCount = result.actions.size();
    for (unsigned source = 0; source < ProductVariables; ++source) {
        result.header.edgeOffsets[source] = result.edges.size();
        result.edges.insert(result.edges.end(), sourceEdges[source].begin(),
                            sourceEdges[source].end());
    }
    result.header.edgeOffsets[ProductVariables] = result.edges.size();
    result.header.edgeCount = result.edges.size();
    return result;
}

struct SemanticTransition {
    unsigned source = 0;
    ActionKey action;
    std::string blackObservation;
    std::string whiteObservation;
    CompiledEdge encoded;
    ClassifiedChild classified;
    std::optional<FramedWorld> physical;
};

using SemanticTransitionKey = std::pair<unsigned, ActionKey>;

[[nodiscard]] std::map<SemanticTransitionKey, SemanticTransition>
semantic_transitions(const PublicFrame& frame, const BuiltBlock& block) {
    if (block.header.actionCount != block.actions.size() ||
        block.header.edgeCount != block.edges.size() ||
        block.header.edgeOffsets[ProductVariables] != block.edges.size())
        throw std::runtime_error(
          "D2 certificate block cardinality/header residual");
    std::map<SemanticTransitionKey, SemanticTransition> result;
    std::map<std::string, std::uint32_t> observationToRelation;
    std::map<std::uint32_t, std::string> relationToObservation;
    std::vector<bool> usedActions(block.actions.size(), false);
    std::array<std::optional<ProductWorld>, ProductVariables> nativeWorlds;
    for (const ProductWorld& world : geometric_worlds(frame))
        nativeWorlds[product_variable(frame, world)] = world;
    for (unsigned source = 0; source < ProductVariables; ++source) {
        if (block.header.edgeOffsets[source] >
            block.header.edgeOffsets[source + 1] ||
            block.header.edgeOffsets[source + 1] > block.edges.size())
            throw std::runtime_error("D2 certificate found invalid edge offsets");
        std::vector<ActionKey> storedLegal;
        for (std::uint32_t ordinal = block.header.edgeOffsets[source];
             ordinal < block.header.edgeOffsets[source + 1]; ++ordinal) {
            const EdgeDisk& stored = block.edges[ordinal];
            if (stored.sourceVariable != source ||
                stored.edge.action >= block.actions.size())
                throw std::runtime_error("D2 certificate found malformed edge");
            const ActionKey action = block.actions[stored.edge.action].key;
            usedActions[stored.edge.action] = true;
            storedLegal.push_back(action);
            const ProductWorld world = decode_product_variable(frame, source);
            Position before = make_position(frame, world);
            const std::optional<Move> move = find_action(before, action);
            if (!move)
                throw std::runtime_error("D2 certificate edge action is illegal");
            Position child = before;
            Undo undo;
            if (!child.make_move(*move, undo))
                throw std::runtime_error("D2 certificate edge failed to apply");
            std::string blackObservation = transition_observation_key(
              before, *move, child, {Color::Black, false});
            if (!child.game_over() && child.side_to_move() == Color::Black)
                blackObservation += decision_observation_key(
                  child, {Color::Black, false});
            const std::string whiteObservation = transition_observation_key(
              before, *move, child, {Color::White, false});
            const auto [byObservation, newObservation] =
              observationToRelation.emplace(blackObservation,
                                             stored.edge.relation);
            const auto [byRelation, newRelation] =
              relationToObservation.emplace(stored.edge.relation,
                                             blackObservation);
            if ((!newObservation && byObservation->second !=
                                    stored.edge.relation) ||
                (!newRelation && byRelation->second != blackObservation))
                throw std::runtime_error(
                  "compiled relation is not the exact Black observation partition");
            const ClassifiedChild classified = classify_child(child);
            std::optional<FramedWorld> physical;
            if (classified.domain == ChildDomain::SameClass)
                physical = same_class_product(child);
            const CompiledEdge expected = encode_child(child, physical);
            if (stored.edge.domain != expected.domain ||
                stored.edge.childGeometry != expected.childGeometry ||
                stored.edge.childConcrete != expected.childConcrete ||
                stored.edge.childActual != expected.childActual ||
                stored.edge.terminalForces != expected.terminalForces)
                throw std::runtime_error(
                  "compiled child differs from native classification");
            SemanticTransition transition{source, action, blackObservation,
              whiteObservation, stored.edge, classified, physical};
            if (!result.emplace(SemanticTransitionKey{source, action},
                                std::move(transition)).second)
                throw std::runtime_error(
                  "D2 certificate found duplicate source/action edge");
        }
        std::sort(storedLegal.begin(), storedLegal.end());
        if (!nativeWorlds[source]) {
            if (!storedLegal.empty())
                throw std::runtime_error(
                  "compiled edge originates at a nongeometric product variable");
            continue;
        }
        const std::vector<ActionKey> nativeLegal = legal_actions(
          make_position(frame, *nativeWorlds[source]));
        if (storedLegal != nativeLegal)
            throw std::runtime_error(
              "compiled source edges are not the complete native legal-action set");
    }
    if (std::find(usedActions.begin(), usedActions.end(), false) !=
          usedActions.end())
        throw std::runtime_error("compiled global action table has an orphan");
    if (observationToRelation.size() != block.observationCount ||
        relationToObservation.size() != block.observationCount)
        throw std::runtime_error(
          "compiled observation relation cardinality residual");
    std::uint32_t relation = 0;
    for (const auto& [id, observation] : relationToObservation) {
        (void)observation;
        if (id != relation++)
            throw std::runtime_error(
              "compiled observation relation IDs are not dense");
    }
    if (result.size() != block.edges.size())
        throw std::runtime_error("compiled transition edge coverage residual");
    return result;
}

[[nodiscard]] std::uint32_t transformed_lower_jester(
  std::uint32_t index, RectangleTransform transform) {
    LowerJesterState state = decode_lower_jester(index);
    state.whiteKing = transform_square(state.whiteKing, transform);
    state.blackKing = transform_square(state.blackKing, transform);
    state.jester = transform_square(state.jester, transform);
    return encode_lower_jester(state);
}

[[nodiscard]] std::uint32_t transformed_lower_ghost(
  std::uint32_t index, RectangleTransform transform) {
    LowerGhostState state = decode_lower_ghost(index);
    state.ownerKing = transform_square(state.ownerKing, transform);
    state.observerKing = transform_square(state.observerKing, transform);
    state.ghost = transform_square(state.ghost, transform);
    return encode_lower_ghost(state);
}

template<typename First, typename Second>
void require_partition_bijection(std::map<First, Second>& forward,
                                 std::map<Second, First>& reverse,
                                 const First& first, const Second& second,
                                 std::uint64_t& residual,
                                 const char* message) {
    const auto [left, insertedLeft] = forward.emplace(first, second);
    const auto [right, insertedRight] = reverse.emplace(second, first);
    if ((!insertedLeft && left->second != second) ||
        (!insertedRight && right->second != first)) {
        ++residual;
        throw std::runtime_error(message);
    }
}

void certify_d2_block_unchecked(const PublicFrame& frame,
                                const BuiltBlock& source,
                                TransitionCertificate& certificate,
                                bool perturbTransition) {
    const std::vector<ProductWorld> sourceWorlds = geometric_worlds(frame);
    std::map<SemanticTransitionKey, SemanticTransition> sourceTransitions;
    try {
        sourceTransitions = semantic_transitions(frame, source);
    }
    catch (...) {
        ++certificate.transitionResidual;
        throw;
    }
    for (std::uint8_t raw = 0; raw < 4; ++raw) {
        const RectangleTransform transform =
          static_cast<RectangleTransform>(raw);
        const FramedWorld firstMapped = transform_world(
          frame, sourceWorlds.front(), transform);
        const PublicFrame& mappedFrame = firstMapped.frame;
        TransitionCertificate ignored;
        BuiltBlock mapped = build_block(mappedFrame, false, ignored);
        if (perturbTransition && raw == 0 && !mapped.edges.empty()) {
            mapped.edges.front().edge.domain =
              mapped.edges.front().edge.domain == CompiledChildDomain::ExactTerminal
                ? CompiledChildDomain::SameClass
                : CompiledChildDomain::ExactTerminal;
        }

        if (!(decode_geometry(encode_geometry(mappedFrame)) == mappedFrame) ||
            mapped.meta.raw != encode_geometry(mappedFrame)) {
            ++certificate.codecResidual;
            throw std::runtime_error("D2 public-frame codec residual");
        }
        std::set<unsigned> mappedVariables;
        for (const ProductWorld& world : sourceWorlds) {
            const unsigned sourceVariable = product_variable(frame, world);
            const FramedWorld transformed = transform_world(frame, world,
                                                             transform);
            const unsigned targetVariable = product_variable(
              mappedFrame, transformed.world);
            const ProductWorld decoded = decode_product_variable(
              mappedFrame, targetVariable);
            const FramedWorld roundTrip = transform_world(
              mappedFrame, transformed.world, transform);
            if (!(transformed.frame == mappedFrame) ||
                !(decoded == transformed.world) ||
                !(roundTrip.frame == frame) || !(roundTrip.world == world) ||
                !mappedVariables.insert(targetVariable).second) {
                ++certificate.codecResidual;
                throw std::runtime_error("D2 product codec/bijection residual");
            }
            ++certificate.codecChecks;

            const Position position = make_position(frame, world);
            std::vector<ActionKey> expected = legal_actions(position);
            for (ActionKey& action : expected)
                action = transform_action(action, transform);
            std::sort(expected.begin(), expected.end());
            const std::vector<ActionKey> actual = legal_actions(
              make_position(mappedFrame, transformed.world));
            if (expected != actual) {
                ++certificate.actionResidual;
                throw std::runtime_error("D2 complete legal-action residual");
            }
            ++certificate.actionChecks;
            (void)sourceVariable;
        }
        if (mappedVariables.size() != sourceWorlds.size()) {
            ++certificate.codecResidual;
            throw std::runtime_error("D2 product variable coverage residual");
        }

        const auto mapMask = [&](const ProductMask& mask) {
            return transform_mask_allow_empty(frame, mask, transform,
                                              mappedFrame);
        };
        if (!(mapMask(source.meta.live) == mapped.meta.live) ||
            !(mapMask(source.meta.terminal) == mapped.meta.terminal) ||
            !(mapMask(source.meta.terminalWhite) == mapped.meta.terminalWhite) ||
            !(mapMask(source.meta.terminalBlack) == mapped.meta.terminalBlack) ||
            !(mapMask(source.meta.admittedFresh) == mapped.meta.admittedFresh)) {
            ++certificate.codecResidual;
            throw std::runtime_error("D2 native-state mask residual");
        }
        ++certificate.codecChecks;

        std::vector<ProductMask> expectedStrata;
        expectedStrata.reserve(source.strata.size());
        for (const ProductMask& stratum : source.strata)
            expectedStrata.push_back(mapMask(stratum));
        std::sort(expectedStrata.begin(), expectedStrata.end(), mask_less);
        std::vector<ProductMask> actualStrata = mapped.strata;
        std::sort(actualStrata.begin(), actualStrata.end(), mask_less);
        if (expectedStrata != actualStrata) {
            ++certificate.decisionResidual;
            throw std::runtime_error("D2 mover-private decision partition residual");
        }
        certificate.decisionChecks += expectedStrata.size() + 1;

        std::vector<ProductWorld> sourceLive, mappedLive;
        for (const ProductWorld& world : sourceWorlds)
            if (source.meta.live.test(product_variable(frame, world))) {
                sourceLive.push_back(world);
                mappedLive.push_back(
                  transform_world(frame, world, transform).world);
            }
        std::vector<ProductMask> expectedDecision, actualDecision;
        for (const DecisionBucket& bucket :
             decision_partition(frame, sourceLive))
            expectedDecision.push_back(mapMask(bucket.worlds));
        for (const DecisionBucket& bucket :
             decision_partition(mappedFrame, mappedLive))
            actualDecision.push_back(bucket.worlds);
        std::sort(expectedDecision.begin(), expectedDecision.end(), mask_less);
        std::sort(actualDecision.begin(), actualDecision.end(), mask_less);
        if (expectedDecision != actualDecision) {
            ++certificate.decisionResidual;
            throw std::runtime_error(
              "D2 native legal-dot decision-cell residual");
        }
        certificate.decisionChecks += expectedDecision.size() + 1;

        std::map<SemanticTransitionKey, SemanticTransition> mappedTransitions;
        try {
            mappedTransitions = semantic_transitions(mappedFrame, mapped);
        }
        catch (...) {
            ++certificate.transitionResidual;
            throw;
        }
        if (mappedTransitions.size() != sourceTransitions.size()) {
            ++certificate.transitionResidual;
            throw std::runtime_error("D2 transition cardinality residual");
        }
        std::map<std::uint32_t, std::uint32_t> relationForward;
        std::map<std::uint32_t, std::uint32_t> relationReverse;
        std::map<std::string, std::string> blackForward, blackReverse;
        std::map<std::string, std::string> whiteForward, whiteReverse;
        for (const auto& [key, transition] : sourceTransitions) {
            const ProductWorld sourceWorld = decode_product_variable(
              frame, transition.source);
            const FramedWorld transformedSource = transform_world(
              frame, sourceWorld, transform);
            const SemanticTransitionKey mappedKey{
              product_variable(mappedFrame, transformedSource.world),
              transform_action(transition.action, transform)};
            const auto found = mappedTransitions.find(mappedKey);
            if (found == mappedTransitions.end()) {
                ++certificate.transitionResidual;
                throw std::runtime_error("D2 transition image is absent");
            }
            const SemanticTransition& target = found->second;
            require_partition_bijection(relationForward, relationReverse,
              transition.encoded.relation, target.encoded.relation,
              certificate.transitionResidual,
              "D2 compiled Black-observation partition residual");
            require_partition_bijection(blackForward, blackReverse,
              transition.blackObservation, target.blackObservation,
              certificate.transitionResidual,
              "D2 complete Black-observation equivalence residual");
            require_partition_bijection(whiteForward, whiteReverse,
              transition.whiteObservation, target.whiteObservation,
              certificate.transitionResidual,
              "D2 complete White-observation equivalence residual");
            if (transition.classified.domain != target.classified.domain) {
                ++certificate.transitionResidual;
                throw std::runtime_error("D2 child domain residual");
            }
            switch (transition.classified.domain) {
              case ChildDomain::SameClass: {
                if (!transition.physical || !target.physical) {
                    ++certificate.transitionResidual;
                    throw std::runtime_error("D2 same-class physical child absent");
                }
                const FramedWorld expected = transform_world(
                  transition.physical->frame, transition.physical->world,
                  transform);
                if (!(expected.frame == target.physical->frame) ||
                    !(expected.world == target.physical->world) ||
                    transition.encoded.childGeometry !=
                      target.encoded.childGeometry) {
                    ++certificate.transitionResidual;
                    throw std::runtime_error(
                      "D2 same-class child frame/actual residual");
                }
                // semantic_transitions() independently proves each stored
                // childActual equals encode_child() of its physical child.
                // Do not compare the two raw actual IDs: when a D2 transform
                // stabilizes the canonical public child frame, the deliberate
                // no-stabilizer-fold convention may permute private product
                // variables while preserving the exact physical mapping.
                break;
              }
              case ChildDomain::LowerJester:
                if (transformed_lower_jester(transition.classified.index,
                                             transform) !=
                    target.classified.index) {
                    ++certificate.transitionResidual;
                    throw std::runtime_error("D2 lower-Jester child residual");
                }
                break;
              case ChildDomain::LowerGhost:
                if (transformed_lower_ghost(transition.classified.index,
                                            transform) !=
                    target.classified.index) {
                    ++certificate.transitionResidual;
                    throw std::runtime_error("D2 lower-Ghost child residual");
                }
                break;
              case ChildDomain::ExactTerminal:
                if (transition.encoded.terminalForces !=
                    target.encoded.terminalForces) {
                    ++certificate.transitionResidual;
                    throw std::runtime_error("D2 terminal-force residual");
                }
                break;
              default:
                ++certificate.transitionResidual;
                throw std::runtime_error("D2 invalid child domain");
            }
            ++certificate.transitionChecks;
        }
        ++certificate.symmetryChecks;
    }
}

void certify_d2_block(const PublicFrame& frame, const BuiltBlock& source,
                      TransitionCertificate& certificate,
                      bool perturbTransition = false) {
    try {
        certify_d2_block_unchecked(frame, source, certificate,
                                   perturbTransition);
    }
    catch (...) {
        ++certificate.symmetryResidual;
        throw;
    }
}

void write_block(std::ostream& output, const BuiltBlock& block) {
    write_value(output, block.header);
    if (!block.actions.empty())
        output.write(reinterpret_cast<const char*>(block.actions.data()),
                     block.actions.size() * sizeof(ActionDisk));
    if (!block.edges.empty())
        output.write(reinterpret_cast<const char*>(block.edges.data()),
                     block.edges.size() * sizeof(EdgeDisk));
}

} // namespace

TransitionCertificate compile_transition_database(
  const TransitionCompileOptions& options) {
    require_hash(options.sourceSha256, "source SHA-256");
    require_hash(options.modelSha256, "model SHA-256");
    require_hash(options.observationSha256, "observation SHA-256");
    if (!options.exhaustiveSymmetryCertificate)
        throw std::invalid_argument(
          "Jester/Ghost shards require the exhaustive D2 certificate");
    if (options.prefix.empty() || options.rawGeometryBegin >= RawGeometryCount)
        throw std::invalid_argument("invalid transition compile range");
    const std::uint32_t remaining = RawGeometryCount - options.rawGeometryBegin;
    const std::uint32_t count = options.rawGeometryCount
      ? std::min(options.rawGeometryCount, remaining) : remaining;
    std::ofstream meta(options.prefix + ".meta", std::ios::binary | std::ios::trunc);
    std::ofstream strata(options.prefix + ".strata", std::ios::binary | std::ios::trunc);
    std::ofstream index(options.prefix + ".index", std::ios::binary | std::ios::trunc);
    std::ofstream blocks(options.prefix + ".blocks", std::ios::binary | std::ios::trunc);
    if (!meta || !strata || !index || !blocks)
        throw std::runtime_error("cannot create Jester/Ghost transition database");
    TransitionCertificate certificate;
    certificate.rawGeometries = count;
    std::uint64_t blockOffset = 0;
    write_value(index, blockOffset);
    std::uint64_t stratumBase = 0;
    std::uint64_t ownerBase = 0;
    for (std::uint32_t offset = 0; offset < count; ++offset) {
        const std::uint32_t raw = options.rawGeometryBegin + offset;
        const PublicFrame frame = decode_geometry(raw);
        if (canonical_geometry(frame).first != raw) continue;
        BuiltBlock block = build_block(frame, false, certificate);
        if (options.exhaustiveSymmetryCertificate)
            certify_d2_block(frame, block, certificate);
        if (stratumBase > std::numeric_limits<std::uint32_t>::max() ||
            block.strata.size() >
              std::numeric_limits<std::uint32_t>::max() - stratumBase)
            throw std::overflow_error(
              "transition stratum address exceeds the on-disk codec");
        block.meta.stratumBase = static_cast<std::uint32_t>(stratumBase);
        block.meta.ownerBase = ownerBase;
        write_value(meta, block.meta);
        if (!block.strata.empty())
            strata.write(reinterpret_cast<const char*>(block.strata.data()),
                         block.strata.size() * sizeof(ProductMask));
        const std::streampos before = blocks.tellp();
        write_block(blocks, block);
        const std::streampos after = blocks.tellp();
        if (before < 0 || after < before)
            throw std::runtime_error("cannot measure transition block");
        blockOffset += static_cast<std::uint64_t>(after - before);
        write_value(index, blockOffset);
        stratumBase += block.strata.size();
        ownerBase += block.meta.liveCount;
        ++certificate.canonicalGeometries;
        certificate.worlds += block.meta.live.count() + block.meta.terminal.count();
        certificate.liveWorlds += block.meta.live.count();
        certificate.actions += block.actions.size();
        certificate.observations += block.observationCount;
        certificate.edges += block.edges.size();
    }
    meta.close(); strata.close(); index.close(); blocks.close();
    TransitionHeaderDisk header;
    std::copy(std::begin(TransitionMagic), std::end(TransitionMagic),
              header.magic.begin());
    header.rawBegin = options.rawGeometryBegin;
    header.rawCount = count;
    header.complete = options.rawGeometryBegin == 0 && count == RawGeometryCount;
    header.geometries = certificate.canonicalGeometries;
    header.worlds = certificate.worlds;
    header.liveWorlds = certificate.liveWorlds;
    header.ownerRoots = ownerBase;
    header.admitted = certificate.admittedFreshWorlds;
    header.terminal = certificate.terminalWorlds;
    header.actions = certificate.actions;
    header.observations = certificate.observations;
    header.edges = certificate.edges;
    header.codecChecks = certificate.codecChecks;
    header.actionChecks = certificate.actionChecks;
    header.decisionChecks = certificate.decisionChecks;
    header.transitionChecks = certificate.transitionChecks;
    header.symmetryChecks = certificate.symmetryChecks;
    header.strata = stratumBase;
    header.blockBytes = blockOffset;
    std::copy(options.sourceSha256.begin(), options.sourceSha256.end(),
              header.sourceSha.begin());
    std::copy(options.modelSha256.begin(), options.modelSha256.end(),
              header.modelSha.begin());
    std::copy(options.observationSha256.begin(), options.observationSha256.end(),
              header.observationSha.begin());
    certificate.payloadSha256 = combined_payload_sha(options.prefix);
    std::copy(certificate.payloadSha256.begin(), certificate.payloadSha256.end(),
              header.payloadSha.begin());
    std::ofstream headerFile(options.prefix + ".header",
      std::ios::binary | std::ios::trunc);
    write_value(headerFile, header);
    if (!headerFile)
        throw std::runtime_error("failed writing transition header");
    headerFile.close();
    const TransitionCertificate verified = verify_transition_database(
      options.prefix, options.sourceSha256, options.modelSha256,
      options.observationSha256, false);
    if (verified.canonicalGeometries != certificate.canonicalGeometries ||
        verified.edges != certificate.edges ||
        verified.codecChecks != certificate.codecChecks ||
        verified.actionChecks != certificate.actionChecks ||
        verified.decisionChecks != certificate.decisionChecks ||
        verified.transitionChecks != certificate.transitionChecks ||
        verified.symmetryChecks != certificate.symmetryChecks ||
        verified.payloadSha256 != certificate.payloadSha256)
        throw std::runtime_error("transition compile/reload certificate mismatch");
    write_verified_marker(options.prefix,header);
    return certificate;
}

TransitionCertificate verify_transition_database(
  const std::string& prefix, const std::string& sourceSha256,
  const std::string& modelSha256, const std::string& observationSha256,
  bool requireComplete) {
    require_hash(sourceSha256, "source SHA-256");
    require_hash(modelSha256, "model SHA-256");
    require_hash(observationSha256, "observation SHA-256");
    std::ifstream input(prefix + ".header", std::ios::binary);
    const TransitionHeaderDisk header = read_value<TransitionHeaderDisk>(input);
    if (header.magic != std::array<char,8>{TransitionMagic[0],TransitionMagic[1],
          TransitionMagic[2],TransitionMagic[3],TransitionMagic[4],TransitionMagic[5],0,0} ||
        header.version != TransitionVersion ||
        header.headerBytes != sizeof(TransitionHeaderDisk) ||
        header.rawDomain != RawGeometryCount ||
        (requireComplete && (!header.complete || header.rawBegin ||
                             header.rawCount != RawGeometryCount)))
        throw std::runtime_error("incompatible transition database header");
    if (std::string(header.sourceSha.data(), 64) != sourceSha256 ||
        std::string(header.modelSha.data(), 64) != modelSha256 ||
        std::string(header.observationSha.data(), 64) != observationSha256)
        throw std::runtime_error("transition database SHA binding mismatch");
    const std::string payload = combined_payload_sha(prefix);
    if (payload != std::string(header.payloadSha.data(), 64))
        throw std::runtime_error("transition payload SHA-256 mismatch");
    verify_transition_storage(prefix,header);
    std::ifstream metas(prefix + ".meta", std::ios::binary);
    std::ifstream strata(prefix + ".strata", std::ios::binary);
    std::ifstream indices(prefix + ".index", std::ios::binary);
    std::ifstream blocks(prefix + ".blocks", std::ios::binary);
    if (!metas || !strata || !indices || !blocks)
        throw std::runtime_error("cannot reopen transition proof payload");
    std::uint64_t previous = read_value<std::uint64_t>(indices);
    if (previous)
        throw std::runtime_error("transition index does not start at zero");
    TransitionCertificate regenerated;
    regenerated.rawGeometries = header.rawCount;
    std::uint64_t ordinal = 0;
    std::uint64_t stratumBase = 0;
    std::uint64_t ownerBase = 0;
    for (std::uint32_t offset = 0; offset < header.rawCount; ++offset) {
        const std::uint32_t raw = header.rawBegin + offset;
        const PublicFrame frame = decode_geometry(raw);
        if (canonical_geometry(frame).first != raw)
            continue;
        if (ordinal >= header.geometries)
            throw std::runtime_error(
              "transition reload has too few canonical geometries");
        BuiltBlock expected = build_block(frame, false, regenerated);
        certify_d2_block(frame, expected, regenerated);
        if (stratumBase > std::numeric_limits<std::uint32_t>::max() ||
            expected.strata.size() >
              std::numeric_limits<std::uint32_t>::max() - stratumBase)
            throw std::overflow_error(
              "regenerated transition stratum address exceeds the codec");
        expected.meta.stratumBase = static_cast<std::uint32_t>(stratumBase);
        expected.meta.ownerBase = ownerBase;
        const GeometryDisk storedMeta = read_value<GeometryDisk>(metas);
        if (std::memcmp(&storedMeta, &expected.meta, sizeof(storedMeta)))
            throw std::runtime_error(
              "transition metadata differs from exhaustive regeneration");
        for (const ProductMask& expectedStratum : expected.strata) {
            const ProductMask stored = read_value<ProductMask>(strata);
            if (!(stored == expectedStratum))
                throw std::runtime_error(
                  "transition stratum differs from exhaustive regeneration");
        }
        const std::uint64_t next = read_value<std::uint64_t>(indices);
        if (next < previous || next > header.blockBytes)
            throw std::runtime_error("transition index is not monotonic");
        blocks.clear();
        blocks.seekg(static_cast<std::streamoff>(previous));
        const GeometryBlockHeader storedHeader =
          read_value<GeometryBlockHeader>(blocks);
        if (std::memcmp(&storedHeader, &expected.header,
                        sizeof(storedHeader)))
            throw std::runtime_error(
              "transition block header differs from regeneration");
        std::vector<ActionDisk> storedActions(storedHeader.actionCount);
        std::vector<EdgeDisk> storedEdges(storedHeader.edgeCount);
        blocks.read(reinterpret_cast<char*>(storedActions.data()),
                    storedActions.size() * sizeof(ActionDisk));
        blocks.read(reinterpret_cast<char*>(storedEdges.data()),
                    storedEdges.size() * sizeof(EdgeDisk));
        if (!blocks || storedActions.size() != expected.actions.size() ||
            storedEdges.size() != expected.edges.size() ||
            (!storedActions.empty() && std::memcmp(storedActions.data(),
              expected.actions.data(), storedActions.size()*sizeof(ActionDisk))) ||
            (!storedEdges.empty() && std::memcmp(storedEdges.data(),
              expected.edges.data(), storedEdges.size()*sizeof(EdgeDisk))) ||
            static_cast<std::uint64_t>(blocks.tellg()) != next)
            throw std::runtime_error(
              "transition actions/edges differ from exhaustive regeneration");
        previous = next;
        stratumBase += expected.strata.size();
        ownerBase += expected.meta.liveCount;
        ++ordinal;
        ++regenerated.canonicalGeometries;
        regenerated.worlds += expected.meta.live.count() +
                              expected.meta.terminal.count();
        regenerated.liveWorlds += expected.meta.live.count();
        regenerated.actions += expected.actions.size();
        regenerated.observations += expected.observationCount;
        regenerated.edges += expected.edges.size();
    }
    if (ordinal != header.geometries || stratumBase != header.strata ||
        ownerBase != header.ownerRoots ||
        previous != header.blockBytes ||
        regenerated.worlds != header.worlds ||
        regenerated.liveWorlds != header.liveWorlds ||
        regenerated.admittedFreshWorlds != header.admitted ||
        regenerated.terminalWorlds != header.terminal ||
        regenerated.actions != header.actions ||
        regenerated.observations != header.observations ||
        regenerated.edges != header.edges ||
        regenerated.codecChecks != header.codecChecks ||
        regenerated.actionChecks != header.actionChecks ||
        regenerated.decisionChecks != header.decisionChecks ||
        regenerated.transitionChecks != header.transitionChecks ||
        regenerated.symmetryChecks != header.symmetryChecks ||
        metas.peek() != std::char_traits<char>::eof() ||
        strata.peek() != std::char_traits<char>::eof() ||
        indices.peek() != std::char_traits<char>::eof())
        throw std::runtime_error(
          "transition global regeneration/conservation residual");
    regenerated.payloadSha256 = payload;
    return regenerated;
}

TransitionCertificate merge_transition_databases(
  const std::vector<std::string>& shardPrefixes,
  const std::string& outputPrefix, const std::string& sourceSha256,
  const std::string& modelSha256, const std::string& observationSha256,
  bool requireComplete) {
    if(shardPrefixes.empty()||outputPrefix.empty())
        throw std::invalid_argument("transition merge needs shards/output");
    struct Shard{std::string prefix;TransitionHeaderDisk header;};
    std::vector<Shard> shards;shards.reserve(shardPrefixes.size());
    for(const std::string&prefix:shardPrefixes){
        if(prefix==outputPrefix)throw std::invalid_argument("transition merge output aliases input");
        std::ifstream input(prefix+".header",std::ios::binary);
        TransitionHeaderDisk header=read_value<TransitionHeaderDisk>(input);
        if(std::string(header.sourceSha.data(),64)!=sourceSha256||
           std::string(header.modelSha.data(),64)!=modelSha256||
           std::string(header.observationSha.data(),64)!=observationSha256)
            throw std::runtime_error("transition shard binding mismatch");
        header=authenticate_transition_database(prefix,sourceSha256,
          modelSha256,observationSha256,false);
        shards.push_back({prefix,header});
    }
    std::sort(shards.begin(),shards.end(),[](const Shard&a,const Shard&b){
        return a.header.rawBegin<b.header.rawBegin;});
    const std::uint32_t mergedBegin=shards.front().header.rawBegin;
    std::uint32_t expectedRaw=mergedBegin;
    for(const Shard&shard:shards){
        if(shard.header.rawBegin!=expectedRaw||
           shard.header.rawCount>RawGeometryCount-expectedRaw)
            throw std::runtime_error("transition shards are not exact gap-free coverage");
        expectedRaw+=shard.header.rawCount;
    }
    if(requireComplete&&(mergedBegin!=0||expectedRaw!=RawGeometryCount))
        throw std::runtime_error("transition shards do not cover full raw domain");
    std::ofstream metas(outputPrefix+".meta",std::ios::binary|std::ios::trunc),
      strata(outputPrefix+".strata",std::ios::binary|std::ios::trunc),
      indices(outputPrefix+".index",std::ios::binary|std::ios::trunc),
      blocks(outputPrefix+".blocks",std::ios::binary|std::ios::trunc);
    if(!metas||!strata||!indices||!blocks)
        throw std::runtime_error("cannot create merged transition database");
    TransitionHeaderDisk merged;std::copy(std::begin(TransitionMagic),
    std::end(TransitionMagic),merged.magic.begin());merged.rawBegin=mergedBegin;
    merged.rawCount=expectedRaw-mergedBegin;
    merged.complete=mergedBegin==0&&expectedRaw==RawGeometryCount;
    std::copy(sourceSha256.begin(),sourceSha256.end(),merged.sourceSha.begin());
    std::copy(modelSha256.begin(),modelSha256.end(),merged.modelSha.begin());
    std::copy(observationSha256.begin(),observationSha256.end(),merged.observationSha.begin());
    std::uint64_t blockBase=0,stratumBase=0,ownerBase=0;write_value(indices,blockBase);
    std::array<char,1<<20>buffer{};
    for(const Shard&shard:shards){
        std::ifstream sourceMeta(shard.prefix+".meta",std::ios::binary),
          sourceStrata(shard.prefix+".strata",std::ios::binary),
          sourceIndex(shard.prefix+".index",std::ios::binary),
          sourceBlocks(shard.prefix+".blocks",std::ios::binary);
        if (read_value<std::uint64_t>(sourceIndex) != 0)
            throw std::runtime_error(
              "transition shard rebase index does not start at zero");
        std::uint64_t localStratumBase=0,localOwnerBase=0,localBlockBase=0;
        for(std::uint64_t id=0;id<shard.header.geometries;++id){
            GeometryDisk meta=read_value<GeometryDisk>(sourceMeta);
            if(meta.stratumBase!=localStratumBase||
               meta.ownerBase!=localOwnerBase)
                throw std::runtime_error(
                  "transition shard contains noncanonical local bases");
            if(stratumBase>std::numeric_limits<std::uint32_t>::max()||
               meta.stratumBase>std::numeric_limits<std::uint32_t>::max()-stratumBase)
                throw std::overflow_error("merged transition stratum address exceeds the codec");
            localStratumBase+=meta.stratumCount;
            localOwnerBase+=meta.liveCount;
            meta.stratumBase=static_cast<std::uint32_t>(meta.stratumBase+stratumBase);
            meta.ownerBase+=ownerBase;write_value(metas,meta);
            const std::uint64_t localEnd=read_value<std::uint64_t>(sourceIndex);
            if(localEnd<localBlockBase||localEnd>shard.header.blockBytes)
                throw std::runtime_error(
                  "transition shard index cannot be rebased exactly");
            localBlockBase=localEnd;
            write_value(indices,blockBase+localEnd);
        }
        if(localStratumBase!=shard.header.strata||
           localOwnerBase!=shard.header.ownerRoots||
           localBlockBase!=shard.header.blockBytes||
           sourceMeta.peek()!=std::char_traits<char>::eof()||
           sourceIndex.peek()!=std::char_traits<char>::eof())
            throw std::runtime_error(
              "transition shard rebase conservation residual");
        while(sourceStrata){sourceStrata.read(buffer.data(),buffer.size());
            if(sourceStrata.gcount()>0)strata.write(buffer.data(),sourceStrata.gcount());}
        while(sourceBlocks){sourceBlocks.read(buffer.data(),buffer.size());
            if(sourceBlocks.gcount()>0)blocks.write(buffer.data(),sourceBlocks.gcount());}
        blockBase+=shard.header.blockBytes;stratumBase+=shard.header.strata;
        ownerBase+=shard.header.ownerRoots;merged.geometries+=shard.header.geometries;
        merged.worlds+=shard.header.worlds;merged.liveWorlds+=shard.header.liveWorlds;
        merged.admitted+=shard.header.admitted;merged.terminal+=shard.header.terminal;
        merged.actions+=shard.header.actions;merged.observations+=shard.header.observations;
        merged.edges+=shard.header.edges;
        merged.codecChecks+=shard.header.codecChecks;
        merged.actionChecks+=shard.header.actionChecks;
        merged.decisionChecks+=shard.header.decisionChecks;
        merged.transitionChecks+=shard.header.transitionChecks;
        merged.symmetryChecks+=shard.header.symmetryChecks;
    }
    merged.strata=stratumBase;merged.ownerRoots=ownerBase;
    merged.blockBytes=blockBase;metas.close();strata.close();indices.close();blocks.close();
    const std::string payload=combined_payload_sha(outputPrefix);
    std::copy(payload.begin(),payload.end(),merged.payloadSha.begin());
    std::ofstream headerFile(outputPrefix+".header",std::ios::binary|std::ios::trunc);
    write_value(headerFile,merged);if(!headerFile)throw std::runtime_error("failed writing merged transition header");headerFile.close();
    // Every input marker was issued only after exhaustive native+D2
    // regeneration. Exact gap-free coverage plus the independently checked
    // base/index rebasing above is therefore a compositional proof; repeating
    // the full single-core native regeneration here is redundant. The public
    // --verify-transitions command remains available for an optional audit.
    write_verified_marker(outputPrefix,merged);
    const TransitionHeaderDisk authenticated=authenticate_transition_database(
      outputPrefix,sourceSha256,modelSha256,observationSha256,requireComplete);
    return transition_certificate(authenticated);
}

namespace {

[[nodiscard]] std::uint64_t checked_add(std::uint64_t first,
                                        std::uint64_t second) {
    if (first > std::numeric_limits<std::uint64_t>::max() - second)
        throw std::overflow_error("Jester/Ghost resource estimate overflow");
    return first + second;
}

[[nodiscard]] std::uint64_t checked_mul(std::uint64_t first,
                                        std::uint64_t second) {
    if (first && second > std::numeric_limits<std::uint64_t>::max() / first)
        throw std::overflow_error("Jester/Ghost resource estimate overflow");
    return first * second;
}

[[nodiscard]] std::uint64_t physical_memory() {
#ifdef __APPLE__
    std::uint64_t bytes = 0; std::size_t size = sizeof(bytes);
    if (::sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0 && bytes)
        return bytes;
#endif
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    const long page = ::sysconf(_SC_PAGESIZE);
    if (pages <= 0 || page <= 0)
        throw std::runtime_error("cannot determine physical memory");
    return checked_mul(static_cast<std::uint64_t>(pages),
                       static_cast<std::uint64_t>(page));
}

[[nodiscard]] ResourceEstimate estimate_resources(
  std::uint64_t geometries, std::uint64_t strata, std::uint64_t worlds,
  std::uint64_t ownerRoots, std::uint64_t transitionBytes,
  const ProductRobdd::Limits& bdd,
  const ResourceLimits& limits) {
    ResourceEstimate result;
    result.canonicalGeometries = geometries;
    result.productWorlds = worlds;
    result.transitionBytes = transitionBytes;
    result.rootBytes = checked_add(
      checked_mul(ownerRoots, 4 * sizeof(ProductRobdd::Id)),
      checked_mul(strata, 3 * sizeof(ProductRobdd::Id)));
    // Root reporting stores exact, collision-free canonical keys.  At most one
    // distinct key can be introduced per concrete realization.  256 bytes per
    // realization safely covers the red/black-tree node, std::string object,
    // allocator bookkeeping, and the largest terminal public-view key used by
    // this closed four-model class on the supported libc++ implementation.
    result.rootCatalogBytes = checked_mul(StateCount, 256);
    result.bddBytes = ProductRobdd::required_bytes(bdd);
    result.compactionBytes = checked_add(
      checked_mul(bdd.maxUpperNodes, sizeof(ProductRobdd::Id)),
      checked_mul(bdd.lowerMaxNodes, sizeof(std::uint32_t)));
    const std::uint64_t arbitraryBytes = checked_add(
      sizeof(ArbitraryHeaderDisk), checked_add(
        checked_mul(bdd.maxUpperNodes, sizeof(ArbitraryUpperNodeDisk)),
        checked_add(
          checked_mul(bdd.lowerMaxNodes, sizeof(ArbitraryLowerNodeDisk)),
          checked_add(
            checked_mul(geometries, sizeof(ArbitraryGeometryDisk)),
            checked_add(checked_mul(strata, sizeof(ProductMask)),
              checked_add(checked_mul(ownerRoots, sizeof(ProductRobdd::Id)),
                          checked_mul(strata,
                                      sizeof(ProductRobdd::Id))))))));
    result.peakDiskBytes = checked_add(transitionBytes,
      checked_add(checked_mul(result.bddBytes, 2),
        checked_add(result.compactionBytes,
          checked_add(result.rootBytes, arbitraryBytes))));
    // The upper unique index and lower ExternalRobdd unique index are random
    // access. Root arrays and bounded caches are also conservatively resident;
    // sequential node/transition pages use a fixed 512 MiB window.
    const std::uint64_t uniqueResident = checked_add(
      checked_mul(bdd.upperUniqueSlots, sizeof(ProductRobdd::Id)),
      checked_mul(bdd.lowerUniqueSlots, sizeof(ExternalRobdd::Id)));
    const std::uint64_t cacheResident = checked_mul(
      checked_add(bdd.upperCacheEntries,
        checked_add(bdd.lowerApplyCacheEntries,
          checked_add(bdd.lowerUnaryCacheEntries,
                      bdd.lowerComposeCacheEntries))), 64);
    result.peakResidentBytes = checked_add(uniqueResident,
      checked_add(result.rootBytes,
        checked_add(result.rootCatalogBytes,
          checked_add(cacheResident, 512ULL << 20))));
    const bool nodeGate = !limits.maxBddNodes ||
      checked_add(bdd.maxUpperNodes, bdd.lowerMaxNodes) <= limits.maxBddNodes;
    result.admitted = nodeGate && limits.maxDiskBytes &&
      limits.maxResidentBytes && result.peakDiskBytes <= limits.maxDiskBytes &&
      result.peakResidentBytes <= limits.maxResidentBytes;
    return result;
}

} // namespace

ResourceEstimate sampled_resource_preflight(
  const TransitionCertificate& sample, std::uint64_t sampledRawGeometries,
  const ProductRobdd::Limits& bdd, const ResourceLimits& limits) {
    if (!sampledRawGeometries || sampledRawGeometries > RawGeometryCount ||
        sample.rawGeometries != sampledRawGeometries ||
        !sample.canonicalGeometries)
        throw std::invalid_argument("invalid sampled resource certificate");
    const auto scale = [&](std::uint64_t value) {
        return (checked_mul(value, RawGeometryCount) +
                sampledRawGeometries - 1) / sampledRawGeometries;
    };
    const std::uint64_t geometries = scale(sample.canonicalGeometries);
    const std::uint64_t worlds = scale(sample.worlds);
    // One domain stratum per White geometry and the exact number of observed
    // decision partitions on Black geometries are not in the public summary;
    // observations are a safe upper bound for the latter.
    const std::uint64_t strata = checked_add(geometries,
                                             scale(sample.observations));
    const std::uint64_t transitionBytes = checked_add(
      checked_mul(geometries,
        sizeof(GeometryDisk) + sizeof(std::uint64_t) + sizeof(GeometryBlockHeader)),
      checked_add(checked_mul(strata, sizeof(ProductMask)),
        checked_add(checked_mul(scale(sample.actions), sizeof(ActionDisk)),
                    checked_mul(scale(sample.edges), sizeof(EdgeDisk)))));
    return estimate_resources(geometries, strata, worlds,
      scale(sample.liveWorlds), transitionBytes, bdd, limits);
}

ResourceEstimate full_domain_preflight(
  const std::string& transitionPrefix, const ProductRobdd::Limits& bdd,
  const ResourceLimits& limits, const std::string& scratchPrefix) {
    std::ifstream input(transitionPrefix + ".header", std::ios::binary);
    const TransitionHeaderDisk header = read_value<TransitionHeaderDisk>(input);
    if (header.magic != std::array<char,8>{TransitionMagic[0],
          TransitionMagic[1], TransitionMagic[2], TransitionMagic[3],
          TransitionMagic[4], TransitionMagic[5], 0, 0} ||
        header.version != TransitionVersion || !header.complete ||
        header.rawBegin || header.rawCount != RawGeometryCount)
        throw std::runtime_error("resource gate requires complete transition proof");
    if (combined_payload_sha(transitionPrefix) !=
          std::string(header.payloadSha.data(), 64))
        throw std::runtime_error("resource gate transition payload mismatch");
    const std::uint64_t transitionBytes = checked_add(
      file_bytes(transitionPrefix + ".header"),
      checked_add(file_bytes(transitionPrefix + ".meta"),
        checked_add(file_bytes(transitionPrefix + ".strata"),
          checked_add(file_bytes(transitionPrefix + ".index"),
                      file_bytes(transitionPrefix + ".blocks")))));
    ResourceEstimate result = estimate_resources(header.geometries,
      header.strata, header.worlds, header.ownerRoots, transitionBytes,
      bdd, limits);
    const std::size_t slash = scratchPrefix.rfind('/');
    const std::string directory = slash == std::string::npos ? "." :
      slash == 0 ? "/" : scratchPrefix.substr(0, slash);
    struct statvfs space{};
    if (::statvfs(directory.c_str(), &space))
        system_error("cannot query scratch free space", directory);
    const std::uint64_t available = checked_mul(space.f_bavail, space.f_frsize);
    const std::uint64_t physical = physical_memory();
    if (available < std::max(limits.minFreeDiskBytes, result.peakDiskBytes) ||
        result.peakResidentBytes > physical * 7 / 10)
        result.admitted = false;
    if (!result.admitted)
        throw std::runtime_error(
          "exact Jester/Ghost solve rejected by RAM/disk/node preflight");
    return result;
}

namespace {

enum class Wdl : std::uint8_t { Invalid = 0, Win = 1, Loss = 2, Draw = 3 };

struct PackedTable {
    std::vector<std::uint8_t> bytes;
    std::size_t plane = 0;
    std::uint32_t count = 0;
    std::string sha;
    [[nodiscard]] Wdl result(std::uint32_t index) const {
        if (index >= count) throw std::out_of_range("packed table index");
        return static_cast<Wdl>((bytes.at(plane + index / 4) >>
          (2 * (index % 4))) & 3);
    }
};

[[nodiscard]] std::uint32_t little_u32(const std::uint8_t* bytes) {
    return bytes[0] | (std::uint32_t(bytes[1]) << 8) |
      (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

[[nodiscard]] PackedTable load_concrete(const std::string& path,
  std::uint32_t expectedCount, PieceType primary, PieceType secondary) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open concrete table " + path);
    input.seekg(0, std::ios::end);
    const std::streamoff end = input.tellg();
    if (end < 56) throw std::runtime_error("concrete table is truncated");
    PackedTable result;
    result.bytes.resize(static_cast<std::size_t>(end));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(result.bytes.data()), result.bytes.size());
    if (!input || std::memcmp(result.bytes.data(), "UFTB1\0\0\0", 8))
        throw std::runtime_error("invalid concrete table magic");
    const std::uint32_t version = little_u32(result.bytes.data() + 8);
    result.count = little_u32(result.bytes.data() + 16);
    const std::uint32_t wdlBytes = little_u32(result.bytes.data() + 28);
    if (version < 5 || version > 7 ||
        little_u32(result.bytes.data() + 12) != static_cast<std::uint32_t>(primary) ||
        result.count != expectedCount || little_u32(result.bytes.data() + 24) != 1 ||
        wdlBytes != (expectedCount + 3) / 4 ||
        little_u32(result.bytes.data() + 40) != static_cast<std::uint32_t>(secondary) ||
        little_u32(result.bytes.data() + 44) != static_cast<std::uint32_t>(Color::White))
        throw std::runtime_error("concrete table material/codec mismatch");
    result.plane = 40 + (version >= 5 ? 8 : 0) +
                   (version >= 6 ? 8 : 0) + (version >= 7 ? 8 : 0);
    if (result.plane + wdlBytes > result.bytes.size())
        throw std::runtime_error("concrete WDL plane is truncated");
    result.sha = sha256_file(path);
    return result;
}

[[nodiscard]] bool wdl_forces(Wdl result, Color side, Color target) {
    return (result == Wdl::Win && side == target) ||
           (result == Wdl::Loss && side != target);
}

class LowerJesterOracle {
  public:
    LowerJesterOracle(const std::string& tablePath,
      const std::string& overlayPath, const std::string& expectedModel,
      const std::string& expectedOverlaySha)
      : concrete_(load_concrete(tablePath, LowerJesterStateCount,
          PieceType::Jester, PieceType::Count)) {
        require_hash(expectedModel, "lower Jester model SHA-256");
        require_hash(expectedOverlaySha, "lower Jester overlay SHA-256");
        if (sha256_file(overlayPath) != expectedOverlaySha)
            throw std::runtime_error("lower Jester overlay full SHA mismatch");
        std::ifstream input(overlayPath, std::ios::binary);
        std::array<char, 160> header{};
        input.read(header.data(), header.size());
        const auto word = [&](std::size_t offset) {
            return little_u32(reinterpret_cast<const std::uint8_t*>(
              header.data() + offset));
        };
        if (!input || std::memcmp(header.data(), OverlayMagic, 8) ||
            word(8) != 2 || word(12) != static_cast<std::uint32_t>(PieceType::Jester) ||
            word(16) != static_cast<std::uint32_t>(PieceType::Count) ||
            word(20) != static_cast<std::uint32_t>(Color::White) ||
            word(24) != LowerJesterStateCount || word(28) != 1 ||
            std::string(header.data() + 32, 64) != concrete_.sha ||
            std::string(header.data() + 96, 64) != expectedModel)
            throw std::runtime_error("lower Jester UFIW2 binding mismatch");
        flags_.resize(LowerJesterStateCount);
        input.read(reinterpret_cast<char*>(flags_.data()), flags_.size());
        if (!input || input.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("lower Jester UFIW2 extent mismatch");
        for(const std::uint8_t flags:flags_)
            if((flags&~std::uint8_t{7})||(!(flags&4)&&(flags&3)))
                throw std::runtime_error("lower Jester UFIW2 malformed flags");
    }
    [[nodiscard]] bool exact(std::uint32_t index, Color target) const {
        return wdl_forces(concrete_.result(index),
                          decode_lower_jester(index).side, target);
    }
    [[nodiscard]] bool pair(std::uint32_t actual, std::uint32_t alternative,
                            Color target) const {
        const LowerJesterSet set = inherited_lower_jester_set({
          {ChildDomain::LowerJester, actual, {}},
          {ChildDomain::LowerJester, alternative, {}}});
        if (set.cardinality != 2)
            throw std::runtime_error("lower royal pair cardinality residual");
        const std::uint8_t flags = flags_.at(actual);
        const std::uint8_t alternativeFlags = flags_.at(alternative);
        if (!(flags & 4) || !(alternativeFlags & 4))
            throw std::runtime_error("lower royal pair outside admitted overlay");
        if ((flags & 2) != (alternativeFlags & 2))
            throw std::runtime_error(
              "lower royal pair disagrees on uninformed force bit");
        return (flags & (target == Color::White ? 1 : 2)) != 0;
    }
  private:
    PackedTable concrete_;
    std::vector<std::uint8_t> flags_;
};

class LowerGhostSidecar {
  public:
    using Node = ProductRobdd::SuffixNode;
    struct Mask { std::uint64_t low = 0; std::uint16_t high = 0; };
    struct Geometry {
        std::uint8_t side=0, ownerKing=0, observerKing=0, visible=0;
        Mask live, terminal, terminalOwner, terminalObserver;
        std::array<std::uint32_t, Squares> actualStratum{};
        std::array<std::uint32_t, Squares> ownerRoot{};
        std::array<std::uint8_t, Squares> visibleOwner{}, visibleObserver{};
    };
    struct Stratum { std::uint32_t geometry=NoIndex; Mask live; std::uint32_t root=0; };

    LowerGhostSidecar(const SolveOptions& options) {
        for (const auto& [hash, name] : std::array<std::pair<std::string,const char*>,3>{{
          {options.lowerGhostSourceSha256,"lower Ghost source SHA-256"},
          {options.lowerGhostModelSha256,"lower Ghost model SHA-256"},
          {options.lowerGhostObservationSha256,"lower Ghost observation SHA-256"}}})
            require_hash(hash, name);
        require_hash(options.lowerGhostSidecarSha256,
                     "lower Ghost sidecar SHA-256");
        if (sha256_file(options.lowerGhostSidecar) !=
              options.lowerGhostSidecarSha256)
            throw std::runtime_error("lower Ghost sidecar full SHA mismatch");
        std::ifstream input(options.lowerGhostSidecar, std::ios::binary);
        auto u8 = [&]() { const int c=input.get(); if(c<0) throw std::runtime_error("truncated UFGM"); return std::uint8_t(c); };
        auto u32 = [&]() { std::array<std::uint8_t,4>b{};input.read(reinterpret_cast<char*>(b.data()),4);if(!input)throw std::runtime_error("truncated UFGM");return little_u32(b.data()); };
        auto u64 = [&]() { const std::uint64_t a=u32(); return a|(std::uint64_t(u32())<<32); };
        auto text = [&](std::size_t n){std::string s(n,'\0');input.read(s.data(),n);if(!input)throw std::runtime_error("truncated UFGM");return s;};
        auto mask = [&](){Mask m;m.low=u64();m.high=std::uint16_t(u8())|(std::uint16_t(u8())<<8);return m;};
        const std::string magic=text(8); const std::uint32_t version=u32(), header=u32();
        const std::uint32_t piece=u32(), owner=u32(), files=u32(), ranks=u32(), squares=u32();
        const std::uint32_t concreteCount=u32(), substates=u32(), geometryCount=u32();
        const std::uint32_t stratumCount=u32(), nodeCount=u32(), nodeBytes=u32();
        const std::uint32_t geometryBytes=u32(), stratumBytes=u32(); (void)u32();
        const std::uint64_t nodeOffset=u64(), geometryOffset=u64(), stratumOffset=u64();
        const std::string source=text(64), model=text(64), observation=text(64), semantics=text(32);
        if (magic != std::string("UFGM1\0\0\0",8) || version != 1 || header != 320 ||
            piece != static_cast<std::uint32_t>(PieceType::Ghost) ||
            owner != static_cast<std::uint32_t>(Color::White) ||
            files != Position::BoardFiles || ranks != Position::BoardRanks || squares != Squares ||
            concreteCount != LowerGhostStateCount || substates != 2 || nodeBytes != 9 ||
            geometryBytes != 844 || stratumBytes != 18 || nodeCount < 2 ||
            source != options.lowerGhostSourceSha256 || model != options.lowerGhostModelSha256 ||
            observation != options.lowerGhostObservationSha256 ||
            semantics.c_str() != std::string("history-mask-public-view-v2") ||
            nodeOffset != 320 || geometryOffset != nodeOffset + std::uint64_t(nodeCount)*9 ||
            stratumOffset != geometryOffset + std::uint64_t(geometryCount)*844)
            throw std::runtime_error("lower Ghost UFGM binding mismatch");
        input.seekg(nodeOffset); nodes_.resize(nodeCount);
        std::set<std::tuple<std::uint8_t,std::uint32_t,std::uint32_t>> unique;
        for (std::uint32_t id=0;id<nodeCount;++id) {
            Node& n=nodes_[id];n.variable=u8();n.low=u32();n.high=u32();
            if (id<2) {
                if(n.variable!=Squares||n.low!=id||n.high!=id)throw std::runtime_error("bad UFGM terminals");
            } else if(n.variable>=Squares||n.low>=id||n.high>=id||n.low==n.high||
                      !unique.emplace(n.variable,n.low,n.high).second)
                throw std::runtime_error("bad UFGM ROBDD tuple");
            if(id>=2){const auto childVariable=[&](std::uint32_t child){return child<2?Squares:nodes_.at(child).variable;};
                if(childVariable(n.low)<=n.variable||childVariable(n.high)<=n.variable)
                    throw std::runtime_error("unordered UFGM ROBDD tuple");}
        }
        input.seekg(geometryOffset); geometries_.resize(geometryCount);
        for(std::uint32_t id=0;id<geometryCount;++id){
            Geometry& g=geometries_[id];g.side=u8();g.ownerKing=u8();g.observerKing=u8();g.visible=u8();
            g.live=mask();g.terminal=mask();g.terminalOwner=mask();g.terminalObserver=mask();
            for(auto&v:g.actualStratum)v=u32();for(auto&v:g.ownerRoot)v=u32();
            input.read(reinterpret_cast<char*>(g.visibleOwner.data()),Squares);
            input.read(reinterpret_cast<char*>(g.visibleObserver.data()),Squares);
            if(!input)throw std::runtime_error("truncated UFGM geometry");
            if(g.side>1||g.ownerKing>=Squares||g.observerKing>=Squares||
               g.ownerKing==g.observerKing||g.visible>1)
                throw std::runtime_error("invalid UFGM geometry");
            for(unsigned actual=0;actual<Squares;++actual){
                if(g.ownerRoot[actual]>=nodeCount||g.visibleOwner[actual]>1||
                   g.visibleObserver[actual]>1)
                    throw std::runtime_error("invalid UFGM force root/value");
                const bool live=test_mask(g.live,actual),terminal=test_mask(g.terminal,actual);
                if(live&&terminal)throw std::runtime_error("UFGM live/terminal overlap");
                if((!live||g.visible)&&g.actualStratum[actual]!=NoIndex)
                    throw std::runtime_error("invalid UFGM stratum reverse map");
            }
            const std::uint32_t code=geometry_code(g.side,g.ownerKing,g.observerKing,g.visible);
            if(!geometryIndex_.emplace(code,id).second)throw std::runtime_error("duplicate UFGM geometry");
        }
        input.seekg(stratumOffset);strata_.resize(stratumCount);
        for(Stratum&s:strata_){s.geometry=u32();s.live=mask();s.root=u32();if(s.geometry>=geometryCount||s.root>=nodeCount)throw std::runtime_error("bad UFGM stratum");}
        if(input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("UFGM trailing bytes");
        for(std::uint32_t gid=0;gid<geometries_.size();++gid){const Geometry&g=geometries_[gid];
            for(unsigned actual=0;actual<Squares;++actual)if(test_mask(g.live,actual)&&!g.visible){
                const std::uint32_t sid=g.actualStratum[actual];
                if(sid==NoIndex||sid>=strata_.size()||strata_[sid].geometry!=gid||
                   !test_mask(strata_[sid].live,actual))
                    throw std::runtime_error("UFGM reverse-map residual");}}
    }
    [[nodiscard]] const std::vector<Node>& nodes() const { return nodes_; }
    [[nodiscard]] std::uint32_t geometry(const LowerGhostState& state) const {
        const std::uint32_t code=geometry_code(static_cast<std::uint8_t>(state.side),
          state.ownerKing,state.observerKing,state.visible);
        const auto found=geometryIndex_.find(code);if(found==geometryIndex_.end())throw std::runtime_error("missing lower Ghost geometry");return found->second;
    }
    [[nodiscard]] const Geometry& geometry(std::uint32_t id) const{return geometries_.at(id);}
    [[nodiscard]] const Stratum& stratum(std::uint32_t id) const{return strata_.at(id);}
  private:
    static bool test_mask(const Mask&m,unsigned square){return square<64?
      (m.low>>square)&1u:(m.high>>(square-64))&1u;}
    static std::uint32_t geometry_code(std::uint8_t side,std::uint8_t owner,
      std::uint8_t observer,bool visible){return side|(std::uint32_t(owner)<<1)|
      (std::uint32_t(observer)<<8)|(std::uint32_t(visible)<<15);}
    std::vector<Node> nodes_;std::vector<Geometry> geometries_;std::vector<Stratum> strata_;
    std::unordered_map<std::uint32_t,std::uint32_t> geometryIndex_;
};

class TransitionDatabase {
  public:
    explicit TransitionDatabase(const std::string& prefix)
      : prefix_(prefix) {
        std::ifstream h(prefix+".header",std::ios::binary);header_=read_value<TransitionHeaderDisk>(h);
        if(!header_.complete||header_.rawCount!=RawGeometryCount)throw std::runtime_error("solver requires complete transition DB");
        meta_.open(prefix+".meta",header_.geometries,false,true);
        strata_.open(prefix+".strata",header_.strata,false,true);
        index_.open(prefix+".index",header_.geometries+1,false,true);
        blocks_.open(prefix+".blocks",std::ios::binary);if(!blocks_)throw std::runtime_error("cannot open transition blocks");
    }
    [[nodiscard]] std::uint64_t geometries()const{return header_.geometries;}
    [[nodiscard]] std::uint64_t strata()const{return header_.strata;}
    [[nodiscard]] std::uint64_t owner_roots()const{return header_.ownerRoots;}
    [[nodiscard]] const GeometryDisk& meta(std::uint64_t id)const{return meta_[id];}
    [[nodiscard]] const ProductMask& stratum(std::uint64_t id)const{return strata_[id];}
    [[nodiscard]] std::uint32_t ordinal(std::uint32_t raw)const{
        std::uint64_t low=0,high=header_.geometries;
        while(low<high){const std::uint64_t mid=(low+high)/2;if(meta_[mid].raw<raw)low=mid+1;else high=mid;}
        if(low>=header_.geometries||meta_[low].raw!=raw)throw std::runtime_error("same-class child absent from complete DB");return static_cast<std::uint32_t>(low);
    }
    struct Block{GeometryBlockHeader header;std::vector<ActionDisk>actions;std::vector<EdgeDisk>edges;};
    [[nodiscard]] Block block(std::uint32_t id){
        if(id>=header_.geometries)throw std::out_of_range("transition block");
        const std::uint64_t begin=index_[id],end=index_[id+1];
        if(begin>end||end>header_.blockBytes)throw std::runtime_error("bad block extent");
        blocks_.clear();blocks_.seekg(begin);Block b;b.header=read_value<GeometryBlockHeader>(blocks_);
        b.actions.resize(b.header.actionCount);b.edges.resize(b.header.edgeCount);
        blocks_.read(reinterpret_cast<char*>(b.actions.data()),b.actions.size()*sizeof(ActionDisk));
        blocks_.read(reinterpret_cast<char*>(b.edges.data()),b.edges.size()*sizeof(EdgeDisk));
        if(!blocks_||b.header.edgeOffsets.front()||b.header.edgeOffsets.back()!=b.edges.size()||
           std::uint64_t(blocks_.tellg())!=end)throw std::runtime_error("malformed transition block");
        return b;
    }
    [[nodiscard]] const TransitionHeaderDisk& header()const{return header_;}
  private:
    std::string prefix_;TransitionHeaderDisk header_{};MmapFile<GeometryDisk>meta_;MmapFile<ProductMask>strata_;MmapFile<std::uint64_t>index_;std::ifstream blocks_;
};

[[nodiscard]] ProductMask mask_and(const ProductMask&a,const ProductMask&b){return {{a.words[0]&b.words[0],a.words[1]&b.words[1],a.words[2]&b.words[2]}};}
[[nodiscard]] bool mask_test(const LowerGhostSidecar::Mask&m,unsigned s){return s<64?(m.low>>s)&1u:(m.high>>(s-64))&1u;}
[[nodiscard]] unsigned lower_ghost_suffix_variable(unsigned ghost){
    if(ghost>=Squares)throw std::out_of_range("lower Ghost suffix square");
    return Squares+ghost;
}

} // namespace

namespace {

struct RuntimeRelation {
    bool initialized=false;
    CompiledChildDomain domain=CompiledChildDomain::ExactTerminal;
    std::uint32_t childGeometry=NoIndex;
    std::uint32_t childStratum=NoIndex;
    bool childTerminal=false;
    ProductMask possibleSources;
    ProductMask badWhiteSources;
    ProductMask badBlackSources;
    std::map<std::uint8_t,ProductMask> image;
    std::map<std::uint32_t,ProductMask> lowerChildren;
};
struct RuntimeActionObservation{std::uint32_t relation=0;ProductMask sources;};
struct RuntimeAction{ProductMask legalSources;std::vector<RuntimeActionObservation>observations;};
struct RuntimeBlock{
    std::array<std::vector<CompiledEdge>,ProductVariables> edges;
    std::vector<RuntimeRelation> relations;std::vector<RuntimeAction>actions;
};

[[nodiscard]] bool lower_terminal_forces(
  const LowerGhostSidecar::Geometry& geometry,
  const LowerGhostState& child, Color target) {
    if (!mask_test(geometry.terminal, child.ghost))
        throw std::runtime_error("lower Ghost terminal specialization is live");
    return mask_test(target == Color::White ? geometry.terminalOwner
                                             : geometry.terminalObserver,
                     child.ghost);
}

[[nodiscard]] ProductRobdd::Id lower_terminal_formula(
  ProductRobdd& bdd, const RuntimeRelation& relation,
  const LowerGhostSidecar::Geometry& geometry, Color target,
  std::optional<std::uint32_t> actual) {
    if (actual) {
        if (relation.lowerChildren.find(*actual) ==
              relation.lowerChildren.end())
            throw std::runtime_error("actual absent from terminal lower Ghost image");
        return bdd.constant(lower_terminal_forces(
          geometry, decode_lower_ghost(*actual), target));
    }
    // The uninformed observer sees only the complete transition observation,
    // so every retained source must force the target outcome. The informed
    // owner branch above instead specializes to its concrete lower child.
    const ProductMask& bad = target == Color::White ?
      relation.badWhiteSources : relation.badBlackSources;
    return bdd.logical_not(bdd.any(bad));
}

[[nodiscard]] std::uint64_t owner_root_index(const GeometryDisk& meta,
                                             unsigned actual) {
    if (actual >= ProductVariables || !meta.live.test(actual) ||
        meta.ownerOrdinal[actual] == 0xff ||
        meta.ownerOrdinal[actual] >= meta.liveCount)
        throw std::runtime_error("invalid compact owner-root coordinate");
    return meta.ownerBase + meta.ownerOrdinal[actual];
}

[[nodiscard]] RuntimeBlock runtime_block(std::uint32_t geometry,
  TransitionDatabase& database,const LowerGhostSidecar& lowerGhost) {
    const TransitionDatabase::Block stored=database.block(geometry);
    RuntimeBlock result;result.actions.resize(stored.actions.size());
    std::uint32_t relationCount=0;
    for(const EdgeDisk& item:stored.edges)relationCount=std::max(relationCount,item.edge.relation+1);
    result.relations.resize(relationCount);
    std::vector<std::map<std::uint32_t,ProductMask>> actionObservations(result.actions.size());
    std::vector<std::vector<bool>> seen(ProductVariables,
      std::vector<bool>(result.actions.size(),false));
    for(const EdgeDisk& item:stored.edges){
        const unsigned source=item.sourceVariable;
        if(source>=ProductVariables||item.edge.action>=result.actions.size())throw std::runtime_error("transition edge ID out of range");
        if(seen[source][item.edge.action])throw std::runtime_error("duplicate action in one product world");
        seen[source][item.edge.action]=true;
        result.edges[source].push_back(item.edge);
        result.actions[item.edge.action].legalSources.set(source);
        actionObservations[item.edge.action][item.edge.relation].set(source);
        RuntimeRelation& relation=result.relations.at(item.edge.relation);
        relation.possibleSources.set(source);
        std::uint32_t childGeometry=NoIndex,childStratum=NoIndex;bool terminal=false;
        if(item.edge.domain==CompiledChildDomain::SameClass){
            childGeometry=database.ordinal(item.edge.childGeometry);
            const GeometryDisk& child=database.meta(childGeometry);
            terminal=child.terminal.test(item.edge.childActual);
            if(!terminal){
                const std::uint32_t local=child.actualStratum[item.edge.childActual];
                if(local==NoIndex||local>=child.stratumCount)throw std::runtime_error("same-class child lacks decision stratum");
                childStratum=child.stratumBase+local;
            }
            relation.image[item.edge.childActual].set(source);
            if(terminal){
                if(!child.terminalWhite.test(item.edge.childActual))relation.badWhiteSources.set(source);
                if(!child.terminalBlack.test(item.edge.childActual))relation.badBlackSources.set(source);
            }
        } else if(item.edge.domain==CompiledChildDomain::LowerGhost){
            const LowerGhostState child=decode_lower_ghost(item.edge.childConcrete);
            childGeometry=lowerGhost.geometry(child);
            const auto& lower=lowerGhost.geometry(childGeometry);
            terminal=mask_test(lower.terminal,child.ghost);
            if(!terminal&&!child.visible){
                childStratum=lower.actualStratum[child.ghost];
                if(childStratum==NoIndex)throw std::runtime_error("lower Ghost child lacks stratum");
            }
            // UFGM variables live in the suffix plane. Both royal-assignment
            // source variables may map to the same lower Ghost square, and
            // their exact union is substituted for suffix variable 80+g.
            relation.image[lower_ghost_suffix_variable(child.ghost)].set(source);
            relation.lowerChildren[item.edge.childConcrete].set(source);
            if(terminal){
                if(!mask_test(lower.terminalOwner,child.ghost))relation.badWhiteSources.set(source);
                if(!mask_test(lower.terminalObserver,child.ghost))relation.badBlackSources.set(source);
            }
        } else if(item.edge.domain==CompiledChildDomain::LowerJester)
            relation.lowerChildren[item.edge.childConcrete].set(source);
        else {
            if(!(item.edge.terminalForces&1))relation.badWhiteSources.set(source);
            if(!(item.edge.terminalForces&2))relation.badBlackSources.set(source);
        }
        if(!relation.initialized){relation.initialized=true;relation.domain=item.edge.domain;
            relation.childGeometry=childGeometry;relation.childStratum=childStratum;relation.childTerminal=terminal;}
        else if(relation.domain!=item.edge.domain||relation.childGeometry!=childGeometry||
                relation.childStratum!=childStratum||relation.childTerminal!=terminal)
            throw std::runtime_error("one observation mixes child epistemic domains");
    }
    for(std::uint32_t action=0;action<result.actions.size();++action)
        for(const auto&[relation,sources]:actionObservations[action])
            result.actions[action].observations.push_back({relation,sources});
    for(const RuntimeRelation& relation:result.relations){
        if(!relation.initialized)throw std::runtime_error("relation IDs are not dense");
        if(relation.domain==CompiledChildDomain::LowerJester&&
           (relation.lowerChildren.empty()||relation.lowerChildren.size()>2))
            throw std::runtime_error("invalid inherited lower Jester image");
        if(relation.domain==CompiledChildDomain::LowerGhost){
            if(relation.lowerChildren.empty())throw std::runtime_error("empty inherited lower Ghost image");
            const LowerGhostState first=decode_lower_ghost(relation.lowerChildren.begin()->first);
            if(first.visible&&relation.image.size()!=1)
                throw std::runtime_error("visible lower Ghost observation is not singleton");
        }
    }
    return result;
}

class ExactKernel {
  public:
    ExactKernel(TransitionDatabase& database,LowerJesterOracle& lowerJester,
      const LowerGhostSidecar& lowerGhost,ProductRobdd& bdd,
      std::vector<ProductRobdd::Id> imported,
      MmapFile<ProductRobdd::Id>& owner,MmapFile<ProductRobdd::Id>& observer,
      MmapFile<ProductRobdd::Id>& domains, SolveCertificate* certificate)
      :db_(database),lj_(lowerJester),lg_(lowerGhost),bdd_(bdd),imported_(std::move(imported)),
       owner_(owner),observer_(observer),domains_(domains),certificate_(certificate){}

    [[nodiscard]] ProductRobdd::Id relation_compose(const RuntimeRelation&r,
      ProductRobdd::Id child,std::uint64_t id){
        std::array<ProductRobdd::Id,ProductVariables> image{};image.fill(bdd_.constant(false));
        for(const auto&[target,sources]:r.image)image[target]=bdd_.any(sources);
        return bdd_.compose(child,image,id);
    }
    [[nodiscard]] ProductRobdd::Id lower_jester_formula(const RuntimeRelation&r,
      Color target,std::optional<std::uint32_t> actual){
        if(r.lowerChildren.empty()||r.lowerChildren.size()>2)throw std::runtime_error("lower Jester image cardinality");
        if(r.lowerChildren.size()==1){const std::uint32_t child=r.lowerChildren.begin()->first;
            if(certificate_)++certificate_->lowerJesterSingletonProbes;
            return bdd_.constant(lj_.exact(child,target));}
        auto first=r.lowerChildren.begin(),second=std::next(first);
        if(actual){
            auto own=r.lowerChildren.find(*actual);if(own==r.lowerChildren.end())throw std::runtime_error("actual absent from lower pair");
            const auto other=own==first?second:first;
            if(certificate_){++certificate_->lowerJesterPairProbes;
                ++certificate_->lowerJesterSingletonProbes;}
            return bdd_.ite(bdd_.any(other->second),
              bdd_.constant(lj_.pair(*actual,other->first,target)),
              bdd_.constant(lj_.exact(*actual,target)));
        }
        const ProductRobdd::Id a=bdd_.any(first->second),b=bdd_.any(second->second);
        if(certificate_){++certificate_->lowerJesterPairProbes;
            certificate_->lowerJesterSingletonProbes+=2;}
        const ProductRobdd::Id onlyA=bdd_.logical_and(a,bdd_.logical_not(b));
        const ProductRobdd::Id onlyB=bdd_.logical_and(b,bdd_.logical_not(a));
        const ProductRobdd::Id both=bdd_.logical_and(a,b);
        ProductRobdd::Id value=bdd_.constant(false);
        if(lj_.exact(first->first,target))value=bdd_.logical_or(value,onlyA);
        if(lj_.exact(second->first,target))value=bdd_.logical_or(value,onlyB);
        if(lj_.pair(first->first,second->first,target))value=bdd_.logical_or(value,both);
        return value;
    }
    [[nodiscard]] ProductRobdd::Id lower_ghost_formula(const RuntimeRelation&r,
      Color target,std::optional<std::uint32_t> actual,std::uint64_t relationId){
        if(r.lowerChildren.empty())throw std::runtime_error("empty lower Ghost image");
        if(certificate_)++certificate_->lowerGhostMaskProbes;
        const LowerGhostState first=decode_lower_ghost(r.lowerChildren.begin()->first);
        const std::uint32_t geometry=lg_.geometry(first);const auto& g=lg_.geometry(geometry);
        if(r.childTerminal)
            return lower_terminal_formula(bdd_,r,g,target,actual);
        std::uint32_t root=0;
        if(actual){const LowerGhostState child=decode_lower_ghost(*actual);
            if(child.visible)return bdd_.constant(target==Color::White?g.visibleOwner[child.ghost]:g.visibleObserver[child.ghost]);
            root=target==Color::White?g.ownerRoot[child.ghost]:lg_.stratum(g.actualStratum[child.ghost]).root;
        }else{
            if(first.visible){bool force=target==Color::White?g.visibleOwner[first.ghost]:g.visibleObserver[first.ghost];return bdd_.constant(force);}
            root=target==Color::White?g.ownerRoot[first.ghost]:lg_.stratum(r.childStratum).root;
        }
        return relation_compose(r,imported_.at(root),relationId);
    }
    [[nodiscard]] ProductRobdd::Id successor(const RuntimeRelation&r,
      const CompiledEdge* edge,Color target,std::uint64_t relationId){
        if(r.domain==CompiledChildDomain::SameClass){
            if(r.childTerminal){const ProductMask&bad=target==Color::White?r.badWhiteSources:r.badBlackSources;
                if(edge)return bdd_.constant(!bad.test(edge->childActual));
                return bdd_.logical_not(bdd_.any(bad));}
            const ProductRobdd::Id root=edge
              ?owner_[owner_root_index(db_.meta(r.childGeometry),edge->childActual)]
              :observer_[r.childStratum];
            return relation_compose(r,root,relationId);
        }
        if(r.domain==CompiledChildDomain::LowerJester)
            return lower_jester_formula(r,target,edge?std::optional<std::uint32_t>(edge->childConcrete):std::nullopt);
        if(r.domain==CompiledChildDomain::LowerGhost)
            return lower_ghost_formula(r,target,edge?std::optional<std::uint32_t>(edge->childConcrete):std::nullopt,relationId);
        const ProductMask&bad=target==Color::White?r.badWhiteSources:r.badBlackSources;
        if(edge)return bdd_.constant((edge->terminalForces&(target==Color::White?1:2))!=0);
        return bdd_.logical_not(bdd_.any(bad));
    }

    void sweep(MmapFile<ProductRobdd::Id>&ownerOut,MmapFile<ProductRobdd::Id>&observerOut){
        ownerOut.fill(bdd_.constant(false));observerOut.fill(bdd_.constant(false));
        for(std::uint32_t gid=0;gid<db_.geometries();++gid){
            const GeometryDisk&meta=db_.meta(gid);RuntimeBlock block=runtime_block(gid,db_,lg_);
            const Color mover=decode_geometry(meta.raw).side;
            for(unsigned actual=0;actual<ProductVariables;++actual){
                if(!meta.live.test(actual))continue;
                const std::uint32_t stratum=meta.stratumBase+meta.actualStratum[actual];
                ProductRobdd::Id value=mover==Color::White?bdd_.constant(false):bdd_.constant(true);
                if(mover==Color::White){
                    for(const CompiledEdge&edge:block.edges[actual])value=bdd_.logical_or(value,
                      successor(block.relations[edge.relation],&edge,Color::White,
                        (std::uint64_t(gid)<<32)|edge.relation));
                }else{
                    for(std::uint32_t action=0;action<block.actions.size();++action){
                        ProductRobdd::Id common=bdd_.logical_and(domains_[stratum],bdd_.subset_of(block.actions[action].legalSources));
                        const auto found=std::find_if(block.edges[actual].begin(),block.edges[actual].end(),
                          [&](const CompiledEdge&e){return e.action==action;});
                        ProductRobdd::Id child=found==block.edges[actual].end()?bdd_.constant(false):
                          successor(block.relations[found->relation],&*found,Color::White,(std::uint64_t(gid)<<32)|found->relation);
                        value=bdd_.logical_and(value,bdd_.logical_or(bdd_.logical_not(common),child));
                    }
                }
                ownerOut[owner_root_index(meta,actual)]=bdd_.logical_and(domains_[stratum],
                  bdd_.logical_and(bdd_.variable(actual),value));
            }
            for(std::uint32_t local=0;local<meta.stratumCount;++local){
                const std::uint32_t stratum=meta.stratumBase+local;const ProductMask&worlds=db_.stratum(stratum);
                ProductRobdd::Id value=mover==Color::White?bdd_.constant(true):bdd_.constant(false);
                if(mover==Color::White){
                    for(unsigned actual=0;actual<ProductVariables;++actual)if(worlds.test(actual))
                        for(const CompiledEdge&edge:block.edges[actual]){
                            ProductRobdd::Id child=successor(block.relations[edge.relation],nullptr,Color::Black,
                              (std::uint64_t(gid)<<32)|edge.relation);
                            value=bdd_.logical_and(value,bdd_.logical_or(bdd_.logical_not(bdd_.variable(actual)),child));
                        }
                }else{
                    for(std::uint32_t action=0;action<block.actions.size();++action){
                        ProductRobdd::Id gate=bdd_.logical_and(domains_[stratum],bdd_.subset_of(block.actions[action].legalSources));
                        for(const auto&observation:block.actions[action].observations){
                            ProductRobdd::Id possible=bdd_.any(mask_and(observation.sources,worlds));
                            ProductRobdd::Id child=successor(block.relations[observation.relation],nullptr,Color::Black,
                              (std::uint64_t(gid)<<32)|observation.relation);
                            gate=bdd_.logical_and(gate,bdd_.logical_or(bdd_.logical_not(possible),child));
                        }
                        value=bdd_.logical_or(value,gate);
                    }
                }
                observerOut[stratum]=bdd_.logical_and(domains_[stratum],value);
            }
        }
    }
  private:
    TransitionDatabase&db_;LowerJesterOracle&lj_;const LowerGhostSidecar&lg_;ProductRobdd&bdd_;
    std::vector<ProductRobdd::Id>imported_;MmapFile<ProductRobdd::Id>&owner_;MmapFile<ProductRobdd::Id>&observer_;MmapFile<ProductRobdd::Id>&domains_;SolveCertificate*certificate_;
};

[[nodiscard]] bool same_arrays(const MmapFile<ProductRobdd::Id>&a,
                               const MmapFile<ProductRobdd::Id>&b){
    if(a.size()!=b.size())return false;for(std::uint64_t i=0;i<a.size();++i)if(a[i]!=b[i])return false;return true;
}

[[nodiscard]] ProductMask fresh_root(const PublicFrame&frame,const ProductWorld&actual){
    std::vector<ProductWorld> admitted=admitted_fresh_worlds(frame);
    if(frame.side==Color::Black){
        const auto buckets=decision_partition(frame,admitted);const unsigned variable=product_variable(frame,actual);
        for(const auto&bucket:buckets)if(bucket.worlds.test(variable))return bucket.worlds;
        return {};
    }
    ProductMask mask;for(const auto&world:admitted)mask.set(product_variable(frame,world));return mask;
}

[[nodiscard]] std::string canonical_terminal_view(
  const PublicFrame& frame, const ProductWorld& world) {
    std::optional<std::string> best;
    for (std::uint8_t raw=0; raw<4; ++raw) {
        const FramedWorld transformed=transform_world(frame,world,
          static_cast<RectangleTransform>(raw));
        const std::string key=view_key(make_position(transformed.frame,
          transformed.world),{Color::Black,false});
        if(!best||key<*best)best=key;
    }
    return *best;
}

template<typename Value, typename Getter>
void write_sequence(std::ofstream& output, std::uint64_t count,
                    Getter getter, const char* label) {
    constexpr std::size_t Chunk = 1u << 15;
    std::vector<Value> buffer;
    buffer.reserve(Chunk);
    for (std::uint64_t cursor = 0; cursor < count;) {
        const std::size_t take = static_cast<std::size_t>(
          std::min<std::uint64_t>(count - cursor, Chunk));
        buffer.clear();
        for (std::size_t offset = 0; offset < take; ++offset)
            buffer.push_back(getter(cursor + offset));
        output.write(reinterpret_cast<const char*>(buffer.data()),
                     static_cast<std::streamsize>(take * sizeof(Value)));
        if (!output)
            throw std::runtime_error(std::string("cannot write ") + label);
        cursor += take;
    }
}

[[nodiscard]] ArbitrarySidecarCertificate write_arbitrary_sidecar(
  const std::string& path, const SolveOptions& options,
  const TransitionHeaderDisk& transition, TransitionDatabase& database,
  ProductRobdd& bdd, MmapFile<ProductRobdd::Id>& owner,
  MmapFile<ProductRobdd::Id>& observer) {
    if (path.empty())
        throw std::invalid_argument(
          "exact Jester/Ghost solve requires outputArbitrarySidecar");
    if (owner.size() != database.owner_roots() ||
        observer.size() != database.strata())
        throw std::runtime_error("arbitrary root extent residual");
    owner.flush();
    observer.flush();
    ArbitraryHeaderDisk header;
    header.upperNodes = bdd.upper_node_count();
    header.lowerNodes = bdd.lower_node_count();
    header.geometries = database.geometries();
    header.strata = database.strata();
    header.ownerRoots = database.owner_roots();
    header.upperOffset = sizeof(header);
    header.lowerOffset = checked_add(header.upperOffset, checked_mul(
      header.upperNodes, sizeof(ArbitraryUpperNodeDisk)));
    header.geometryOffset = checked_add(header.lowerOffset, checked_mul(
      header.lowerNodes, sizeof(ArbitraryLowerNodeDisk)));
    header.stratumOffset = checked_add(header.geometryOffset, checked_mul(
      header.geometries, sizeof(ArbitraryGeometryDisk)));
    header.ownerOffset = checked_add(header.stratumOffset, checked_mul(
      header.strata, sizeof(ProductMask)));
    header.observerOffset = checked_add(header.ownerOffset, checked_mul(
      header.ownerRoots, sizeof(ProductRobdd::Id)));
    const std::uint64_t extent = checked_add(header.observerOffset, checked_mul(
      header.strata, sizeof(ProductRobdd::Id)));
    header.payloadBytes = extent - sizeof(header);
    header.semantics = arbitrary_semantics();
    copy_digest(header.sourceSha, options.sourceSha256, "source SHA-256");
    copy_digest(header.modelSha, options.modelSha256, "model SHA-256");
    copy_digest(header.observationSha, options.observationSha256,
                "observation SHA-256");
    copy_digest(header.transitionPayloadSha,
      std::string(transition.payloadSha.data(), 64),
      "transition payload SHA-256");
    copy_digest(header.transitionHeaderSha,
      sha256_file(options.transitionPrefix + ".header"),
      "transition header SHA-256");
    copy_digest(header.transitionMarkerSha,
      sha256_file(options.transitionPrefix + ".verified"),
      "transition marker SHA-256");
    copy_digest(header.lowerJesterOverlaySha,
      options.lowerJesterOverlaySha256, "lower Jester overlay SHA-256");
    copy_digest(header.lowerGhostSidecarSha,
      options.lowerGhostSidecarSha256, "lower Ghost sidecar SHA-256");

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    write_value(output, header);
    write_sequence<ArbitraryUpperNodeDisk>(output, header.upperNodes,
      [&](std::uint64_t id) {
          const auto node = bdd.upper_node_record(id);
          ArbitraryUpperNodeDisk result; result.variable = node.variable;
          result.low = node.low; result.high = node.high; return result;
      }, "Jester/Ghost arbitrary upper nodes");
    write_sequence<ArbitraryLowerNodeDisk>(output, header.lowerNodes,
      [&](std::uint64_t id) {
          const auto node = bdd.lower_node_record(
            static_cast<std::uint32_t>(id));
          ArbitraryLowerNodeDisk result; result.variable = node.variable;
          result.low = node.low; result.high = node.high; return result;
      }, "Jester/Ghost arbitrary lower nodes");
    write_sequence<ArbitraryGeometryDisk>(output, header.geometries,
      [&](std::uint64_t id) {
          const GeometryDisk& source = database.meta(id);
          ArbitraryGeometryDisk result; result.raw = source.raw;
          result.ownerBase = source.ownerBase;
          result.stratumBase = source.stratumBase;
          result.stratumCount = source.stratumCount;
          result.liveCount = source.liveCount; result.live = source.live;
          return result;
      }, "Jester/Ghost arbitrary geometries");
    write_sequence<ProductMask>(output, header.strata,
      [&](std::uint64_t id) { return database.stratum(id); },
      "Jester/Ghost arbitrary strata");
    write_sequence<ProductRobdd::Id>(output, header.ownerRoots,
      [&](std::uint64_t id) { return owner[id]; },
      "Jester/Ghost arbitrary owner roots");
    write_sequence<ProductRobdd::Id>(output, header.strata,
      [&](std::uint64_t id) { return observer[id]; },
      "Jester/Ghost arbitrary observer roots");
    output.close();
    if (!output || file_bytes(path) != extent)
        throw std::runtime_error("Jester/Ghost arbitrary extent residual");
    const std::string payload = sha256_range(path, sizeof(header),
                                             header.payloadBytes);
    std::copy(payload.begin(), payload.end(), header.payloadSha.begin());
    std::fstream rewrite(path, std::ios::binary | std::ios::in |
                               std::ios::out);
    rewrite.write(reinterpret_cast<const char*>(&header), sizeof(header));
    rewrite.close();
    if (!rewrite)
        throw std::runtime_error("cannot finalize Jester/Ghost sidecar");
    SolveOptions standalone = options;
    standalone.transitionPrefix.clear();
    return verify_arbitrary_sidecar(path, standalone);
}

} // namespace

SolveCertificate solve_exact(const SolveOptions& options) {
    require_hash(options.sourceSha256, "source SHA-256");
    require_hash(options.modelSha256, "model SHA-256");
    require_hash(options.observationSha256, "observation SHA-256");
    if (options.scratchPrefix.empty() ||
        (!options.measureIterations &&
         (options.outputOverlay.empty() ||
          options.outputArbitrarySidecar.empty())))
        throw std::invalid_argument("exact solve needs scratch and proof output");
    const TransitionHeaderDisk transitionHeader =
      authenticate_transition_database(options.transitionPrefix,
        options.sourceSha256,options.modelSha256,
        options.observationSha256,true);
    const TransitionCertificate transition=transition_certificate(
      transitionHeader);
    (void)full_domain_preflight(options.transitionPrefix, options.bdd,
      options.resources, options.scratchPrefix);
    const PackedTable concrete = load_concrete(options.sourceTable,
      StateCount, PieceType::Jester, PieceType::Ghost);
    if (concrete.sha != options.sourceSha256)
        throw std::runtime_error("source UFTB SHA mismatch");
    LowerJesterOracle lowerJester(options.lowerJesterTable,
      options.lowerJesterOverlay, options.lowerJesterModelSha256,
      options.lowerJesterOverlaySha256);
    LowerGhostSidecar lowerGhost(options);
    GhostInformationProbe independentLowerProbe(options.lowerGhostSidecar,
      options.lowerGhostSourceSha256, options.lowerGhostModelSha256,
      options.lowerGhostObservationSha256);
    (void)independentLowerProbe;
    TransitionDatabase database(options.transitionPrefix);
    ProductRobdd bdd(options.scratchPrefix + ".bdd-a", options.bdd, true);
    std::vector<ProductRobdd::Id> imported =
      bdd.import_suffix(lowerGhost.nodes());
    const std::uint64_t ownerCount = database.owner_roots();
    MmapFile<ProductRobdd::Id> owner(options.scratchPrefix + ".owner-a",
      ownerCount, true), ownerNext(options.scratchPrefix + ".owner-b",
      ownerCount, true);
    MmapFile<ProductRobdd::Id> observer(options.scratchPrefix + ".observer-a",
      database.strata(), true), observerNext(
      options.scratchPrefix + ".observer-b", database.strata(), true);
    MmapFile<ProductRobdd::Id> domains(options.scratchPrefix + ".domains",
      database.strata(), true);
    owner.fill(bdd.constant(false)); observer.fill(bdd.constant(false));
    for (std::uint64_t id = 0; id < database.strata(); ++id)
        // Empty is retained as the vacuous bottom element. Fresh roots are
        // independently nonempty; preserving bottom is required for exact
        // observer downward closure and matches certified KGhost semantics.
        domains[id] = bdd.subset_of(database.stratum(id));

    SolveCertificate certificate;
    certificate.transitionPayloadSha256 = transition.payloadSha256;
    certificate.transitionHeaderSha256 =
      sha256_file(options.transitionPrefix + ".header");
    certificate.transitionMarkerSha256 =
      sha256_file(options.transitionPrefix + ".verified");
    certificate.lowerJesterOverlaySha256 =
      options.lowerJesterOverlaySha256;
    certificate.lowerGhostSidecarSha256 =
      options.lowerGhostSidecarSha256;
    certificate.observationResidual = transition.codecResidual +
      transition.actionResidual + transition.decisionResidual +
      transition.transitionResidual + transition.symmetryResidual;
    for (;;) {
        ExactKernel kernel(database, lowerJester, lowerGhost, bdd, imported,
                           owner, observer, domains, &certificate);
        kernel.sweep(ownerNext, observerNext);
        ++certificate.iterations;
        const bool stable = same_arrays(owner, ownerNext) &&
                            same_arrays(observer, observerNext);
        if (options.measureIterations &&
            (stable || certificate.iterations >= options.measureIterations)) {
            certificate.upperBddNodes = bdd.upper_node_count();
            certificate.lowerBddNodes = bdd.lower_node_count();
            return certificate;
        }
        if (stable)
            break;
        std::swap(owner, ownerNext);
        std::swap(observer, observerNext);
        if (options.compactEvery &&
            certificate.iterations % options.compactEvery == 0) {
            std::vector<ProductRobdd::Id> roots;
            roots.reserve(owner.size() + observer.size() + domains.size() +
                          imported.size());
            for (std::uint64_t id = 0; id < owner.size(); ++id)
                roots.push_back(owner[id]);
            for (std::uint64_t id = 0; id < observer.size(); ++id)
                roots.push_back(observer[id]);
            for (std::uint64_t id = 0; id < domains.size(); ++id)
                roots.push_back(domains[id]);
            roots.insert(roots.end(), imported.begin(), imported.end());
            auto compacted = bdd.compact(options.scratchPrefix +
              (certificate.compactions % 2 ? ".bdd-a" : ".bdd-b"),
              options.scratchPrefix + ".compact-remap", roots);
            if (compacted.second.rootResidual ||
                compacted.second.structuralResidual)
                throw std::runtime_error("ProductRobdd compaction residual");
            std::size_t cursor = 0;
            for (std::uint64_t id = 0; id < owner.size(); ++id)
                owner[id] = roots[cursor++];
            for (std::uint64_t id = 0; id < observer.size(); ++id)
                observer[id] = roots[cursor++];
            for (std::uint64_t id = 0; id < domains.size(); ++id)
                domains[id] = roots[cursor++];
            std::copy(roots.begin() + cursor, roots.end(), imported.begin());
            bdd = std::move(compacted.first);
            ++certificate.compactions;
        }
    }

    ExactKernel verifier(database, lowerJester, lowerGhost, bdd, imported,
                         owner, observer, domains, &certificate);
    verifier.sweep(ownerNext, observerNext);
    for (std::uint64_t id = 0; id < owner.size(); ++id)
        certificate.bellmanResidual += owner[id] != ownerNext[id];
    for (std::uint64_t id = 0; id < observer.size(); ++id)
        certificate.bellmanResidual += observer[id] != observerNext[id];
    if (certificate.bellmanResidual)
        throw std::runtime_error("exact Bellman residual is nonzero");

    for (std::uint32_t gid = 0; gid < database.geometries(); ++gid) {
        const GeometryDisk& meta = database.meta(gid);
        const PublicFrame frame = decode_geometry(meta.raw);
        for (unsigned actual = 0; actual < ProductVariables; ++actual) {
            if (!meta.live.test(actual)) continue;
            const std::uint32_t stratum = meta.stratumBase +
                                          meta.actualStratum[actual];
            certificate.rankResidual += !bdd.is_upward_closed(
              owner[owner_root_index(meta, actual)],
              database.stratum(stratum));
            ProductMask singleton; singleton.set(actual);
            const std::uint32_t sourceIndex = encode_source(product_to_source(
              frame, decode_product_variable(frame, actual)));
            const Wdl exact = concrete.result(sourceIndex);
            certificate.singletonResidual += bdd.evaluate(
              owner[owner_root_index(meta, actual)], singleton)
              != wdl_forces(exact, frame.side, Color::White);
            certificate.singletonResidual += bdd.evaluate(observer[stratum],
              singleton) != wdl_forces(exact, frame.side, Color::Black);
            // These are complete force functions, not merely fresh-root
            // samples.  owner[] already requires a nonempty domain mask that
            // contains actual; observer[] is restricted to the identical
            // mover-private decision cell. Their conjunction must therefore
            // be identically false over every reachable history belief.
            certificate.dualWinResidual += bdd.logical_and(
              owner[owner_root_index(meta, actual)], observer[stratum]) !=
              bdd.constant(false);
        }
        for (std::uint32_t local = 0; local < meta.stratumCount; ++local)
            certificate.rankResidual += !bdd.is_downward_closed(
              observer[meta.stratumBase + local]);
    }
    if (certificate.rankResidual || certificate.singletonResidual ||
        certificate.dualWinResidual)
        throw std::runtime_error(
          "exact rank/singleton/arbitrary dual-force residual is nonzero");

    // Discard construction-only domain and imported-lower roots from the
    // permanent arena. The all-beliefs artifact retains exactly the force
    // functions used by every live owner realization and observer cell.
    {
        std::vector<ProductRobdd::Id> roots;
        roots.reserve(owner.size() + observer.size());
        for (std::uint64_t id = 0; id < owner.size(); ++id)
            roots.push_back(owner[id]);
        for (std::uint64_t id = 0; id < observer.size(); ++id)
            roots.push_back(observer[id]);
        auto compacted = bdd.compact(options.scratchPrefix + ".bdd-final",
          options.scratchPrefix + ".compact-final", roots);
        if (compacted.second.rootResidual ||
            compacted.second.structuralResidual)
            throw std::runtime_error("final arbitrary compaction residual");
        std::size_t cursor = 0;
        for (std::uint64_t id = 0; id < owner.size(); ++id)
            owner[id] = roots[cursor++];
        for (std::uint64_t id = 0; id < observer.size(); ++id)
            observer[id] = roots[cursor++];
        bdd = std::move(compacted.first);
        ++certificate.compactions;
    }
    const ArbitrarySidecarCertificate arbitrary = write_arbitrary_sidecar(
      options.outputArbitrarySidecar, options, transitionHeader, database,
      bdd, owner, observer);
    certificate.arbitrarySidecarSha256 = arbitrary.fileSha256;
    certificate.arbitraryStructuralResidual = arbitrary.structuralResidual;
    ArbitrarySidecarProbe arbitraryProbe(options.outputArbitrarySidecar,
                                          options);
    for (std::uint32_t gid = 0; gid < database.geometries(); ++gid) {
        const GeometryDisk& meta = database.meta(gid);
        const PublicFrame frame = decode_geometry(meta.raw);
        for (unsigned actual = 0; actual < ProductVariables; ++actual) {
            if (!meta.live.test(actual)) continue;
            ProductMask singleton;
            singleton.set(actual);
            const ProductWorld world = decode_product_variable(frame, actual);
            const std::uint32_t stratum = meta.stratumBase +
                                          meta.actualStratum[actual];
            certificate.arbitrarySingletonResidual +=
              arbitraryProbe.owner_forces(frame, world, singleton) !=
              bdd.evaluate(owner[owner_root_index(meta, actual)], singleton);
            certificate.arbitrarySingletonResidual +=
              arbitraryProbe.observer_forces(frame, world, singleton) !=
              bdd.evaluate(observer[stratum], singleton);
        }
    }
    if (certificate.arbitraryStructuralResidual ||
        certificate.arbitrarySingletonResidual)
        throw std::runtime_error("arbitrary structural/singleton residual");

    std::vector<std::uint8_t> flags(StateCount, 0);
    std::array<std::set<std::string>, 2> rootSets;
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        const ConcreteState source = decode_source(index);
        const FramedWorld physical = source_to_product(source);
        const Position position = make_position(physical.frame, physical.world);
        const std::size_t side = static_cast<std::size_t>(source.side);
        if (fresh_world_admission(physical.frame, physical.world) !=
              AdmissionVerdict::Admit) {
            ++certificate.unreachable[side]
              [static_cast<unsigned>(concrete.result(index))];
            ++certificate.unreachableRealizations[side];
            continue;
        }
        ++certificate.legalRealizations[side];
        if (position.game_over()) {
            flags[index] = 4 | terminal_flags(position);
            rootSets[side].insert(canonical_terminal_view(
              physical.frame, physical.world));
        } else {
            const ProductMask root = fresh_root(physical.frame, physical.world);
            const unsigned rawActual = product_variable(physical.frame,
                                                        physical.world);
            if (!root.test(rawActual)) {
                ++certificate.partitionResidual;
                throw std::runtime_error(
                  "admitted root belongs to no decision information cell");
            }
            const CanonicalSet canonical = canonicalize_set(
              physical.frame, root);
            const FramedWorld actual = transform_world(physical.frame,
              physical.world, canonical.transform);
            const std::uint32_t gid = database.ordinal(
              encode_geometry(canonical.value.frame));
            const unsigned variable = product_variable(
              canonical.value.frame, actual.world);
            const GeometryDisk& meta = database.meta(gid);
            const std::uint32_t stratum = meta.stratumBase +
                                          meta.actualStratum[variable];
            std::string setKey(sizeof(gid) + sizeof(canonical.value.worlds),
                               '\0');
            std::memcpy(setKey.data(), &gid, sizeof(gid));
            std::memcpy(setKey.data() + sizeof(gid),
                        &canonical.value.worlds,
                        sizeof(canonical.value.worlds));
            rootSets[side].insert(std::move(setKey));
            const bool white = bdd.evaluate(owner[
              owner_root_index(meta, variable)],
              canonical.value.worlds);
            const bool black = bdd.evaluate(observer[stratum],
                                             canonical.value.worlds);
            certificate.arbitraryRootResidual +=
              arbitraryProbe.owner_forces(physical.frame, physical.world,
                                           root) != white;
            certificate.arbitraryRootResidual +=
              arbitraryProbe.observer_forces(physical.frame, physical.world,
                                              root) != black;
            certificate.dualWinResidual += white && black;
            flags[index] = 4 | (white ? 1 : 0) | (black ? 2 : 0);
        }
        if (flags[index] & 4) {
            const bool mover = flags[index] &
              (source.side == Color::White ? 1 : 2);
            const bool opponent = flags[index] &
              (source.side == Color::White ? 2 : 1);
            ++certificate.totals[side][mover ? 1 : opponent ? 2 : 3];
        }
    }
    for (std::size_t side = 0; side < 2; ++side) {
        certificate.informationSets[side] = rootSets[side].size();
        std::uint64_t conserved = 0;
        for (unsigned result = 1; result < 4; ++result)
            conserved += certificate.totals[side][result] +
                         certificate.unreachable[side][result];
        certificate.conservationResidual += conserved != StateCount / 2;
        certificate.conservationResidual +=
          certificate.legalRealizations[side] +
          certificate.unreachableRealizations[side] != StateCount / 2;
    }
    if (certificate.dualWinResidual || certificate.conservationResidual ||
        certificate.partitionResidual || certificate.observationResidual ||
        certificate.arbitraryRootResidual)
        throw std::runtime_error("root conservation/dual-win residual");

    std::ofstream output(options.outputOverlay,
                         std::ios::binary | std::ios::trunc);
    output.write(OverlayMagic, 8);
    for (std::uint32_t word : {2u,
          static_cast<std::uint32_t>(PieceType::Jester),
          static_cast<std::uint32_t>(PieceType::Ghost),
          static_cast<std::uint32_t>(Color::White), StateCount, 2u})
        write_value(output, word);
    output.write(options.sourceSha256.data(), 64);
    output.write(options.modelSha256.data(), 64);
    output.write(reinterpret_cast<const char*>(flags.data()), flags.size());
    if (!output) throw std::runtime_error("failed writing exact UFIW2");
    output.close();
    certificate.overlaySha256 = sha256_file(options.outputOverlay);
    certificate.upperBddNodes = bdd.upper_node_count();
    certificate.lowerBddNodes = bdd.lower_node_count();
    return certificate;
}

ArbitrarySidecarCertificate verify_arbitrary_sidecar(
  const std::string& path, const SolveOptions& options) {
    require_hash(options.sourceSha256, "source SHA-256");
    require_hash(options.modelSha256, "model SHA-256");
    require_hash(options.observationSha256, "observation SHA-256");
    require_hash(options.lowerJesterOverlaySha256,
                 "lower Jester overlay SHA-256");
    require_hash(options.lowerGhostSidecarSha256,
                 "lower Ghost sidecar SHA-256");
    std::ifstream input(path, std::ios::binary);
    const ArbitraryHeaderDisk header = read_value<ArbitraryHeaderDisk>(input);
    const auto text = [](const std::array<char,64>& value) {
        return std::string(value.data(), value.size());
    };
    if (header.magic != std::array<char,8>{'U','F','J','G','1',0,0,0} ||
        header.version != 1 || header.headerBytes != sizeof(header) ||
        header.endian != 0x01020304 ||
        header.primary != static_cast<std::uint32_t>(PieceType::Jester) ||
        header.secondary != static_cast<std::uint32_t>(PieceType::Ghost) ||
        header.owner != static_cast<std::uint32_t>(Color::White) ||
        header.files != Position::BoardFiles ||
        header.ranks != Position::BoardRanks || header.squares != Squares ||
        header.variables != ProductVariables ||
        header.stateCount != StateCount ||
        header.upperNodeBytes != sizeof(ArbitraryUpperNodeDisk) ||
        header.lowerNodeBytes != sizeof(ArbitraryLowerNodeDisk) ||
        header.geometryBytes != sizeof(ArbitraryGeometryDisk) ||
        header.maskBytes != sizeof(ProductMask) ||
        header.rootBytes != sizeof(ProductRobdd::Id) || header.reserved ||
        header.alignment ||
        header.lowerNodes < 2 || !header.geometries || !header.strata ||
        !header.ownerRoots || header.semantics != arbitrary_semantics() ||
        text(header.sourceSha) != options.sourceSha256 ||
        text(header.modelSha) != options.modelSha256 ||
        text(header.observationSha) != options.observationSha256 ||
        text(header.lowerJesterOverlaySha) !=
          options.lowerJesterOverlaySha256 ||
        text(header.lowerGhostSidecarSha) !=
          options.lowerGhostSidecarSha256 ||
        !valid_sha256(text(header.transitionPayloadSha)) ||
        !valid_sha256(text(header.transitionHeaderSha)) ||
        !valid_sha256(text(header.transitionMarkerSha)) ||
        !valid_sha256(text(header.payloadSha)))
        throw std::runtime_error("invalid Jester/Ghost arbitrary header");
    std::uint64_t cursor = sizeof(header);
    const auto section = [&](std::uint64_t declared, std::uint64_t count,
                             std::uint64_t width) {
        if (declared != cursor)
            throw std::runtime_error("Jester/Ghost arbitrary offset residual");
        cursor = checked_add(cursor, checked_mul(count, width));
    };
    section(header.upperOffset, header.upperNodes,
            sizeof(ArbitraryUpperNodeDisk));
    section(header.lowerOffset, header.lowerNodes,
            sizeof(ArbitraryLowerNodeDisk));
    section(header.geometryOffset, header.geometries,
            sizeof(ArbitraryGeometryDisk));
    section(header.stratumOffset, header.strata, sizeof(ProductMask));
    section(header.ownerOffset, header.ownerRoots, sizeof(ProductRobdd::Id));
    section(header.observerOffset, header.strata, sizeof(ProductRobdd::Id));
    if (header.payloadBytes != cursor - sizeof(header) ||
        file_bytes(path) != cursor ||
        sha256_range(path, sizeof(header), header.payloadBytes) !=
          text(header.payloadSha))
        throw std::runtime_error("Jester/Ghost arbitrary payload residual");
    if (!options.transitionPrefix.empty()) {
        const TransitionHeaderDisk transition = authenticate_transition_database(
          options.transitionPrefix, options.sourceSha256, options.modelSha256,
          options.observationSha256, true);
        if (text(header.transitionPayloadSha) !=
              std::string(transition.payloadSha.data(), 64) ||
            text(header.transitionHeaderSha) !=
              sha256_file(options.transitionPrefix + ".header") ||
            text(header.transitionMarkerSha) !=
              sha256_file(options.transitionPrefix + ".verified"))
            throw std::runtime_error(
              "Jester/Ghost arbitrary transition provenance residual");
    }
    if (!options.lowerJesterOverlay.empty() &&
        sha256_file(options.lowerJesterOverlay) !=
          options.lowerJesterOverlaySha256)
        throw std::runtime_error(
          "Jester/Ghost arbitrary lower-Jester provenance residual");
    if (!options.lowerGhostSidecar.empty() &&
        sha256_file(options.lowerGhostSidecar) !=
          options.lowerGhostSidecarSha256)
        throw std::runtime_error(
          "Jester/Ghost arbitrary lower-Ghost provenance residual");

    std::ifstream upper(path, std::ios::binary);
    upper.seekg(static_cast<std::streamoff>(header.upperOffset));
    std::vector<std::uint8_t> upperLevels;
    upperLevels.reserve(static_cast<std::size_t>(header.upperNodes));
    std::set<std::tuple<std::uint8_t,ProductRobdd::Id,ProductRobdd::Id>>
      upperTuples;
    const auto validRoot = [&](ProductRobdd::Id root,
                               std::uint64_t upperCount) {
        if (root & LeafTag)
            return (root & LeafPayload) < header.lowerNodes;
        return root < upperCount;
    };
    for (std::uint64_t id = 0; id < header.upperNodes; ++id) {
        const ArbitraryUpperNodeDisk node =
          read_value<ArbitraryUpperNodeDisk>(upper);
        if (node.variable >= Squares || node.reserved !=
              std::array<std::uint8_t,7>{} || node.low == node.high ||
            !validRoot(node.low, id) || !validRoot(node.high, id) ||
            !upperTuples.emplace(node.variable,node.low,node.high).second)
            throw std::runtime_error("bad Jester/Ghost arbitrary upper node");
        const auto level = [&](ProductRobdd::Id child) {
            return child & LeafTag ? Squares : upperLevels.at(child);
        };
        if (level(node.low) <= node.variable ||
            level(node.high) <= node.variable)
            throw std::runtime_error(
              "unordered Jester/Ghost arbitrary upper node");
        upperLevels.push_back(node.variable);
    }
    std::ifstream lower(path, std::ios::binary);
    lower.seekg(static_cast<std::streamoff>(header.lowerOffset));
    std::vector<std::uint8_t> lowerLevels;
    lowerLevels.reserve(static_cast<std::size_t>(header.lowerNodes));
    std::set<std::tuple<std::uint8_t,std::uint32_t,std::uint32_t>>
      lowerTuples;
    for (std::uint64_t id = 0; id < header.lowerNodes; ++id) {
        const ArbitraryLowerNodeDisk node =
          read_value<ArbitraryLowerNodeDisk>(lower);
        if (id < 2) {
            if (node.variable != Squares || node.reserved !=
                  std::array<std::uint8_t,3>{} ||
                node.low != id || node.high != id)
                throw std::runtime_error(
                  "bad Jester/Ghost arbitrary lower terminal");
        } else if (node.variable >= Squares || node.reserved !=
                     std::array<std::uint8_t,3>{} || node.low >= id ||
                   node.high >= id || node.low == node.high ||
                   !lowerTuples.emplace(node.variable,node.low,node.high).second ||
                   lowerLevels[node.low] <= node.variable ||
                   lowerLevels[node.high] <= node.variable) {
            throw std::runtime_error("bad Jester/Ghost arbitrary lower node");
        }
        lowerLevels.push_back(node.variable);
    }

    std::ifstream geometries(path, std::ios::binary);
    std::ifstream strata(path, std::ios::binary);
    std::ifstream ownerRoots(path, std::ios::binary);
    std::ifstream observerRoots(path, std::ios::binary);
    geometries.seekg(static_cast<std::streamoff>(header.geometryOffset));
    strata.seekg(static_cast<std::streamoff>(header.stratumOffset));
    ownerRoots.seekg(static_cast<std::streamoff>(header.ownerOffset));
    observerRoots.seekg(static_cast<std::streamoff>(header.observerOffset));
    std::uint64_t ownerCursor = 0, stratumCursor = 0;
    std::uint32_t previousRaw = 0;
    for (std::uint64_t gid = 0; gid < header.geometries; ++gid) {
        const ArbitraryGeometryDisk meta =
          read_value<ArbitraryGeometryDisk>(geometries);
        const PublicFrame frame = decode_geometry(meta.raw);
        ProductMask geometric;
        for (const ProductWorld& world : geometric_worlds(frame))
            geometric.set(product_variable(frame, world));
        if (meta.reserved || meta.alignment != std::array<std::uint8_t,3>{} ||
            (gid && meta.raw <= previousRaw) ||
            canonical_geometry(frame).first != meta.raw ||
            meta.ownerBase != ownerCursor ||
            meta.stratumBase != stratumCursor || !meta.stratumCount ||
            !meta.liveCount || meta.live.count() != meta.liveCount)
            throw std::runtime_error("bad Jester/Ghost arbitrary geometry");
        for (unsigned word = 0; word < meta.live.words.size(); ++word)
            if (meta.live.words[word] & ~geometric.words[word])
                throw std::runtime_error(
                  "Jester/Ghost arbitrary live-mask domain residual");
        previousRaw = meta.raw;
        ProductMask partition;
        for (std::uint32_t local = 0; local < meta.stratumCount; ++local) {
            const ProductMask cell = read_value<ProductMask>(strata);
            const ProductRobdd::Id observerRoot =
              read_value<ProductRobdd::Id>(observerRoots);
            if (!cell.count() || !validRoot(observerRoot, header.upperNodes))
                throw std::runtime_error(
                  "bad Jester/Ghost arbitrary observer stratum");
            for (unsigned word = 0; word < cell.words.size(); ++word) {
                if ((cell.words[word] & ~meta.live.words[word]) ||
                    (cell.words[word] & partition.words[word]))
                    throw std::runtime_error(
                      "Jester/Ghost arbitrary stratum partition residual");
                partition.words[word] |= cell.words[word];
            }
        }
        if (!(partition == meta.live))
            throw std::runtime_error(
              "Jester/Ghost arbitrary incomplete stratum partition");
        for (unsigned local = 0; local < meta.liveCount; ++local)
            if (!validRoot(read_value<ProductRobdd::Id>(ownerRoots),
                           header.upperNodes))
                throw std::runtime_error(
                  "bad Jester/Ghost arbitrary owner root");
        ownerCursor += meta.liveCount;
        stratumCursor += meta.stratumCount;
    }
    if (ownerCursor != header.ownerRoots || stratumCursor != header.strata)
        throw std::runtime_error(
          "Jester/Ghost arbitrary catalog conservation residual");
    ArbitrarySidecarCertificate result;
    result.upperNodes = header.upperNodes;
    result.lowerNodes = header.lowerNodes;
    result.geometries = header.geometries;
    result.strata = header.strata;
    result.ownerRoots = header.ownerRoots;
    result.bytes = cursor;
    result.payloadSha256 = text(header.payloadSha);
    result.fileSha256 = sha256_file(path);
    result.transitionPayloadSha256 = text(header.transitionPayloadSha);
    result.transitionHeaderSha256 = text(header.transitionHeaderSha);
    result.transitionMarkerSha256 = text(header.transitionMarkerSha);
    result.lowerJesterOverlaySha256 = text(header.lowerJesterOverlaySha);
    result.lowerGhostSidecarSha256 = text(header.lowerGhostSidecarSha);
    return result;
}

class ArbitrarySidecarProbe::Impl {
  public:
    Impl(const std::string& path, const SolveOptions& options)
      : certificate_(verify_arbitrary_sidecar(path, options)) {
        bytes_ = file_bytes(path);
        if (bytes_ > std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("Jester/Ghost sidecar too large");
        descriptor_ = ::open(path.c_str(), O_RDONLY);
        if (descriptor_ < 0) system_error("cannot open", path);
        void* mapping = ::mmap(nullptr, static_cast<std::size_t>(bytes_),
          PROT_READ, MAP_SHARED, descriptor_, 0);
        if (mapping == MAP_FAILED) {
            ::close(descriptor_); descriptor_ = -1;
            system_error("cannot mmap", path);
        }
        data_ = static_cast<const std::uint8_t*>(mapping);
        header_ = read<ArbitraryHeaderDisk>(0);
    }
    ~Impl() {
        if (data_) ::munmap(const_cast<std::uint8_t*>(data_),
                            static_cast<std::size_t>(bytes_));
        if (descriptor_ >= 0) ::close(descriptor_);
    }
    [[nodiscard]] bool forces(const PublicFrame& frame,
                              const ProductWorld& actual,
                              const ProductMask& worlds, bool owner) const {
        const unsigned rawActual = product_variable(frame, actual);
        if (!worlds.count() || !worlds.test(rawActual))
            throw std::invalid_argument(
              "Jester/Ghost belief does not contain actual world");
        const CanonicalSet canonical = canonicalize_set(frame, worlds);
        const FramedWorld mapped = transform_world(frame, actual,
                                                   canonical.transform);
        const std::uint32_t raw = encode_geometry(canonical.value.frame);
        std::uint64_t low = 0, high = header_.geometries;
        while (low < high) {
            const std::uint64_t middle = (low + high) / 2;
            if (geometry(middle).raw < raw) low = middle + 1;
            else high = middle;
        }
        if (low >= header_.geometries || geometry(low).raw != raw)
            throw std::runtime_error("Jester/Ghost probe geometry absent");
        const ArbitraryGeometryDisk meta = geometry(low);
        const unsigned variable = product_variable(canonical.value.frame,
                                                   mapped.world);
        if (!meta.live.test(variable))
            throw std::invalid_argument(
              "Jester/Ghost probe requested terminal/nonlive world");
        std::uint32_t local = NoIndex;
        for (std::uint32_t candidate = 0;
             candidate < meta.stratumCount; ++candidate) {
            const ProductMask cell = read<ProductMask>(header_.stratumOffset +
              (meta.stratumBase + candidate) * sizeof(ProductMask));
            if (cell.test(variable)) { local = candidate; break; }
        }
        if (local == NoIndex)
            throw std::runtime_error("Jester/Ghost decision-cell residual");
        const ProductMask cell = read<ProductMask>(header_.stratumOffset +
          (meta.stratumBase + local) * sizeof(ProductMask));
        for (unsigned word = 0; word < cell.words.size(); ++word)
            if (canonical.value.worlds.words[word] & ~cell.words[word])
                throw std::invalid_argument(
                  "Jester/Ghost belief spans legal-dot decision cells");
        ProductRobdd::Id root;
        if (owner) {
            unsigned rank = 0;
            for (unsigned candidate = 0; candidate < variable; ++candidate)
                rank += meta.live.test(candidate);
            if (rank >= meta.liveCount)
                throw std::runtime_error("Jester/Ghost owner rank residual");
            root = read<ProductRobdd::Id>(header_.ownerOffset +
              (meta.ownerBase + rank) * sizeof(ProductRobdd::Id));
        } else {
            root = read<ProductRobdd::Id>(header_.observerOffset +
              (meta.stratumBase + local) * sizeof(ProductRobdd::Id));
        }
        while (!(root & LeafTag)) {
            const ArbitraryUpperNodeDisk node = read<ArbitraryUpperNodeDisk>(
              header_.upperOffset + root * sizeof(ArbitraryUpperNodeDisk));
            root = canonical.value.worlds.test(node.variable) ?
              node.high : node.low;
        }
        std::uint32_t lower = static_cast<std::uint32_t>(root & LeafPayload);
        while (lower > 1) {
            const ArbitraryLowerNodeDisk node = read<ArbitraryLowerNodeDisk>(
              header_.lowerOffset + std::uint64_t(lower) *
                                    sizeof(ArbitraryLowerNodeDisk));
            lower = canonical.value.worlds.test(Squares + node.variable) ?
              node.high : node.low;
        }
        return lower == 1;
    }
    [[nodiscard]] const ArbitrarySidecarCertificate& certificate() const {
        return certificate_;
    }
  private:
    template<typename Value>
    [[nodiscard]] Value read(std::uint64_t offset) const {
        if (offset > bytes_ || sizeof(Value) > bytes_ - offset)
            throw std::runtime_error("Jester/Ghost mapped read overflow");
        Value value;
        std::memcpy(&value, data_ + offset, sizeof(value));
        return value;
    }
    [[nodiscard]] ArbitraryGeometryDisk geometry(std::uint64_t id) const {
        return read<ArbitraryGeometryDisk>(header_.geometryOffset +
          id * sizeof(ArbitraryGeometryDisk));
    }
    int descriptor_ = -1;
    const std::uint8_t* data_ = nullptr;
    std::uint64_t bytes_ = 0;
    ArbitraryHeaderDisk header_{};
    ArbitrarySidecarCertificate certificate_;
};

ArbitrarySidecarProbe::ArbitrarySidecarProbe(
  const std::string& path, const SolveOptions& options)
  : impl_(new Impl(path, options)) {}
ArbitrarySidecarProbe::~ArbitrarySidecarProbe() { delete impl_; }
ArbitrarySidecarProbe::ArbitrarySidecarProbe(
  ArbitrarySidecarProbe&& other) noexcept
  : impl_(std::exchange(other.impl_, nullptr)) {}
ArbitrarySidecarProbe& ArbitrarySidecarProbe::operator=(
  ArbitrarySidecarProbe&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = std::exchange(other.impl_, nullptr);
    }
    return *this;
}
bool ArbitrarySidecarProbe::owner_forces(
  const PublicFrame& frame, const ProductWorld& actual,
  const ProductMask& worlds) const {
    if (!impl_) throw std::runtime_error("moved-from Jester/Ghost probe");
    return impl_->forces(frame, actual, worlds, true);
}
bool ArbitrarySidecarProbe::observer_forces(
  const PublicFrame& frame, const ProductWorld& actual,
  const ProductMask& worlds) const {
    if (!impl_) throw std::runtime_error("moved-from Jester/Ghost probe");
    return impl_->forces(frame, actual, worlds, false);
}
const ArbitrarySidecarCertificate&
ArbitrarySidecarProbe::certificate() const {
    if (!impl_) throw std::runtime_error("moved-from Jester/Ghost probe");
    return impl_->certificate();
}

SolveCertificate verify_exact_overlay(const SolveOptions& options) {
    const PackedTable concrete = load_concrete(options.sourceTable,
      StateCount, PieceType::Jester, PieceType::Ghost);
    std::ifstream input(options.outputOverlay, std::ios::binary);
    std::array<char, 160> header{};
    input.read(header.data(), header.size());
    if (!input || std::memcmp(header.data(), OverlayMagic, 8) ||
        little_u32(reinterpret_cast<const std::uint8_t*>(header.data()+8)) != 2 ||
        little_u32(reinterpret_cast<const std::uint8_t*>(header.data()+12)) !=
          static_cast<std::uint32_t>(PieceType::Jester) ||
        little_u32(reinterpret_cast<const std::uint8_t*>(header.data()+16)) !=
          static_cast<std::uint32_t>(PieceType::Ghost) ||
        little_u32(reinterpret_cast<const std::uint8_t*>(header.data()+20)) !=
          static_cast<std::uint32_t>(Color::White) ||
        little_u32(reinterpret_cast<const std::uint8_t*>(header.data()+24)) !=
          StateCount || std::string(header.data()+32,64) != options.sourceSha256 ||
        little_u32(reinterpret_cast<const std::uint8_t*>(header.data()+28)) != 2 ||
        std::string(header.data()+96,64) != options.modelSha256 ||
        concrete.sha != options.sourceSha256)
        throw std::runtime_error("UFIW2 verification binding mismatch");
    std::vector<std::uint8_t> flags(StateCount);
    input.read(reinterpret_cast<char*>(flags.data()), flags.size());
    if (!input || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("UFIW2 verification extent mismatch");
    SolveCertificate certificate;
    certificate.overlaySha256 = sha256_file(options.outputOverlay);
    std::array<std::set<std::string>,2> rootSets;
    for (std::uint32_t index=0; index<StateCount; ++index) {
        const ConcreteState state=decode_source(index);
        const std::size_t side=static_cast<std::size_t>(state.side);
        if ((flags[index] & ~std::uint8_t{7}) ||
            (!(flags[index] & 4) && (flags[index] & 3)))
            throw std::runtime_error("malformed UFIW2 force flags");
        const FramedWorld physical=source_to_product(state);
        const Position position=make_position(physical.frame,physical.world);
        const bool admitted=fresh_world_admission(physical.frame,physical.world)==
                            AdmissionVerdict::Admit;
        if(bool(flags[index]&4)!=admitted)
            throw std::runtime_error("UFIW2 admission flag disagrees with native causal admission");
        if (!(flags[index]&4)) {
            ++certificate.unreachableRealizations[side];
            ++certificate.unreachable[side]
              [static_cast<unsigned>(concrete.result(index))];
            continue;
        }
        ++certificate.legalRealizations[side];
        if(position.game_over()){
            if((flags[index]&3)!=terminal_flags(position))
                throw std::runtime_error("UFIW2 terminal force-bit residual");
            rootSets[side].insert(canonical_terminal_view(
              physical.frame,physical.world));
        }else{
            const ProductMask root=fresh_root(physical.frame,physical.world);
            const unsigned actual=product_variable(physical.frame,physical.world);
            if(!root.test(actual)){++certificate.partitionResidual;continue;}
            const CanonicalSet canonical=canonicalize_set(physical.frame,root);
            const std::uint32_t geometry=encode_geometry(canonical.value.frame);
            std::string key(sizeof(geometry)+sizeof(canonical.value.worlds),'\0');
            std::memcpy(key.data(),&geometry,sizeof(geometry));
            std::memcpy(key.data()+sizeof(geometry),&canonical.value.worlds,
                        sizeof(canonical.value.worlds));
            rootSets[side].insert(std::move(key));
        }
        const bool white=flags[index]&1, black=flags[index]&2;
        certificate.dualWinResidual += white&&black;
        const bool mover=state.side==Color::White?white:black;
        const bool opponent=state.side==Color::White?black:white;
        ++certificate.totals[side][mover?1:opponent?2:3];
    }
    for (std::size_t side=0; side<2; ++side) {
        certificate.informationSets[side]=rootSets[side].size();
        std::uint64_t conserved=0;
        for(unsigned result=1;result<4;++result)
            conserved += certificate.totals[side][result] +
                         certificate.unreachable[side][result];
        certificate.conservationResidual += conserved != StateCount/2;
        certificate.conservationResidual +=
          certificate.legalRealizations[side]+
          certificate.unreachableRealizations[side]!=StateCount/2;
    }
    if(certificate.dualWinResidual||certificate.conservationResidual||
       certificate.partitionResidual)
        throw std::runtime_error("UFIW2 verification residual");
    return certificate;
}

void exact_small_domain_self_test(const std::string& scratchPrefix) {
    ProductRobdd::Limits limits;
    limits.maxUpperNodes=10'000; limits.upperUniqueSlots=32'768;
    limits.upperCacheEntries=20'000; limits.budgetBytes=1ULL<<30;
    limits.lowerMaxNodes=10'000; limits.lowerUniqueSlots=32'768;
    limits.lowerApplyCacheEntries=20'000;
    limits.lowerUnaryCacheEntries=limits.lowerComposeCacheEntries=10'000;
    ProductRobdd bdd(scratchPrefix+".truth",limits,true);
    const auto a=bdd.variable(0), b=bdd.variable(1);
    const auto c=bdd.variable(80), d=bdd.variable(81);
    const auto formula=bdd.logical_or(bdd.logical_and(a,c),
                                      bdd.logical_and(b,d));
    for(unsigned bits=0;bits<16;++bits) {
        ProductMask mask;
        if(bits&1)mask.set(0);if(bits&2)mask.set(1);
        if(bits&4)mask.set(80);if(bits&8)mask.set(81);
        const bool expected=((bits&1)&&(bits&4))||((bits&2)&&(bits&8));
        if(bdd.evaluate(formula,mask)!=expected)
            throw std::runtime_error("product ROBDD truth-table residual");
    }
    std::array<ProductRobdd::Id,ProductVariables> image{};
    image.fill(bdd.constant(false)); image[0]=b;image[1]=a;image[80]=d;image[81]=c;
    const auto composed=bdd.compose(formula,image,0x1234);
    ProductMask allowed;for(unsigned variable:{0u,1u,80u,81u})allowed.set(variable);
    if(composed!=formula||!bdd.is_upward_closed(formula,allowed)||
       bdd.is_downward_closed(formula))
        throw std::runtime_error("product ROBDD composition/monotonicity residual");
    const auto domain=bdd.subset_of(allowed);ProductMask empty;
    if(!bdd.evaluate(domain,empty)||!bdd.is_downward_closed(domain))
        throw std::runtime_error("empty-belief/domain downward-closure residual");
    const auto syntheticOwner=bdd.logical_and(domain,a);
    const auto syntheticObserver=bdd.logical_and(domain,bdd.logical_not(a));
    if(bdd.logical_and(syntheticOwner,syntheticObserver)!=bdd.constant(false)||
       bdd.logical_and(syntheticOwner,syntheticOwner)==bdd.constant(false))
        throw std::runtime_error(
          "arbitrary owner/observer dual-force corruption regression");

    // A lower KGhost predicate is a suffix function. Its image is the union
    // of parent sources from both royal-assignment planes; mapping it into the
    // upper plane (or only one assignment) is a silent false-result bug.
    const std::vector<ProductRobdd::SuffixNode> suffix{{Squares,0,0},
      {Squares,1,1},{5,0,1}};
    const auto imported=bdd.import_suffix(suffix);
    std::array<ProductRobdd::Id,ProductVariables> lowerImage{};
    lowerImage.fill(bdd.constant(false));
    lowerImage[lower_ghost_suffix_variable(5)]=bdd.logical_or(
      bdd.variable(2),bdd.variable(82));
    const auto inherited=bdd.compose(imported[2],lowerImage,0x5678);
    for(unsigned bits=0;bits<4;++bits){ProductMask mask;
        if(bits&1)mask.set(2);if(bits&2)mask.set(82);
        if(bdd.evaluate(inherited,mask)!=(bits!=0))
            throw std::runtime_error("cross-assignment lower Ghost image residual");}

    // A captured-Jester transition may leave a terminal lower KGhost child.
    // White knows the concrete Ghost square and receives that child's exact
    // owner result; Black must still force the result over every source in the
    // indistinguishable transition observation.
    LowerGhostSidecar::Geometry terminalGeometry;
    const auto setLower=[](LowerGhostSidecar::Mask&mask,unsigned square){
        if(square<64)mask.low|=std::uint64_t{1}<<square;
        else mask.high|=std::uint16_t(1u<<(square-64));};
    LowerGhostState ownerWin{Role::GhostOwner,0,1,5,false};
    LowerGhostState observerWin{Role::GhostOwner,0,1,6,false};
    setLower(terminalGeometry.terminal,ownerWin.ghost);
    setLower(terminalGeometry.terminal,observerWin.ghost);
    setLower(terminalGeometry.terminalOwner,ownerWin.ghost);
    setLower(terminalGeometry.terminalObserver,observerWin.ghost);
    RuntimeRelation terminalRelation;terminalRelation.childTerminal=true;
    ProductMask ownerSource,observerSource,terminalBelief;
    ownerSource.set(0);observerSource.set(1);
    terminalBelief.set(0);terminalBelief.set(1);
    terminalRelation.lowerChildren.emplace(encode_lower_ghost(ownerWin),
                                            ownerSource);
    terminalRelation.lowerChildren.emplace(encode_lower_ghost(observerWin),
                                            observerSource);
    terminalRelation.badWhiteSources=observerSource;
    terminalRelation.badBlackSources=ownerSource;
    const auto ownerWhite=lower_terminal_formula(bdd,terminalRelation,
      terminalGeometry,Color::White,encode_lower_ghost(ownerWin));
    const auto ownerWhiteLoss=lower_terminal_formula(bdd,terminalRelation,
      terminalGeometry,Color::White,encode_lower_ghost(observerWin));
    const auto observerWhite=lower_terminal_formula(bdd,terminalRelation,
      terminalGeometry,Color::White,std::nullopt);
    const auto ownerBlack=lower_terminal_formula(bdd,terminalRelation,
      terminalGeometry,Color::Black,encode_lower_ghost(observerWin));
    const auto ownerBlackLoss=lower_terminal_formula(bdd,terminalRelation,
      terminalGeometry,Color::Black,encode_lower_ghost(ownerWin));
    const auto observerBlack=lower_terminal_formula(bdd,terminalRelation,
      terminalGeometry,Color::Black,std::nullopt);
    if(!bdd.evaluate(ownerWhite,terminalBelief)||
       bdd.evaluate(ownerWhiteLoss,terminalBelief)||
       bdd.evaluate(observerWhite,terminalBelief)||
       !bdd.evaluate(ownerBlack,terminalBelief)||
       bdd.evaluate(ownerBlackLoss,terminalBelief)||
       bdd.evaluate(observerBlack,terminalBelief))
        throw std::runtime_error(
          "terminal lower Ghost owner/observer specialization residual");

    // Find a native hidden-Ghost witness where two different private White
    // actions have the identical complete Black observation. The exact
    // successor information is their union, never two action-keyed beliefs.
    bool policyUnionWitness=false;
    for(std::uint32_t raw=0;raw<20'000&&!policyUnionWitness;++raw){
        const PublicFrame frame=decode_geometry(raw);
        if(frame.side!=Color::White||frame.visibleGhost)continue;
        struct Outcome{unsigned source;ActionKey action;unsigned child;};
        std::map<std::string,std::vector<Outcome>> grouped;
        for(const ProductWorld&world:admitted_fresh_worlds(frame)){
            Position before=make_position(frame,world);
            if(before.game_over())continue;
            for(const Move&move:before.legal_moves()){
                const int actor=before.piece_on(move.from);
                if(actor==Position::NoPiece||before.piece(actor).type!=PieceType::Ghost)continue;
                Position child=before;Undo undo;if(!child.make_move(move,undo))throw std::runtime_error("policy witness move failed");
                const auto physical=same_class_product(child);if(!physical)continue;
                std::string observation=transition_observation_key(before,move,child,{Color::Black,false});
                if(!child.game_over()&&child.side_to_move()==Color::Black)
                    observation+=decision_observation_key(child,{Color::Black,false});
                grouped[observation].push_back({product_variable(frame,world),
                  action_key(move),product_variable(physical->frame,physical->world)});
            }
        }
        for(const auto&[unused,outcomes]:grouped){(void)unused;
            if(policyUnionWitness)break;
            for(std::size_t first=0;first<outcomes.size()&&!policyUnionWitness;++first)
                for(std::size_t second=first+1;
                    second<outcomes.size()&&!policyUnionWitness;++second)
                    if(outcomes[first].source!=outcomes[second].source&&
                       !(outcomes[first].action==outcomes[second].action)){
                        ProductMask unionMask;unionMask.set(outcomes[first].source);
                        unionMask.set(outcomes[second].source);
                        TransitionCertificate compileCertificate;
                        const BuiltBlock compiled=build_block(frame,true,
                          compileCertificate);
                        const auto edgeFor=[&](const Outcome&outcome)->const EdgeDisk*{
                            const auto action=std::find_if(compiled.actions.begin(),
                              compiled.actions.end(),[&](const ActionDisk&item){
                                return item.key==outcome.action;});
                            if(action==compiled.actions.end())return nullptr;
                            const std::uint32_t actionId=static_cast<std::uint32_t>(
                              action-compiled.actions.begin());
                            for(std::uint32_t edge=compiled.header.edgeOffsets[outcome.source];
                                edge<compiled.header.edgeOffsets[outcome.source+1];++edge)
                                if(compiled.edges[edge].edge.action==actionId)
                                    return &compiled.edges[edge];
                            return nullptr;};
                        const EdgeDisk*firstEdge=edgeFor(outcomes[first]);
                        const EdgeDisk*secondEdge=edgeFor(outcomes[second]);
                        if(!firstEdge||!secondEdge||
                           firstEdge->edge.relation!=secondEdge->edge.relation)
                            throw std::runtime_error(
                              "compiled hidden actions did not share observation relation");
                        ProductMask compiledSources;
                        for(const EdgeDisk&edge:compiled.edges)
                            if(edge.edge.relation==firstEdge->edge.relation)
                                compiledSources.set(edge.sourceVariable);
                        if(!compiledSources.test(outcomes[first].source)||
                           !compiledSources.test(outcomes[second].source))
                            throw std::runtime_error(
                              "compiled observation relation lost policy outcomes");
                        policyUnionWitness=unionMask.count()==2;
                    }
        }
    }
    if(!policyUnionWitness)
        throw std::runtime_error("no global hidden-action observation-union witness");

    // The production D2 certificate must execute every semantic layer and
    // reject altered stored transition semantics, rather than merely proving
    // deterministic regeneration of the same bytes.
    bool d2Witness = false;
    bool d2PerturbationRejected = false;
    for (std::uint32_t raw = 0; raw < 20'000 && !d2Witness; ++raw) {
        const PublicFrame frame = decode_geometry(raw);
        if (canonical_geometry(frame).first != raw) continue;
        TransitionCertificate ignored;
        const BuiltBlock block = build_block(frame, false, ignored);
        if (block.edges.empty()) continue;
        TransitionCertificate checks;
        certify_d2_block(frame, block, checks);
        if (!checks.codecChecks || !checks.actionChecks ||
            !checks.decisionChecks || !checks.transitionChecks ||
            checks.symmetryChecks != 4 || checks.codecResidual ||
            checks.actionResidual || checks.decisionResidual ||
            checks.transitionResidual || checks.symmetryResidual)
            throw std::runtime_error("D2 semantic check-execution residual");
        try {
            TransitionCertificate perturbed;
            certify_d2_block(frame, block, perturbed, true);
        }
        catch (const std::exception&) {
            d2PerturbationRejected = true;
        }
        d2Witness = true;
    }
    if (!d2Witness || !d2PerturbationRejected)
        throw std::runtime_error(
          "D2 stored-transition perturbation was not rejected");

    // A reflected physical world generally has a different raw product
    // variable even though it belongs to the same canonical public-frame
    // orbit.  Each side must derive childActual from its own physical child;
    // comparing the two pre-canonical/raw IDs would be invalid (and becomes
    // especially important if a future material frame has a stabilizer).
    bool permutedProductWitness = false;
    for (std::uint32_t raw = 0; raw < 100 && !permutedProductWitness; ++raw) {
        const PublicFrame frame = decode_geometry(raw);
        for (const ProductWorld& world : geometric_worlds(frame)) {
            const FramedWorld mapped = transform_world(
              frame, world, RectangleTransform::Horizontal);
            if (product_variable(frame, world) ==
                product_variable(mapped.frame, mapped.world))
                continue;
            const Position sourcePosition = make_position(frame, world);
            const Position targetPosition = make_position(mapped.frame,
                                                          mapped.world);
            if (classify_child(sourcePosition).domain != ChildDomain::SameClass ||
                classify_child(targetPosition).domain != ChildDomain::SameClass)
                continue;
            const CompiledEdge sourceChild = encode_child(
              sourcePosition, FramedWorld{frame, world});
            const CompiledEdge targetChild = encode_child(
              targetPosition, mapped);
            const auto [sourceRaw, sourceTransform] =
              canonical_geometry(frame);
            const auto [targetRaw, targetTransform] =
              canonical_geometry(mapped.frame);
            const FramedWorld sourceCanonical = transform_world(
              frame, world, sourceTransform);
            const FramedWorld targetCanonical = transform_world(
              mapped.frame, mapped.world, targetTransform);
            if (sourceChild.childGeometry != sourceRaw ||
                targetChild.childGeometry != targetRaw ||
                sourceRaw != targetRaw ||
                sourceChild.childActual != product_variable(
                  sourceCanonical.frame, sourceCanonical.world) ||
                targetChild.childActual != product_variable(
                  targetCanonical.frame, targetCanonical.world))
                throw std::runtime_error(
                  "D2 canonical child-actual re-encoding residual");
            permutedProductWitness = true;
            break;
        }
    }
    if (!permutedProductWitness)
        throw std::runtime_error("no D2 product-variable permutation witness");

    bool rejectedTerminal=false;
    for(std::uint32_t raw=RawGeometryCount/2;
        raw<RawGeometryCount/2+20'000&&!rejectedTerminal;++raw){
        const PublicFrame frame=decode_geometry(raw);
        for(const ProductWorld&world:geometric_worlds(frame)){
            if(make_position(frame,world).game_over()&&
               fresh_world_admission(frame,world)==AdmissionVerdict::Reject){
                rejectedTerminal=true;break;
            }
        }
    }
    if(!rejectedTerminal)
        throw std::runtime_error("no rejected dense terminal admission witness");
    bool rootDedup=false;
    for(std::uint32_t raw=0;raw<20'000&&!rootDedup;++raw){
        const PublicFrame rootFrame=decode_geometry(raw);
        if(rootFrame.side!=Color::White||rootFrame.visibleGhost)continue;
        const std::vector<ProductWorld> rootWorlds=admitted_fresh_worlds(rootFrame);
        if(rootWorlds.size()<2)continue;
        const ProductMask first=fresh_root(rootFrame,rootWorlds[0]);
        const ProductMask second=fresh_root(rootFrame,rootWorlds[1]);
        rootDedup=first==second&&
          canonicalize_set(rootFrame,first).value==
          canonicalize_set(rootFrame,second).value;
    }
    if(!rootDedup)
        throw std::runtime_error("fresh-root set dedup/conservation residual");

    std::vector<ProductRobdd::Id> roots{formula,composed};
    auto compacted=bdd.compact(scratchPrefix+".truth-compact",
      scratchPrefix+".truth-remap",roots);
    if(compacted.second.structuralResidual||compacted.second.rootResidual)
        throw std::runtime_error("product ROBDD compaction residual");
    bdd=std::move(compacted.first);
    compacted=bdd.compact(scratchPrefix+".truth",
      scratchPrefix+".truth-remap",roots);
    bdd=std::move(compacted.first);
    compacted=bdd.compact(scratchPrefix+".truth-compact",
      scratchPrefix+".truth-remap",roots);
    if(compacted.second.structuralResidual||compacted.second.rootResidual)
        throw std::runtime_error("third ProductRobdd compaction residual");
    const auto arenaBytes=[&](const std::string&prefix){return
      file_bytes(prefix+".upper.nodes")+file_bytes(prefix+".upper.unique")+
      file_bytes(prefix+".lower.nodes")+file_bytes(prefix+".lower.unique");};
    if(arenaBytes(scratchPrefix+".truth")+
       arenaBytes(scratchPrefix+".truth-compact")!=
       2*ProductRobdd::required_bytes(limits))
        throw std::runtime_error("two-arena compaction disk-extent residual");

    // Serialize and mmap-query a genuine correlated, multi-world catalog.
    // This exercises the production UFJG1 node coordinate split, compact
    // owner rank, decision-cell containment, and whole-belief D2 transform.
    bdd=std::move(compacted.first);
    const PublicFrame sidecarFrame=decode_geometry(0);
    const std::vector<ProductWorld> sidecarWorlds=
      geometric_worlds(sidecarFrame);
    const ProductWorld firstWorld=sidecarWorlds.at(0);
    const ProductWorld secondWorld=sidecarWorlds.at(1);
    const unsigned firstVariable=product_variable(sidecarFrame,firstWorld);
    const unsigned secondVariable=product_variable(sidecarFrame,secondWorld);
    ProductMask sidecarBelief;sidecarBelief.set(firstVariable);
    sidecarBelief.set(secondVariable);
    const ProductRobdd::Id firstRoot=bdd.variable(firstVariable);
    const ProductRobdd::Id secondMember=bdd.variable(secondVariable);
    const ProductRobdd::Id secondRoot=bdd.constant(false);
    const ProductRobdd::Id observerRoot=bdd.logical_or(firstRoot,secondMember);
    ArbitraryHeaderDisk sidecarHeader;
    sidecarHeader.upperNodes=bdd.upper_node_count();
    sidecarHeader.lowerNodes=bdd.lower_node_count();
    sidecarHeader.geometries=1;sidecarHeader.strata=1;
    sidecarHeader.ownerRoots=2;sidecarHeader.upperOffset=sizeof(sidecarHeader);
    sidecarHeader.lowerOffset=sidecarHeader.upperOffset+
      sidecarHeader.upperNodes*sizeof(ArbitraryUpperNodeDisk);
    sidecarHeader.geometryOffset=sidecarHeader.lowerOffset+
      sidecarHeader.lowerNodes*sizeof(ArbitraryLowerNodeDisk);
    sidecarHeader.stratumOffset=sidecarHeader.geometryOffset+
      sizeof(ArbitraryGeometryDisk);
    sidecarHeader.ownerOffset=sidecarHeader.stratumOffset+sizeof(ProductMask);
    sidecarHeader.observerOffset=sidecarHeader.ownerOffset+
      2*sizeof(ProductRobdd::Id);
    const std::uint64_t sidecarExtent=sidecarHeader.observerOffset+
      sizeof(ProductRobdd::Id);
    sidecarHeader.payloadBytes=sidecarExtent-sizeof(sidecarHeader);
    sidecarHeader.semantics=arbitrary_semantics();
    SolveOptions sidecarOptions;
    sidecarOptions.sourceSha256=std::string(64,'1');
    sidecarOptions.modelSha256=std::string(64,'2');
    sidecarOptions.observationSha256=std::string(64,'3');
    sidecarOptions.lowerJesterOverlaySha256=std::string(64,'4');
    sidecarOptions.lowerGhostSidecarSha256=std::string(64,'5');
    copy_digest(sidecarHeader.sourceSha,sidecarOptions.sourceSha256,"test");
    copy_digest(sidecarHeader.modelSha,sidecarOptions.modelSha256,"test");
    copy_digest(sidecarHeader.observationSha,
                sidecarOptions.observationSha256,"test");
    copy_digest(sidecarHeader.transitionPayloadSha,std::string(64,'6'),"test");
    copy_digest(sidecarHeader.transitionHeaderSha,std::string(64,'7'),"test");
    copy_digest(sidecarHeader.transitionMarkerSha,std::string(64,'8'),"test");
    copy_digest(sidecarHeader.lowerJesterOverlaySha,
                sidecarOptions.lowerJesterOverlaySha256,"test");
    copy_digest(sidecarHeader.lowerGhostSidecarSha,
                sidecarOptions.lowerGhostSidecarSha256,"test");
    const std::string sidecarPath=scratchPrefix+".ufjg";
    std::ofstream sidecar(sidecarPath,std::ios::binary|std::ios::trunc);
    write_value(sidecar,sidecarHeader);
    write_sequence<ArbitraryUpperNodeDisk>(sidecar,
      sidecarHeader.upperNodes,[&](std::uint64_t id){const auto node=
        bdd.upper_node_record(id);ArbitraryUpperNodeDisk result;
        result.variable=node.variable;result.low=node.low;result.high=node.high;
        return result;},"test upper nodes");
    write_sequence<ArbitraryLowerNodeDisk>(sidecar,
      sidecarHeader.lowerNodes,[&](std::uint64_t id){const auto node=
        bdd.lower_node_record(static_cast<std::uint32_t>(id));
        ArbitraryLowerNodeDisk result;result.variable=node.variable;
        result.low=node.low;result.high=node.high;return result;},
      "test lower nodes");
    ArbitraryGeometryDisk sidecarGeometry;sidecarGeometry.raw=0;
    sidecarGeometry.ownerBase=0;sidecarGeometry.stratumBase=0;
    sidecarGeometry.stratumCount=1;sidecarGeometry.liveCount=2;
    sidecarGeometry.live=sidecarBelief;write_value(sidecar,sidecarGeometry);
    write_value(sidecar,sidecarBelief);write_value(sidecar,firstRoot);
    write_value(sidecar,secondRoot);write_value(sidecar,observerRoot);
    sidecar.close();
    const std::string sidecarPayload=sha256_range(sidecarPath,
      sizeof(sidecarHeader),sidecarHeader.payloadBytes);
    std::copy(sidecarPayload.begin(),sidecarPayload.end(),
              sidecarHeader.payloadSha.begin());
    std::fstream rewrite(sidecarPath,std::ios::binary|std::ios::in|
      std::ios::out);write_value(rewrite,sidecarHeader);rewrite.close();
    const ArbitrarySidecarCertificate sidecarCertificate=
      verify_arbitrary_sidecar(sidecarPath,sidecarOptions);
    if(sidecarCertificate.structuralResidual||
       sidecarCertificate.ownerRoots!=2)
        throw std::runtime_error("UFJG1 structural test residual");
    ArbitrarySidecarProbe probe(sidecarPath,sidecarOptions);
    if(!probe.owner_forces(sidecarFrame,firstWorld,sidecarBelief)||
       !probe.observer_forces(sidecarFrame,firstWorld,sidecarBelief)||
       probe.owner_forces(sidecarFrame,secondWorld,sidecarBelief))
        throw std::runtime_error("UFJG1 correlated multi-world query residual");
    ProductMask spanning=sidecarBelief;
    spanning.set(product_variable(sidecarFrame,sidecarWorlds.at(2)));
    bool spanningRejected=false;try{(void)probe.owner_forces(
      sidecarFrame,firstWorld,spanning);}catch(const std::invalid_argument&){
        spanningRejected=true;}
    if(!spanningRejected)
        throw std::runtime_error("UFJG1 spanning legal-dot cell accepted");
    ProductMask omittedSingleton;const ProductWorld omitted=sidecarWorlds.at(2);
    omittedSingleton.set(product_variable(sidecarFrame,omitted));
    bool omittedRejected=false;try{(void)probe.observer_forces(
      sidecarFrame,omitted,omittedSingleton);}catch(const std::invalid_argument&){
        omittedRejected=true;}
    if(!omittedRejected)
        throw std::runtime_error("UFJG1 terminal/nonlive actual accepted");
    const ProductSet transformed=transform_set(sidecarFrame,sidecarBelief,
      RectangleTransform::Both);
    const FramedWorld transformedActual=transform_world(sidecarFrame,
      firstWorld,RectangleTransform::Both);
    if(!probe.owner_forces(transformed.frame,transformedActual.world,
                           transformed.worlds)||
       !probe.observer_forces(transformed.frame,transformedActual.world,
                              transformed.worlds))
        throw std::runtime_error("UFJG1 D2 query residual");
    ArbitrarySidecarProbe moved(std::move(probe));
    bool movedRejected=false;try{(void)probe.certificate();}
    catch(const std::exception&){movedRejected=true;}
    if(!movedRejected||!moved.certificate().bytes)
        throw std::runtime_error("UFJG1 moved-from probe residual");
    const std::string malformedPath=scratchPrefix+".malformed.ufjg";
    {std::ifstream source(sidecarPath,std::ios::binary);std::ofstream target(
       malformedPath,std::ios::binary|std::ios::trunc);
     target<<source.rdbuf();}
    {std::fstream corrupt(malformedPath,std::ios::binary|std::ios::in|
       std::ios::out);corrupt.seekp(static_cast<std::streamoff>(
         sizeof(ArbitraryHeaderDisk)));char byte=0;corrupt.read(&byte,1);
       corrupt.seekp(static_cast<std::streamoff>(sizeof(ArbitraryHeaderDisk)));
       byte^=1;corrupt.write(&byte,1);}
    bool malformedRejected=false;try{(void)verify_arbitrary_sidecar(
      malformedPath,sidecarOptions);}catch(const std::exception&){
        malformedRejected=true;}
    if(!malformedRejected)
        throw std::runtime_error("malformed UFJG1 payload accepted");
    const auto copySidecar=[&](const std::string&destination){
        std::ifstream source(sidecarPath,std::ios::binary);
        std::ofstream target(destination,std::ios::binary|std::ios::trunc);
        target<<source.rdbuf();};
    const auto resignPayload=[&](const std::string&path,
                                 ArbitraryHeaderDisk&header){
        const std::string digest=sha256_range(path,sizeof(header),
                                              header.payloadBytes);
        std::copy(digest.begin(),digest.end(),header.payloadSha.begin());
        std::fstream output(path,std::ios::binary|std::ios::in|std::ios::out);
        write_value(output,header);};
    const std::string endianPath=scratchPrefix+".endian.ufjg";
    copySidecar(endianPath);ArbitraryHeaderDisk endianHeader=sidecarHeader;
    endianHeader.endian=0;{std::fstream output(endianPath,std::ios::binary|
      std::ios::in|std::ios::out);write_value(output,endianHeader);}
    bool endianRejected=false;try{(void)verify_arbitrary_sidecar(
      endianPath,sidecarOptions);}catch(const std::exception&){
        endianRejected=true;}
    if(!endianRejected)throw std::runtime_error("UFJG1 endian drift accepted");
    if(sidecarHeader.upperNodes<2)
        throw std::runtime_error("UFJG1 uniqueness test lacks upper nodes");
    const std::string duplicatePath=scratchPrefix+".duplicate.ufjg";
    copySidecar(duplicatePath);ArbitraryHeaderDisk duplicateHeader=sidecarHeader;
    {std::fstream file(duplicatePath,std::ios::binary|std::ios::in|
       std::ios::out);ArbitraryUpperNodeDisk prior;
     file.seekg(static_cast<std::streamoff>(duplicateHeader.upperOffset+
       (duplicateHeader.upperNodes-2)*sizeof(prior)));
     file.read(reinterpret_cast<char*>(&prior),sizeof(prior));
     file.seekp(static_cast<std::streamoff>(duplicateHeader.upperOffset+
       (duplicateHeader.upperNodes-1)*sizeof(prior)));
     file.write(reinterpret_cast<const char*>(&prior),sizeof(prior));}
    resignPayload(duplicatePath,duplicateHeader);
    bool duplicateRejected=false;try{(void)verify_arbitrary_sidecar(
      duplicatePath,sidecarOptions);}catch(const std::exception&){
        duplicateRejected=true;}
    if(!duplicateRejected)
        throw std::runtime_error("duplicate UFJG1 node tuple accepted");
    const std::string rootPath=scratchPrefix+".root-bound.ufjg";
    copySidecar(rootPath);ArbitraryHeaderDisk rootHeader=sidecarHeader;
    {std::fstream file(rootPath,std::ios::binary|std::ios::in|std::ios::out);
     const ProductRobdd::Id invalidRoot=rootHeader.upperNodes;
     file.seekp(static_cast<std::streamoff>(rootHeader.ownerOffset));
     write_value(file,invalidRoot);}
    resignPayload(rootPath,rootHeader);
    bool rootRejected=false;try{(void)verify_arbitrary_sidecar(
      rootPath,sidecarOptions);}catch(const std::exception&){rootRejected=true;}
    if(!rootRejected)throw std::runtime_error("UFJG1 root bound accepted");
}

} // namespace Stockfish::Ultimate::JesterGhostInformation
