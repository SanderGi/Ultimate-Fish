/*
  Ultimate Fish - exact external-memory reduced ordered decision diagrams
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_EXTERNAL_ROBDD_H_INCLUDED
#define ULTIMATE_EXTERNAL_ROBDD_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Stockfish::Ultimate {

// Nodes are persisted as the collision-free 9-byte tuple
// (uint8 variable, uint32 low, uint32 high).  The compact open-address unique
// index stores only a uint32 node ID; every hit is checked against the complete
// tuple in the node arena, so the smaller index does not rely on fingerprints.
// Computed-table limits affect only memoization: once full, operations continue
// without inserting entries.
class ExternalRobdd {
  public:
    using Id = std::uint32_t;
    static constexpr Id False = 0;
    static constexpr Id True = 1;

    struct Limits {
        std::uint32_t variables = 80;
        std::uint32_t maxNodes = 500'000'000;
        std::uint64_t uniqueSlots = std::uint64_t{1} << 30;
        std::size_t applyCacheEntries = 200'000;
        std::size_t unaryCacheEntries = 100'000;
        std::size_t composeCacheEntries = 100'000;
        std::uint64_t budgetBytes = 97ULL << 30;
    };

    struct Node {
        std::uint8_t variable = 0;
        Id low = False;
        Id high = False;
    };

    struct CompactionCertificate {
        std::uint64_t roots = 0;
        std::uint64_t markedNodes = 0;
        std::uint64_t copiedNodes = 0;
        std::uint64_t structuralResidual = 0;
        std::uint64_t rootResidual = 0;
    };

    ExternalRobdd(const std::string& prefix, const Limits& limits,
                  bool create);
    ~ExternalRobdd();
    ExternalRobdd(ExternalRobdd&&) noexcept;
    ExternalRobdd& operator=(ExternalRobdd&&) noexcept;
    ExternalRobdd(const ExternalRobdd&) = delete;
    ExternalRobdd& operator=(const ExternalRobdd&) = delete;

    [[nodiscard]] Id variable(std::uint32_t variable) const;
    [[nodiscard]] Node node(Id id) const;
    [[nodiscard]] std::uint32_t variable_count() const;
    [[nodiscard]] std::uint32_t node_count() const;
    [[nodiscard]] const Limits& limits() const;

    [[nodiscard]] Id make(std::uint32_t variable, Id low, Id high);
    [[nodiscard]] Id logical_not(Id root);
    [[nodiscard]] Id logical_and(Id lhs, Id rhs);
    [[nodiscard]] Id logical_or(Id lhs, Id rhs);
    [[nodiscard]] Id ite(Id condition, Id whenTrue, Id whenFalse);
    // Exact logical implication without materializing lhs & !rhs.  This is a
    // read-only structural walk, so proof-only monotonicity checks do not add
    // nodes to the persistent arena or dirty its mmap.
    [[nodiscard]] bool implies(Id lhs, Id rhs);
    [[nodiscard]] Id any(std::uint64_t low, std::uint16_t high);
    [[nodiscard]] Id subset_of(std::uint64_t low, std::uint16_t high);
    [[nodiscard]] Id compose(Id root, const std::vector<Id>& image,
                             std::uint64_t relationId);
    [[nodiscard]] bool evaluate(Id root, std::uint64_t low,
                                std::uint16_t high) const;
    [[nodiscard]] bool is_upward_closed(Id root, std::uint64_t low,
                                        std::uint16_t high);
    [[nodiscard]] bool is_downward_closed(Id root, std::uint64_t low,
                                          std::uint16_t high);

    void clear_computed_caches();
    void flush();

    // Rebuild all nodes reachable from roots into replacementPrefix, updating
    // roots only after every recursive tuple has been copied and verified.
    // remapPath is a fixed uint32 mmap scratch array.  No semantic sampling is
    // involved: structuralResidual proves equality for every assignment.
    [[nodiscard]] std::pair<std::unique_ptr<ExternalRobdd>,
                            CompactionCertificate>
    compact(const std::string& replacementPrefix,
            const std::string& remapPath,
            std::vector<Id>& roots,
            std::uint32_t workers = 1);

    [[nodiscard]] static std::uint64_t required_bytes(const Limits& limits);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace Stockfish::Ultimate

#endif  // ULTIMATE_EXTERNAL_ROBDD_H_INCLUDED
