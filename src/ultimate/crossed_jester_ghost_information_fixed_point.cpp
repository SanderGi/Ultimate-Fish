/* Ultimate Fish exact crossed Jester/Ghost fixed point. GPLv3+. */

#include "crossed_jester_ghost_information_fixed_point.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace Stockfish::Ultimate::CrossedJesterGhostSolver {
namespace Model = CrossedJesterGhostInformation;

FixedPointSolution::FixedPointSolution(
  std::unique_ptr<InformationFixedPoint> solver,
  std::vector<std::uint32_t> atomBase,
  FixedPointCertificate certificate)
  : solver_(std::move(solver)), atomBase_(std::move(atomBase)),
    certificate_(certificate) {}

InformationToken FixedPointSolution::token(NodeId node,
                                           std::uint32_t atom) const {
    if (node + 1 >= atomBase_.size() ||
        atom >= atomBase_[node + 1] - atomBase_[node])
        throw std::out_of_range(
          "crossed fixed-point atom is outside the solved graph");
    return atomBase_[node] + atom;
}

bool FixedPointSolution::value(NodeId node, std::uint32_t atom) const {
    return solver_->value(token(node, atom));
}

std::uint32_t FixedPointSolution::activation_rank(
  NodeId node, std::uint32_t atom) const {
    return solver_->activation_rank(token(node, atom));
}

std::uint32_t FixedPointSolution::witness_index(
  NodeId node, std::uint32_t atom) const {
    return solver_->witness_index(token(node, atom));
}

const FixedPointCertificate& FixedPointSolution::certificate() const {
    return certificate_;
}

FixedPointSolution solve_closed_graph(
  Arena& arena, Color target, const LowerForceOracle& oracle,
  const std::string& scratchDirectory) {
    if (target != Color::White && target != Color::Black)
        throw std::invalid_argument("invalid crossed fixed-point target");
    if (arena.size() >= std::numeric_limits<NodeId>::max())
        throw std::overflow_error("crossed fixed-point graph exceeds uint32");

    FixedPointCertificate certificate;
    certificate.target = target;
    certificate.nodes = arena.size();
    std::vector<std::uint32_t> atomBase(arena.size() + 1);
    std::vector<std::uint32_t> gateBase(arena.size() + 1);
    for (NodeId node = 0; node < arena.size(); ++node) {
        const Model::KnowledgeState state = arena.node(node);
        const TargetBellmanPlan plan = arena.bellman_plan(node, target, false);
        if (plan.atoms.size() != state.atoms.size())
            ++certificate.equationResidual;
        const std::uint64_t nextAtoms =
          std::uint64_t(atomBase[node]) + state.atoms.size();
        const std::uint64_t nextGates =
          std::uint64_t(gateBase[node]) + plan.gates.size();
        if (nextAtoms >= InformationTrue || nextGates >= InformationTrue)
            throw std::overflow_error(
              "crossed fixed-point layout exceeds token domain");
        atomBase[node + 1] = static_cast<std::uint32_t>(nextAtoms);
        gateBase[node + 1] = static_cast<std::uint32_t>(nextGates);
    }
    certificate.atomVariables = atomBase.back();
    certificate.gateVariables = gateBase.back();
    certificate.totalVariables =
      certificate.atomVariables + certificate.gateVariables;
    if (!certificate.totalVariables ||
        certificate.totalVariables >= InformationTrue)
        throw std::overflow_error(
          "crossed fixed-point variables exceed token domain");
    if (certificate.equationResidual)
        throw std::runtime_error(
          "crossed fixed-point layout has an equation residual");

    auto solver = std::make_unique<InformationFixedPoint>(
      static_cast<std::uint32_t>(certificate.totalVariables),
      scratchDirectory);
    const auto atom_token = [&](NodeId node, std::uint32_t atom) {
        if (node + 1 >= atomBase.size() ||
            atom >= atomBase[node + 1] - atomBase[node])
            throw std::runtime_error(
              "crossed fixed-point internal atom is out of range");
        ++certificate.internalTokens;
        return static_cast<InformationToken>(atomBase[node] + atom);
    };
    const auto gate_token = [&](NodeId node, std::uint32_t gate) {
        if (node + 1 >= gateBase.size() ||
            gate >= gateBase[node + 1] - gateBase[node])
            throw std::runtime_error(
              "crossed fixed-point gate is out of range");
        return static_cast<InformationToken>(
          certificate.atomVariables + gateBase[node] + gate);
    };

    for (NodeId node = 0; node < arena.size(); ++node) {
        const NodeExpansion expansion = arena.regenerate(node, false);
        const TargetBellmanPlan plan = arena.bellman_plan(node, target, false);
        const auto child_token = [&](const ChildReference& reference) {
            if (reference.domain == Model::ChildDomain::SameClass) {
                if (!reference.node)
                    throw std::runtime_error(
                      "crossed fixed-point internal reference has no node");
                return atom_token(*reference.node, reference.childAtom);
            }
            const bool force = resolve_external_force(
              external_force_query(expansion, target, reference), oracle);
            if (force)
                ++certificate.externalTrue;
            else
                ++certificate.externalFalse;
            return force ? InformationTrue : InformationFalse;
        };

        for (std::uint32_t gate = 0; gate < plan.gates.size(); ++gate) {
            if (plan.gates[gate].action != gate)
                throw std::runtime_error(
                  "crossed fixed-point action gate IDs are not dense");
            std::vector<InformationToken> children;
            children.reserve(plan.gates[gate].children.size());
            for (const ChildReference& child : plan.gates[gate].children)
                children.push_back(child_token(child));
            solver->define_and(gate_token(node, gate), children);
        }
        for (std::uint32_t atom = 0; atom < plan.atoms.size(); ++atom) {
            const AtomEquationPlan& equation = plan.atoms[atom];
            if (equation.atom != atom)
                throw std::runtime_error(
                  "crossed fixed-point atom IDs are not dense");
            std::vector<InformationToken> children;
            if (equation.kind == EquationKind::Or) {
                if (!equation.children.empty())
                    throw std::runtime_error(
                      "crossed mover equation has direct children");
                children.reserve(equation.gates.size());
                for (const std::uint32_t gate : equation.gates)
                    children.push_back(gate_token(node, gate));
                solver->define_or(atom_token(node, atom), children);
            }
            else {
                if (!equation.gates.empty())
                    throw std::runtime_error(
                      "crossed nonmover equation has action gates");
                children.reserve(equation.children.size());
                for (const ChildReference& child : equation.children)
                    children.push_back(child_token(child));
                solver->define_and(atom_token(node, atom), children);
            }
        }
    }
    certificate.solve = solver->solve();
    if (certificate.solve.bellmanResidual ||
        certificate.solve.rankResidual)
        throw std::runtime_error(
          "crossed fixed-point solve certificate has a residual");
    return FixedPointSolution(
      std::move(solver), std::move(atomBase), certificate);
}

}  // namespace Stockfish::Ultimate::CrossedJesterGhostSolver
