/* Ultimate Fish - exact external ROBDD tests (GPLv3 or later). */

#include "external_robdd.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

int main() {
    using Stockfish::Ultimate::ExternalRobdd;
    ExternalRobdd::Limits limits;
    limits.variables = 8;
    limits.maxNodes = 20'000;
    // Deliberately force open-address collisions.  The compact index stores
    // only IDs, so correctness here proves that every collision is resolved by
    // comparing the authoritative full node tuple rather than a fingerprint.
    limits.uniqueSlots = 1u << 6;
    limits.applyCacheEntries = 32;
    limits.unaryCacheEntries = 16;
    limits.composeCacheEntries = 16;
    limits.budgetBytes = 1ULL << 30;
    ExternalRobdd bdd("/tmp/ultimate-external-robdd", limits, true);
    const auto x0 = bdd.variable(0);
    const auto x1 = bdd.variable(1);
    const auto x2 = bdd.variable(2);
    const auto x3 = bdd.variable(3);
    const auto conjunction = bdd.logical_and(x0, x1);
    const auto formula = bdd.logical_or(conjunction,
      bdd.logical_and(bdd.logical_not(x2), x3));
    if (bdd.logical_and(x0, x1) != conjunction ||
        !bdd.is_upward_closed(bdd.logical_or(x0, x1), 0xff, 0) ||
        !bdd.is_downward_closed(
          bdd.logical_not(conjunction), 0xff, 0))
        throw std::runtime_error("external ROBDD reduction/monotonicity failed");
    std::array<bool, 256> expected{};
    for (unsigned assignment = 0; assignment < expected.size(); ++assignment) {
        expected[assignment] =
          ((assignment & 1u) && (assignment & 2u)) ||
          (!(assignment & 4u) && (assignment & 8u));
        if (bdd.evaluate(formula, assignment, 0) != expected[assignment])
            throw std::runtime_error("external ROBDD truth table mismatch");
    }
    const std::array<ExternalRobdd::Id, 7> implicationRoots{
      ExternalRobdd::False, ExternalRobdd::True, x0, x1, conjunction,
      formula, bdd.logical_not(formula)};
    const std::uint32_t beforeImplication = bdd.node_count();
    for (const auto lhs : implicationRoots)
        for (const auto rhs : implicationRoots) {
            bool expectedImplication = true;
            for (unsigned assignment = 0; assignment < expected.size();
                 ++assignment)
                expectedImplication &= !bdd.evaluate(lhs, assignment, 0) ||
                                       bdd.evaluate(rhs, assignment, 0);
            if (bdd.implies(lhs, rhs) != expectedImplication)
                throw std::runtime_error(
                  "external ROBDD implication mismatch");
        }
    if (bdd.node_count() != beforeImplication)
        throw std::runtime_error(
          "external ROBDD implication dirtied the node arena");

    // Geometry Bellman sweeps share one exact arena.  Exercise concurrent
    // recursive computed tables and unique-node admission, then prove every
    // result exhaustively rather than relying on scheduling-dependent IDs.
    std::array<ExternalRobdd::Id, 8> concurrentRoots{};
    std::vector<std::thread> threads;
    for (unsigned worker = 0; worker < concurrentRoots.size(); ++worker) {
        threads.emplace_back([&, worker] {
            ExternalRobdd::Id root = ExternalRobdd::False;
            for (unsigned repeat = 0; repeat < 1'000; ++repeat) {
                const auto a = bdd.variable(worker % 4);
                const auto b = bdd.variable((worker + 1) % 4);
                const auto c = bdd.variable((worker + 2) % 4);
                root = bdd.logical_or(
                  bdd.logical_and(a, b), bdd.logical_not(c));
                if (repeat % 31 == 0)
                    bdd.clear_computed_caches();
            }
            concurrentRoots[worker] = root;
        });
    }
    for (std::thread& thread : threads)
        thread.join();
    for (unsigned worker = 0; worker < concurrentRoots.size(); ++worker)
        for (unsigned assignment = 0; assignment < expected.size();
             ++assignment) {
            const bool a = assignment & (1u << (worker % 4));
            const bool b = assignment & (1u << ((worker + 1) % 4));
            const bool c = assignment & (1u << ((worker + 2) % 4));
            if (bdd.evaluate(concurrentRoots[worker], assignment, 0) !=
                ((a && b) || !c))
                throw std::runtime_error(
                  "concurrent external ROBDD truth table mismatch");
        }
    const std::uint32_t persistedNodes = bdd.node_count();
    bdd.flush();

    // A completed fixed point must be reusable without replaying all earlier
    // Bellman iterations.  Reopening discovers the dense logical prefix from
    // the preallocated arena, validates it, and retains collision-free unique
    // reduction for both old and newly appended tuples.
    ExternalRobdd reopened(
      "/tmp/ultimate-external-robdd", limits, false);
    if (reopened.node_count() != persistedNodes ||
        reopened.logical_and(reopened.variable(0), reopened.variable(1)) !=
          conjunction)
        throw std::runtime_error("external ROBDD fixed-point reopen failed");
    for (unsigned assignment = 0; assignment < expected.size(); ++assignment)
        if (reopened.evaluate(formula, assignment, 0) != expected[assignment])
            throw std::runtime_error(
              "reopened external ROBDD changed a persisted function");

    // A fixed point may legitimately outgrow its original acceleration-index
    // load factor before it exhausts the authoritative node arena.  Expanding
    // the disposable unique table must retain every persisted ID and allow
    // exact new reduction without rewriting the old checkpoint files.
    ExternalRobdd::Limits expandedLimits = limits;
    expandedLimits.uniqueSlots *= 2;
    ExternalRobdd expanded(
      "/tmp/ultimate-external-robdd", expandedLimits, false);
    if (expanded.node_count() != persistedNodes ||
        expanded.logical_and(expanded.variable(0), expanded.variable(1)) !=
          conjunction)
        throw std::runtime_error("external ROBDD unique rehash failed");
    for (unsigned assignment = 0; assignment < expected.size(); ++assignment)
        if (expanded.evaluate(formula, assignment, 0) != expected[assignment])
            throw std::runtime_error(
              "expanded-index external ROBDD changed a persisted function");

    std::vector<ExternalRobdd::Id> image(8);
    for (unsigned variable = 0; variable < image.size(); ++variable)
        image[variable] = bdd.variable(variable);
    std::swap(image[0], image[1]);
    if (bdd.compose(formula, image, 1) != formula)
        throw std::runtime_error("external ROBDD composition failed");

    // Create unreachable exact nodes before compaction.
    (void)bdd.logical_and(bdd.variable(4), bdd.variable(5));
    (void)bdd.logical_or(bdd.variable(6), bdd.variable(7));
    std::vector<ExternalRobdd::Id> roots{formula, conjunction};
    auto [compacted, certificate] = bdd.compact(
      "/tmp/ultimate-external-robdd-compact",
      "/tmp/ultimate-external-robdd-remap", roots, 4);
    if (certificate.structuralResidual || certificate.rootResidual ||
        certificate.roots != roots.size() ||
        compacted->node_count() > bdd.node_count())
        throw std::runtime_error("external ROBDD compaction certificate failed");
    for (unsigned assignment = 0; assignment < expected.size(); ++assignment)
        if (compacted->evaluate(roots[0], assignment, 0) !=
            expected[assignment])
            throw std::runtime_error("compacted ROBDD changed a root function");

    ExternalRobdd::Limits uncached = limits;
    uncached.applyCacheEntries = 0;
    uncached.unaryCacheEntries = 0;
    uncached.composeCacheEntries = 0;
    ExternalRobdd noCache(
      "/tmp/ultimate-external-robdd-uncached", uncached, true);
    const auto uncachedFormula = noCache.logical_or(
      noCache.logical_and(noCache.variable(0), noCache.variable(1)),
      noCache.logical_and(noCache.logical_not(noCache.variable(2)),
                          noCache.variable(3)));
    for (unsigned assignment = 0; assignment < expected.size(); ++assignment)
        if (noCache.evaluate(uncachedFormula, assignment, 0) !=
            expected[assignment])
            throw std::runtime_error("bounded-cache eviction changed semantics");

    ExternalRobdd::Limits overBudget = limits;
    overBudget.budgetBytes = 1;
    bool rejected = false;
    try {
        (void)ExternalRobdd(
          "/tmp/ultimate-external-robdd-over-budget", overBudget, true);
    }
    catch (const std::runtime_error&) {
        rejected = true;
    }
    if (!rejected)
        throw std::runtime_error("external ROBDD budget gate did not reject");
    std::cout << "external_robdd nodes " << bdd.node_count()
              << " compact_nodes " << compacted->node_count()
              << " marked " << certificate.markedNodes
              << " structural_residual 0 root_residual 0"
              << " implication_read_only 1 reopen_exact 1 unique_rehash_exact 1"
              << " cache_eviction_exact 1 budget_gate 1\n";
}
