/*
  Ultimate Fish - exact K+Jester versus K+Ghost information model
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.
*/

#ifndef ULTIMATE_RECIPROCAL_JESTER_GHOST_INFORMATION_MODEL_H_INCLUDED
#define ULTIMATE_RECIPROCAL_JESTER_GHOST_INFORMATION_MODEL_H_INCLUDED

#include "jester_ghost_information_model.h"

namespace Stockfish::Ultimate::ReciprocalJesterGhostInformation {

// The physical codec/product universe is shared with K+Jester+Ghost-v-K:
// two royal assignments times eighty Ghost squares. Ownership is not shared.
// White knows only kingAtFirst; Black knows only ghost. Keeping this in a
// separate namespace prevents the same-side solver's fully-informed-owner
// assumptions from authenticating the reciprocal game.
using JesterGhostInformation::ActionKey;
using JesterGhostInformation::AdmissionVerdict;
using JesterGhostInformation::ChildDomain;
using JesterGhostInformation::ClassifiedChild;
using JesterGhostInformation::ConcreteState;
using JesterGhostInformation::DecisionBucket;
using JesterGhostInformation::FramedWorld;
using JesterGhostInformation::LowerGhostState;
using JesterGhostInformation::ProductMask;
using JesterGhostInformation::ProductSet;
using JesterGhostInformation::ProductVariables;
using JesterGhostInformation::ProductWorld;
using JesterGhostInformation::PublicFrame;
using JesterGhostInformation::Role;
using JesterGhostInformation::TransitionWorld;

using JesterGhostInformation::action_key;
using JesterGhostInformation::decode_lower_ghost;
using JesterGhostInformation::decode_product_mask;
using JesterGhostInformation::decode_product_variable;
using JesterGhostInformation::decode_source;
using JesterGhostInformation::encode_lower_ghost;
using JesterGhostInformation::encode_source;
using JesterGhostInformation::geometric_worlds;
using JesterGhostInformation::legal_actions;
using JesterGhostInformation::product_variable;
using JesterGhostInformation::source_to_product;

[[nodiscard]] Position make_position(const PublicFrame& frame,
                                     const ProductWorld& world);

// White cells retain one known royal assignment and may group Ghost squares.
// Black cells retain one known Ghost square and may group royal assignments.
// The mover-private legal-dot observation may refine either partition.
[[nodiscard]] std::vector<DecisionBucket> decision_partition(
  const PublicFrame& frame, const std::vector<ProductWorld>& worlds);

[[nodiscard]] ClassifiedChild classify_child(const Position& position);
[[nodiscard]] std::optional<FramedWorld> same_class_product(
  const Position& position);

// The observer's persistent owned fact is part of the observation cell. This
// helper therefore never merges White royal assignments for White or Ghost
// squares for Black even if their public transition strings happen to match.
[[nodiscard]] std::vector<std::vector<TransitionWorld>> transition_partition(
  const PublicFrame& frame, const std::vector<ProductWorld>& worlds,
  const ActionKey& action, Color observer);

[[nodiscard]] AdmissionVerdict fresh_world_admission(
  const PublicFrame& frame, const ProductWorld& world);
[[nodiscard]] std::vector<ProductWorld> admitted_fresh_worlds(
  const PublicFrame& frame);

}  // namespace Stockfish::Ultimate::ReciprocalJesterGhostInformation

#endif
