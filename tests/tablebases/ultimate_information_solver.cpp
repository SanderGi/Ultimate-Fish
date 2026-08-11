/*
  Focused tests for Ultimate Fish's disk-backed monotone information solver.
  Build directly; this file is intentionally independent of the main test
  target so the tablebase graph producer can integrate concurrently.
*/

#include "information_solver.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Stockfish::Ultimate;

namespace {

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

void require_clean(const InformationSolveSummary &summary,
                   std::uint64_t variables, std::uint64_t activated) {
  require(summary.variables == variables, "summary variable count");
  require(summary.activated == activated, "summary activation count");
  require(summary.bellmanResidual == 0, "Bellman residual");
  require(summary.rankResidual == 0, "rank/witness residual");
}

void test_or_and_constants_and_witnesses() {
  InformationFixedPoint solver(8, "/tmp");
  solver.define_or(
      0, std::vector<InformationToken>{InformationFalse, InformationTrue});
  solver.define_and(1, std::vector<InformationToken>{0, InformationTrue});
  solver.define_or(2, std::vector<InformationToken>{InformationFalse});
  solver.define_and(3, std::vector<InformationToken>{});
  solver.define_and(4, std::vector<InformationToken>{0, InformationFalse});
  solver.define_or(5, std::vector<InformationToken>{2, 1});
  solver.define_and(
      6, std::vector<InformationToken>{InformationTrue, InformationTrue});
  solver.define_or(7, std::vector<InformationToken>{5, 0});

  const InformationSolveSummary summary = solver.solve();
  require_clean(summary, 8, 6);
  require(solver.value(InformationTrue), "true constant");
  require(!solver.value(InformationFalse), "false constant");
  require(solver.value(0) && solver.activation_rank(0) == 1 &&
              solver.witness_index(0) == 1,
          "constant OR witness");
  require(solver.value(1) && solver.activation_rank(1) == 2 &&
              solver.witness_index(1) == 0,
          "AND propagation witness");
  require(!solver.value(2) && solver.activation_rank(2) == 0,
          "false OR remains false");
  require(solver.value(3) && solver.activation_rank(3) == 1 &&
              solver.witness_index(3) == InformationNoWitness,
          "empty AND is true");
  require(!solver.value(4), "false constant blocks AND");
  require(solver.value(5) && solver.activation_rank(5) == 3 &&
              solver.witness_index(5) == 1,
          "OR propagated witness");
  require(solver.value(6) && solver.activation_rank(6) == 1 &&
              solver.witness_index(6) == 1,
          "constant AND witness");
  require(solver.value(7) && solver.activation_rank(7) == 2 &&
              solver.witness_index(7) == 1,
          "OR chooses minimum-rank child");
  require_clean(solver.verify(), 8, 6);
}

void test_cycles_use_the_least_fixed_point() {
  InformationFixedPoint solver(5, "/tmp");
  solver.define_or(0, std::vector<InformationToken>{1, InformationFalse});
  solver.define_or(1, std::vector<InformationToken>{0, InformationFalse});
  solver.define_or(2, std::vector<InformationToken>{2, InformationTrue});
  solver.define_and(3, std::vector<InformationToken>{4});
  solver.define_and(4, std::vector<InformationToken>{3});

  require_clean(solver.solve(), 5, 1);
  require(!solver.value(0) && !solver.value(1),
          "unsupported OR cycle stays outside least fixed point");
  require(solver.value(2) && solver.activation_rank(2) == 1,
          "constant seeds a self cycle");
  require(!solver.value(3) && !solver.value(4),
          "unsupported AND cycle stays outside least fixed point");
}

void test_or_of_pairs_is_an_or_of_action_gates() {
  InformationFixedPoint solver(6, "/tmp");
  solver.define_and(0, std::vector<InformationToken>{});
  solver.define_or(1, std::vector<InformationToken>{InformationFalse});
  solver.define_or_of_pairs(
      2, std::vector<InformationPair>{{0, 1}, {0, InformationTrue}});
  solver.define_or_of_pairs(3, std::vector<InformationPair>{{2, 0}});
  solver.define_or_of_pairs(4, std::vector<InformationPair>{{0, 0}});
  solver.define_or_of_pairs(5, std::vector<InformationPair>{
                                   {InformationTrue, InformationTrue}, {3, 0}});

  require_clean(solver.solve(), 6, 5);
  require(solver.value(2) && solver.activation_rank(2) == 2 &&
              solver.witness_index(2) == 1,
          "pair action witness");
  require(solver.value(3) && solver.activation_rank(3) == 3,
          "pair waits for both children");
  require(solver.value(4) && solver.activation_rank(4) == 2,
          "repeated child satisfies both pair positions exactly");
  require(solver.value(5) && solver.activation_rank(5) == 1 &&
              solver.witness_index(5) == 0,
          "constant pair seed");
}

void test_pair_rank_waits_for_settled_children() {
  // Processing variable 3 sees variable 5 activated at rank 3 but not yet
  // settled.  Pair 0 would incorrectly activate the parent at rank 4 if the
  // solver treated merely enqueued children as final.  Pair 1 becomes the
  // true minimum-rank witness when variable 4 settles.
  InformationFixedPoint solver(7, "/tmp");
  solver.define_and(0, std::vector<InformationToken>{});
  solver.define_and(1, std::vector<InformationToken>{});
  solver.define_or(2, std::vector<InformationToken>{0});
  solver.define_or(3, std::vector<InformationToken>{1});
  solver.define_or(4, std::vector<InformationToken>{1});
  solver.define_or(5, std::vector<InformationToken>{2});
  solver.define_or_of_pairs(6, std::vector<InformationPair>{{3, 5}, {3, 4}});

  require_clean(solver.solve(), 7, 7);
  require(solver.activation_rank(6) == 3,
          "pair rank uses the minimum satisfied action");
  require(solver.witness_index(6) == 1,
          "pair witness does not use an unsettled higher-rank child");
}

void test_empty_token_stream() {
  InformationFixedPoint solver(1, "/tmp");
  solver.define_and(0, std::vector<InformationToken>{});
  require_clean(solver.solve(), 1, 1);
  require(solver.activation_rank(0) == 1, "empty token stream maps safely");
}

void test_forward_references_and_disk_backed_chain() {
  constexpr std::uint32_t Count = 100'000;
  InformationFixedPoint solver(Count, "/tmp");
  for (std::uint32_t variable = 0; variable + 1 < Count; ++variable) {
    const InformationToken next = variable + 1;
    solver.define_or(variable, &next, 1);
  }
  const InformationToken seed = InformationTrue;
  solver.define_or(Count - 1, &seed, 1);

  const InformationSolveSummary summary = solver.solve();
  require_clean(summary, Count, Count);
  require(summary.reverseEdges == Count - 1, "chain reverse-edge count");
  require(solver.activation_rank(Count - 1) == 1, "chain seed rank");
  require(solver.activation_rank(0) == Count, "exact long-chain rank");
  require(solver.witness_index(0) == 0, "chain witness");
}

void test_definition_errors_are_rejected() {
  bool duplicate = false;
  try {
    InformationFixedPoint solver(1, "/tmp");
    solver.define_and(0, std::vector<InformationToken>{});
    solver.define_or(0, std::vector<InformationToken>{InformationTrue});
  } catch (const std::runtime_error &) {
    duplicate = true;
  }
  require(duplicate, "duplicate equation rejection");

  bool missing = false;
  try {
    InformationFixedPoint solver(2, "/tmp");
    solver.define_and(0, std::vector<InformationToken>{});
    (void)solver.solve();
  } catch (const std::runtime_error &) {
    missing = true;
  }
  require(missing, "missing equation rejection");
}

} // namespace

int main() {
  try {
    test_or_and_constants_and_witnesses();
    test_cycles_use_the_least_fixed_point();
    test_or_of_pairs_is_an_or_of_action_gates();
    test_pair_rank_waits_for_settled_children();
    test_empty_token_stream();
    test_forward_references_and_disk_backed_chain();
    test_definition_errors_are_rejected();
    std::cout << "ultimate information solver tests passed\n";
  } catch (const std::exception &error) {
    std::cerr << "ultimate information solver test failed: " << error.what()
              << '\n';
    return 1;
  }
  return 0;
}
