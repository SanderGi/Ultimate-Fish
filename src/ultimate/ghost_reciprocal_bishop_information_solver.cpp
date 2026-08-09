/*
  Ultimate Fish - exact reciprocal Bishop/Ghost information adapter
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.

  The reciprocal solver textually reuses the frozen public-extra kernel.  Its
  inherited-lower regression must, however, respect the reciprocal ownership
  frame: Black owns the Ghost, so Black's legal dots are private and cannot be
  requested using White's disclosure context.  Keeping that narrow adapter in
  this translation unit avoids changing fingerprints for other public-extra
  domains which authenticate the shared implementation byte-for-byte.
*/

#include "ghost_public_extra_information_solver.h"

#define solve_exact frozen_reciprocal_solve_exact
#include "ghost_public_extra_information_solver.cpp"
#undef solve_exact

namespace Stockfish::Ultimate::GhostPublicExtraExact {
namespace {

void reciprocal_inherited_lower_mask_self_test(
  ExternalGhostExtraFixedPoint& solver) {
    struct World {
        std::uint8_t parentActual = 0;
        std::uint8_t childActual = 0;
        std::uint32_t childGeometry = NoIndex;
        std::uint32_t childStratum = NoIndex;
    };
    if (solver.material_.ghostColor != Color::Black ||
        solver.material_.observer() != Color::White)
        throw std::runtime_error(
          "reciprocal inherited-mask test has the wrong ownership frame");
    const PublicExtraGeometry raw{
      static_cast<std::uint8_t>(Color::Black), 0, 79, 78, 0};
    const auto [unusedGeometry, parentTransform] = solver.domain_.locate(raw);
    (void)unusedGeometry;
    const DisclosureContext observer{solver.material_.observer(), false};
    std::map<std::string, std::vector<World>> groups;
    std::uint64_t privateDotQueries = 0;
    for (std::uint8_t ghost = 0; ghost < Variables; ++ghost) {
        if (ghost == raw.whiteKing || ghost == raw.blackKing ||
            ghost == raw.bishop)
            continue;
        Position position = make_geometry_position(raw, ghost,
                                                    solver.material_);
        if (position.game_over())
            continue;
        std::string decision;
        if (position.side_to_move() == observer.observer) {
            decision = decision_observation_key(position, observer);
            ++privateDotQueries;
        }
        for (const Move& move : position.legal_moves()) {
            if (move.from != raw.blackKing || move.to != raw.bishop)
                continue;
            Position child = position;
            Undo undo;
            if (!child.make_move(move, undo))
                throw std::runtime_error(
                  "reciprocal lower inherited-mask fixture move failed");
            const ClassifiedChild classified = classify_child(
              child, solver.material_);
            if (classified.domain != ChildDomain::LowerGhost)
                throw std::runtime_error(
                  "reciprocal inherited-mask fixture escaped KGhost");
            const CanonicalLowerSignature signature =
              canonical_lower_signature(classified.index);
            const auto located = solver.lower_.locate(
              signature.side, signature.ownerKing,
              signature.observerKing, signature.visible != 0,
              signature.actual);
            const auto& lowerGeometry = solver.lower_.geometry(
              located.geometry);
            if (lowerGeometry.visible ||
                lowerGeometry.actualStratum[located.actual] == NoIndex)
                continue;
            const std::string observation = complete_transition_observation(
              position, move, child, observer);
            std::ostringstream key;
            key << decision.size() << ':' << decision << '|'
                << observation.size() << ':' << observation << '|'
                << located.geometry << '|'
                << lowerGeometry.actualStratum[located.actual];
            groups[key.str()].push_back({
              rectangle_transform_square(ghost, parentTransform),
              located.actual, located.geometry,
              lowerGeometry.actualStratum[located.actual]});
        }
    }
    if (privateDotQueries)
        throw std::runtime_error(
          "reciprocal owner action leaked a private legal-dot query");
    const auto selected = std::max_element(groups.begin(), groups.end(),
      [](const auto& lhs, const auto& rhs) {
          return lhs.second.size() < rhs.second.size();
      });
    if (selected == groups.end() || selected->second.size() < 2)
        throw std::runtime_error(
          "reciprocal inherited-mask regression lacks a non-singleton bucket");
    const std::vector<World>& worlds = selected->second;
    ExternalMask parentMask;
    ExternalMask childMask;
    std::map<std::uint8_t, ExternalMask> imageMasks;
    for (const World& world : worlds) {
        external_mask_set(parentMask, world.parentActual);
        external_mask_set(childMask, world.childActual);
        external_mask_set(imageMasks[world.childActual], world.parentActual);
        if (world.childGeometry != worlds.front().childGeometry ||
            world.childStratum != worlds.front().childStratum)
            throw std::runtime_error(
              "reciprocal inherited-mask observation mixes public children");
    }
    std::vector<ExternalRobdd::Id> image(Variables, ExternalRobdd::False);
    for (const auto& [child, sources] : imageMasks)
        image[child] = solver.mask_any(sources);
    std::uint64_t residual = 0;
    const auto& lowerGeometry = solver.lower_.geometry(
      worlds.front().childGeometry);
    for (const World& world : worlds) {
        const std::uint32_t sourceRoot =
          lowerGeometry.ownerRoot[world.childActual];
        const bool expected = solver.lower_.evaluate(sourceRoot, childMask);
        const ExternalRobdd::Id composed = solver.bdd_->compose(
          solver.lowerImport_.root(sourceRoot), image,
          0xf000000000000011ULL);
        residual += solver.bdd_->evaluate(
          composed, parentMask.low, parentMask.high) != expected;
    }
    const std::uint32_t observerSource = solver.lower_.stratum(
      worlds.front().childStratum).observerRoot;
    const bool observerExpected = solver.lower_.evaluate(
      observerSource, childMask);
    const ExternalRobdd::Id observerComposed = solver.bdd_->compose(
      solver.lowerImport_.root(observerSource), image,
      0xf000000000000012ULL);
    residual += solver.bdd_->evaluate(observerComposed, parentMask.low,
                                      parentMask.high) != observerExpected;
    if (residual)
        throw std::runtime_error(
          "reciprocal inherited-mask composition residual is nonzero");
    std::cout << "reciprocal_lower_inherited_mask sources "
              << worlds.size() << " child_memberships "
              << __builtin_popcountll(childMask.low) +
                   __builtin_popcount(childMask.high)
              << " owner_roots " << worlds.size()
              << " observer_roots 1 private_dot_queries 0 residual 0\n"
              << std::flush;
}

}  // namespace

SolveCertificate solve_exact(const SolveOptions& options) {
    if (options.measureIterations == 0 &&
        (options.outputOverlay.empty() || options.outputArbitrary.empty()))
        throw std::invalid_argument(
          "reciprocal exact solve requires UFIW2 and UFGX2 outputs");
    if (sha256_file(options.lowerGhostSidecar) !=
        options.lowerGhostSidecarSha256)
        throw std::runtime_error("lower Ghost sidecar full SHA mismatch");
    verify_transitions(options.transitionPrefix);
    const MaterialSpec reciprocal = reciprocal_material();
    PackedFourTable concrete(options.sourceTable, reciprocal);
    if (hex_digest(concrete.sha()) != options.sourceSha256)
        throw std::runtime_error("reciprocal concrete source SHA mismatch");
    ExternalTransitionDatabase database(options.transitionPrefix, reciprocal);
    ExternalGhostExtraSolveOptions legacy;
    legacy.scratch = options.scratchPrefix;
    legacy.output = options.outputOverlay;
    legacy.sourceSha256 = options.sourceSha256;
    legacy.modelSha256 = options.modelSha256;
    legacy.observationSha256 = options.observationSha256;
    legacy.bddLimits.maxNodes = options.maxNodes;
    legacy.bddLimits.uniqueSlots = options.uniqueSlots;
    legacy.bddLimits.applyCacheEntries = options.applyCacheEntries;
    legacy.bddLimits.unaryCacheEntries = options.unaryCacheEntries;
    legacy.bddLimits.composeCacheEntries = options.composeCacheEntries;
    legacy.bddLimits.budgetBytes = options.bddBudgetBytes;
    legacy.compactEvery = options.compactEvery;
    legacy.measureIterations = options.measureIterations;
    gate_external_ghost_extra_solve(options.transitionPrefix, database, legacy);
    LowerGhostSymbolicSidecar lower(options.lowerGhostSidecar,
      options.lowerGhostSourceSha256, options.lowerGhostModelSha256,
      options.lowerGhostObservationSha256);
    ExtraGeometryDomain domain;

    ExternalGhostExtraFixedPoint solver(database, lower, concrete, domain,
      constructor_material(), legacy);
    solver.material_ = reciprocal;
    reciprocal_inherited_lower_mask_self_test(solver);
    reciprocal_fresh_admission_self_test();
    const bool proofComplete = run_reciprocal_fixed_point(solver, domain);
    SolveCertificate certificate;
    if (!proofComplete)
        return certificate;
    report_reciprocal_fresh_roots(solver);
    certificate = write_sidecar(options.outputArbitrary, options, solver,
                                std::move(certificate));
    ArbitrarySidecarProbe probe(options.outputArbitrary, options);
    certificate.arbitraryStructuralResidual +=
      probe.certificate().arbitraryStructuralResidual;
    prove_sidecar_singletons(probe, solver, certificate);
    return certificate;
}

}  // namespace Stockfish::Ultimate::GhostPublicExtraExact
