/*
  Ultimate Fish - Chess Ultimate public-information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "information.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace Stockfish::Ultimate {
namespace {

constexpr int NoClass = -1;

[[nodiscard]] char color_name(Color color) {
    return color == Color::White ? 'w' : 'b';
}

[[nodiscard]] bool concealed_ghost(const PieceState& piece,
                                   const DisclosureContext& disclosure) {
    return piece.type == PieceType::Ghost && !piece.visible
        && piece.color != disclosure.observer;
}

[[nodiscard]] std::string public_type(const PieceState& piece,
                                      const DisclosureContext& disclosure) {
    if ((piece.type == PieceType::King || piece.type == PieceType::Jester)
        && !disclosure.knows_royal_identity(piece.color))
        return "royal";
    return std::string(Position::type_name(piece.type));
}

// An attached Angel follows its host while off board.  If that host is an
// invisible enemy Ghost, serializing the Angel's cached square would disclose
// the Ghost through a relationship field even though the Ghost itself was
// redacted.
[[nodiscard]] bool concealed_location(const Position& position, int id,
                                      const DisclosureContext& disclosure) {
    const PieceState& piece = position.piece(id);
    if (concealed_ghost(piece, disclosure))
        return true;
    if (piece.onBoard || piece.host < 0 || piece.host >= position.piece_count())
        return false;
    const PieceState& host = position.piece(piece.host);
    return host.alive && concealed_ghost(host, disclosure);
}

[[nodiscard]] std::string square_token(const Position& position, int id,
                                       const DisclosureContext& disclosure) {
    return concealed_location(position, id, disclosure)
         ? "?"
         : Position::square_name(position.piece(id).square);
}

struct PublicNode {
    int id = Position::NoPiece;
    std::string intrinsic;
    int link = Position::NoPiece;
    int host = Position::NoPiece;
    int refinementClass = NoClass;
};

[[nodiscard]] std::vector<int> normalized_attachment_orders(
  const Position& position) {
    std::vector<int> normalized(position.piece_count(), 0);
    std::map<int, std::vector<int>> byHost;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (piece.alive && piece.type == PieceType::Angel
            && piece.host >= 0 && piece.host < position.piece_count())
            byHost[piece.host].push_back(id);
    }
    for (auto& [host, angels] : byHost) {
        (void)host;
        std::sort(angels.begin(), angels.end(), [&](int lhs, int rhs) {
            const PieceState& a = position.piece(lhs);
            const PieceState& b = position.piece(rhs);
            // Equal orders are malformed, but the remaining fields keep the
            // projection deterministic without making allocation ID public.
            return std::tie(a.attachmentOrder, a.square, a.color, a.type)
                 < std::tie(b.attachmentOrder, b.square, b.color, b.type);
        });
        for (std::size_t rank = 0; rank < angels.size(); ++rank)
            normalized[angels[rank]] = static_cast<int>(rank + 1);
    }
    return normalized;
}

[[nodiscard]] std::string intrinsic_key(
  const Position& position,
  int id,
  int attachmentOrder,
  const DisclosureContext& disclosure) {
    const PieceState& piece = position.piece(id);
    std::ostringstream out;
    out << public_type(piece, disclosure) << ',' << color_name(piece.color)
        << ',' << square_token(position, id, disclosure)
        << ",board=" << int(piece.onBoard)
        << ",action=" << int(piece.action)
        << ",cooldown=" << int(piece.cooldown)
        << ",freeze=" << int(piece.freezeCount)
        << ",power=" << int(piece.power)
        << ",moved=" << int(piece.moved)
        << ",visible=" << int(piece.visible)
        << ",attachment=" << attachmentOrder;
    return out.str();
}

[[nodiscard]] int relation_class(const std::vector<int>& idToNode,
                                 const std::vector<PublicNode>& nodes,
                                 int relation) {
    if (relation < 0 || relation >= static_cast<int>(idToNode.size()))
        return NoClass;
    const int node = idToNode[relation];
    return node == Position::NoPiece ? NoClass : nodes[node].refinementClass;
}

[[nodiscard]] std::vector<PublicNode> public_nodes(
  const Position& position,
  const DisclosureContext& disclosure,
  std::vector<int>* finalIdToNode = nullptr) {
    const auto attachmentOrders = normalized_attachment_orders(position);
    std::vector<int> idToNode(position.piece_count(), Position::NoPiece);
    std::vector<PublicNode> nodes;
    nodes.reserve(position.piece_count());
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive)
            continue;
        idToNode[id] = static_cast<int>(nodes.size());
        nodes.push_back({id,
                         intrinsic_key(position, id, attachmentOrders[id], disclosure),
                         piece.link, piece.host, NoClass});
    }

    // Color refinement is exact for Ultimate's relationship graph: links are
    // disjoint Copycat/clone or Angel/Halo pairs, while host edges form
    // attachment stars ordered by the normalized attachment rank above.  It
    // removes internal piece IDs while retaining every rule-relevant edge.
    std::vector<std::string> intrinsicClasses;
    intrinsicClasses.reserve(nodes.size());
    for (const PublicNode& node : nodes)
        intrinsicClasses.push_back(node.intrinsic);
    std::sort(intrinsicClasses.begin(), intrinsicClasses.end());
    intrinsicClasses.erase(
      std::unique(intrinsicClasses.begin(), intrinsicClasses.end()),
      intrinsicClasses.end());
    for (PublicNode& node : nodes)
        node.refinementClass = static_cast<int>(std::lower_bound(
          intrinsicClasses.begin(), intrinsicClasses.end(), node.intrinsic)
          - intrinsicClasses.begin());

    for (std::size_t iteration = 0; iteration <= nodes.size(); ++iteration) {
        std::vector<std::string> signatures;
        signatures.reserve(nodes.size());
        for (const PublicNode& node : nodes) {
            std::ostringstream signature;
            signature << node.intrinsic
                      << "|link=" << relation_class(idToNode, nodes, node.link)
                      << "|host=" << relation_class(idToNode, nodes, node.host);
            signatures.push_back(signature.str());
        }

        std::vector<std::string> unique = signatures;
        std::sort(unique.begin(), unique.end());
        unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
        std::vector<int> next(nodes.size(), NoClass);
        for (std::size_t index = 0; index < nodes.size(); ++index)
            next[index] = static_cast<int>(std::lower_bound(
              unique.begin(), unique.end(), signatures[index]) - unique.begin());

        bool unchanged = true;
        for (std::size_t index = 0; index < nodes.size(); ++index)
            if (next[index] != nodes[index].refinementClass) {
                unchanged = false;
                break;
            }
        for (std::size_t index = 0; index < nodes.size(); ++index)
            nodes[index].refinementClass = next[index];
        if (unchanged)
            break;
    }

    if (finalIdToNode)
        *finalIdToNode = std::move(idToNode);
    return nodes;
}

[[nodiscard]] std::string terminal_token(const Position& position) {
    if (!position.game_over())
        return "ongoing";
    const std::optional<Color> winner = position.winner();
    if (!winner)
        return "draw";
    return *winner == Color::White ? "white" : "black";
}

[[nodiscard]] std::string forced_piece_token(
  const Position& position,
  const std::vector<int>& idToNode,
  const std::vector<PublicNode>& nodes) {
    const int forced = position.forced_piece();
    if (forced < 0 || forced >= static_cast<int>(idToNode.size()))
        return "-";
    const int node = idToNode[forced];
    return node == Position::NoPiece
         ? "-"
         : std::to_string(nodes[node].refinementClass);
}

// Preserve the en-passant victim as a normalized node class as well: ordinarily
// it is uniquely implied by the square, but custom analysis positions are
// allowed to spell it explicitly.
[[nodiscard]] std::string en_passant_victim_token(
  const Position& position,
  const std::vector<int>& idToNode,
  const std::vector<PublicNode>& nodes) {
    const int id = position.en_passant_victim();
    if (id < 0 || id >= static_cast<int>(idToNode.size()))
        return "-";
    const int node = idToNode[id];
    return node == Position::NoPiece
         ? "-"
         : std::to_string(nodes[node].refinementClass);
}

[[nodiscard]] std::string public_actor_type(
  const Position& position,
  int actor,
  const DisclosureContext& disclosure) {
    if (actor < 0 || actor >= position.piece_count())
        return "none";
    return public_type(position.piece(actor), disclosure);
}

[[nodiscard]] std::string move_kind_name(MoveKind kind) {
    switch (kind) {
    case MoveKind::Normal: return "move";
    case MoveKind::Castle: return "castle";
    case MoveKind::Swap: return "swap";
    case MoveKind::Spawn: return "spawn";
    case MoveKind::Shoot: return "shoot";
    case MoveKind::Pull: return "pull";
    case MoveKind::Link: return "link";
    case MoveKind::Pass: return "pass";
    }
    return "unknown";
}

}  // namespace

std::string view_key(const Position& position,
                     const DisclosureContext& disclosure) {
    std::vector<int> idToNode;
    const std::vector<PublicNode> nodes = public_nodes(
      position, disclosure, &idToNode);

    std::vector<std::string> records;
    records.reserve(nodes.size());
    for (const PublicNode& node : nodes) {
        std::ostringstream record;
        record << node.intrinsic
               << "|link=" << relation_class(idToNode, nodes, node.link)
               << "|host=" << relation_class(idToNode, nodes, node.host);
        records.push_back(record.str());
    }
    std::sort(records.begin(), records.end());

    std::ostringstream out;
    out << "UFVIEW1"
        << "|observer=" << color_name(disclosure.observer)
        << "|enemyKingKnown=" << int(disclosure.enemyKingKnown)
        << "|turn=" << color_name(position.side_to_move())
        << "|continuation=" << static_cast<int>(position.continuation())
        << "|forced=" << forced_piece_token(position, idToNode, nodes)
        << "|ep=" << Position::square_name(position.en_passant_square())
        << "|epVictim=" << en_passant_victim_token(position, idToNode, nodes)
        << "|timeout=";
    if (const std::optional<Color> winner = position.forced_timeout_winner())
        out << color_name(*winner);
    else
        out << '-';
    out << "|terminal=" << terminal_token(position)
        << "|pieces=" << records.size();
    for (const std::string& record : records)
        out << '|' << record.size() << ':' << record;
    return out.str();
}

std::string decision_observation_key(
  const Position& position,
  const DisclosureContext& disclosure) {
    if (disclosure.observer != position.side_to_move())
        throw std::invalid_argument(
          "legal-dot decision observation is private to the side to move");

    // Native Character.GetAvailableMoves/SetUpDot exposes one marker at a
    // board destination after selecting its source model.  It does not expose
    // Move::auxiliary, promotion choice, or a second internal action identity
    // when those produce the same rendered dot.  Deduplicating by endpoints is
    // therefore exact UI semantics, not a lossy engine-action projection.
    using Marker = std::pair<int, int>;
    constexpr Marker PassMarker{-1, -1};
    std::vector<Marker> markers;
    for (const Move& move : position.legal_moves()) {
        if (move.kind == MoveKind::Pass)
            markers.push_back(PassMarker);
        else
            markers.emplace_back(move.from, move.to);
    }
    std::sort(markers.begin(), markers.end());
    markers.erase(std::unique(markers.begin(), markers.end()), markers.end());

    const std::string ordinary = view_key(position, disclosure);
    std::ostringstream out;
    out << "UFDECISION1|view=" << ordinary.size() << ':' << ordinary
        << "|markers=" << markers.size();
    for (const auto [source, destination] : markers) {
        if (source < 0)
            out << "|pass";
        else
            out << '|' << Position::square_name(source)
                << '>' << Position::square_name(destination);
    }
    return out.str();
}

std::string transition_observation_key(
  const Position& before,
  const Move& move,
  const Position& after,
  const DisclosureContext& disclosure) {
    if (move.kind == MoveKind::Pass) {
        const std::string resultingView = view_key(after, disclosure);
        return "UFTRANS1|actor=none|kind=pass|from=-|to=-|promotion=-|view="
             + std::to_string(resultingView.size()) + ':' + resultingView;
    }

    const int actor = before.piece_on(move.from);
    const PieceState* actorBefore = actor >= 0 && actor < before.piece_count()
                                  ? &before.piece(actor)
                                  : nullptr;
    const bool enemyGhost = actorBefore
                         && actorBefore->type == PieceType::Ghost
                         && actorBefore->color != disclosure.observer;
    const bool sourceKnown = !enemyGhost || actorBefore->visible;

    bool destinationKnown = !enemyGhost;
    if (enemyGhost) {
        // Capturing Ghosts reveal on attack even if a Bomb/Goop effect removes
        // the model before it can remain in the resulting position.
        destinationKnown = before.is_capture(move);
        if (!destinationKnown && actor < after.piece_count()) {
            const PieceState& actorAfter = after.piece(actor);
            destinationKnown = actorAfter.alive && actorAfter.visible;
        }
    }

    std::string promotion = "-";
    if (move.promotion != PieceType::Count)
        promotion = std::string(Position::type_name(move.promotion));
    const std::string resultingView = view_key(after, disclosure);

    std::ostringstream out;
    out << "UFTRANS1"
        << "|actor=" << public_actor_type(before, actor, disclosure)
        << "|kind=" << move_kind_name(move.kind)
        << "|from=" << (sourceKnown ? Position::square_name(move.from) : "?")
        << "|to=" << (destinationKnown ? Position::square_name(move.to) : "?")
        << "|promotion=" << promotion
        << "|view=" << resultingView.size() << ':' << resultingView;
    return out.str();
}

}  // namespace Stockfish::Ultimate
