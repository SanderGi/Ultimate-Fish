/* Ultimate Fish - exact external ROBDD tests (GPLv3 or later). */

#include "../src/ultimate/external_robdd.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
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
      "/tmp/ultimate-external-robdd-remap", roots);
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
              << " cache_eviction_exact 1 budget_gate 1\n";
}
