/*
  Ultimate Fish exact retrograde tablebase generator
  GPLv3 or later
*/

#include "position.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t SquareCount = Position::BoardSquares;
constexpr std::uint32_t StateCount =
  2 * SquareCount * (SquareCount - 1) * (SquareCount - 2);

enum class Wdl : std::uint8_t { Unknown, Win, Loss, Draw };

struct State {
    Color side;
    std::uint8_t whiteKing;
    std::uint8_t blackKing;
    std::uint8_t attacker;
};

struct Node {
    Wdl wdl = Wdl::Unknown;
    std::uint16_t dtw = 0;
    std::uint16_t remaining = 0;
    std::uint16_t longestWinChild = 0;
};

std::uint32_t encode(const State& state) {
    const std::uint32_t blackRank = state.blackKing - (state.blackKing > state.whiteKing);
    const std::uint8_t low = std::min(state.whiteKing, state.blackKing);
    const std::uint8_t high = std::max(state.whiteKing, state.blackKing);
    const std::uint32_t attackerRank = state.attacker - (state.attacker > low)
                                                    - (state.attacker > high);
    return (((static_cast<std::uint32_t>(state.side) * SquareCount + state.whiteKing)
             * (SquareCount - 1) + blackRank)
            * (SquareCount - 2) + attackerRank);
}

State decode(std::uint32_t index) {
    const std::uint32_t attackerRank = index % (SquareCount - 2);
    index /= SquareCount - 2;
    const std::uint32_t blackRank = index % (SquareCount - 1);
    index /= SquareCount - 1;
    const std::uint8_t whiteKing = index % SquareCount;
    const Color side = static_cast<Color>(index / SquareCount);
    const std::uint8_t blackKing = blackRank + (blackRank >= whiteKing);
    const std::uint8_t low = std::min(whiteKing, blackKing);
    const std::uint8_t high = std::max(whiteKing, blackKing);
    std::uint8_t attacker = attackerRank;
    if (attacker >= low)
        ++attacker;
    if (attacker >= high)
        ++attacker;
    return {side, whiteKing, blackKing, attacker};
}

const char* wdl_name(Wdl wdl) {
    switch (wdl) {
    case Wdl::Win: return "win";
    case Wdl::Loss: return "loss";
    case Wdl::Draw: return "draw";
    default: return "unknown";
    }
}

bool closed_stateless_attacker(PieceType type) {
    switch (type) {
    case PieceType::Queen:
    case PieceType::Rook:
    case PieceType::Bishop:
    case PieceType::Knight:
    case PieceType::Ninja:
    case PieceType::Turtle:
    case PieceType::Dragon:
        return true;
    default:
        return false;
    }
}

}  // namespace

class TablebaseGenerator {
   public:
    TablebaseGenerator(PieceType attackerType, std::string output,
                       std::string checkpoint, std::uint32_t checkpointEvery) :
        attackerType_(attackerType), output_(std::move(output)),
        checkpoint_(std::move(checkpoint)), checkpointEvery_(checkpointEvery),
        nodes_(StateCount), predecessorCounts_(StateCount) {}

    void self_test() const {
        for (std::uint32_t index = 0; index < StateCount; ++index) {
            const State state = decode(index);
            if (state.whiteKing == state.blackKing || state.whiteKing == state.attacker ||
                state.blackKing == state.attacker || encode(state) != index)
                throw std::runtime_error("tablebase state codec is not bijective");
        }
        std::cout << "codecok states " << StateCount << '\n';
    }

    void dry_run(std::uint32_t count) const {
        count = std::min(count, StateCount);
        std::uint64_t edges = 0;
        for (std::uint32_t index = 0; index < count; ++index) {
            Position position = make_position(decode(index));
            for (const Move& move : position.legal_moves()) {
                Position child = position;
                if (!child.apply_move_unchecked(move))
                    throw std::runtime_error("legal tablebase move failed trusted application");
                ++edges;
                if (in_class(child))
                    (void) child_index(child);
            }
        }
        std::cout << "dryrun states " << count << " edges " << edges << '\n';
    }

    void generate() {
        const auto start = std::chrono::steady_clock::now();
        std::uint32_t begin = load_checkpoint();
        for (std::uint32_t index = begin; index < StateCount; ++index) {
            analyze_node(index, true, [](std::uint32_t) {});
            if (checkpointEvery_ && (index + 1) % checkpointEvery_ == 0) {
                save_checkpoint(index + 1);
                progress("frontier", index + 1, start);
            }
        }
        save_checkpoint(StateCount);

        std::vector<std::uint32_t> offsets(StateCount + 1, 0);
        for (std::uint32_t index = 0; index < StateCount; ++index) {
            const std::uint64_t next = std::uint64_t(offsets[index]) + predecessorCounts_[index];
            if (next > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("tablebase predecessor graph exceeds 32-bit storage");
            offsets[index + 1] = static_cast<std::uint32_t>(next);
        }
        std::vector<std::uint32_t> predecessors(offsets.back());
        std::vector<std::uint32_t> cursor(offsets.begin(), offsets.end() - 1);
        for (std::uint32_t index = 0; index < StateCount; ++index) {
            analyze_node(index, false, [&](std::uint32_t child) {
                predecessors[cursor[child]++] = index;
            });
            if (checkpointEvery_ && (index + 1) % checkpointEvery_ == 0)
                progress("reverse", index + 1, start);
        }

        std::deque<std::uint32_t> queue;
        for (std::uint32_t index = 0; index < StateCount; ++index)
            if (nodes_[index].wdl == Wdl::Win || nodes_[index].wdl == Wdl::Loss)
                queue.push_back(index);
        while (!queue.empty()) {
            const std::uint32_t child = queue.front();
            queue.pop_front();
            const Node childNode = nodes_[child];
            for (std::uint32_t edge = offsets[child]; edge < offsets[child + 1]; ++edge) {
                Node& parent = nodes_[predecessors[edge]];
                if (parent.wdl != Wdl::Unknown)
                    continue;
                if (childNode.wdl == Wdl::Loss) {
                    parent.wdl = Wdl::Win;
                    parent.dtw = static_cast<std::uint16_t>(std::min<int>(
                      std::numeric_limits<std::uint16_t>::max(), childNode.dtw + 1));
                    queue.push_back(predecessors[edge]);
                }
                else if (childNode.wdl == Wdl::Win) {
                    if (parent.remaining)
                        --parent.remaining;
                    parent.longestWinChild = std::max(parent.longestWinChild, childNode.dtw);
                    if (!parent.remaining) {
                        parent.wdl = Wdl::Loss;
                        parent.dtw = static_cast<std::uint16_t>(std::min<int>(
                          std::numeric_limits<std::uint16_t>::max(),
                          parent.longestWinChild + 1));
                        queue.push_back(predecessors[edge]);
                    }
                }
            }
        }
        for (Node& node : nodes_)
            if (node.wdl == Wdl::Unknown)
                node.wdl = Wdl::Draw;
        verify_solution();
        write_output(offsets.back());
        progress("complete", StateCount, start);
    }

   private:
    Position make_position(const State& state) const {
        Position position;
        position.clear();
        const int whiteKing = position.add_piece(PieceType::King, Color::White,
                                                  state.whiteKing);
        const int blackKing = position.add_piece(PieceType::King, Color::Black,
                                                  state.blackKing);
        const int attacker = position.add_piece(attackerType_, Color::White,
                                                 state.attacker);
        if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
            attacker == Position::NoPiece)
            throw std::runtime_error("failed to construct tablebase state");
        // This first tablebase is the closed no-castling state class. Once a
        // King/Rook has moved the flag cannot become false, so every quiet
        // successor remains probeable in this same class.
        position.piece(whiteKing).moved = true;
        position.piece(blackKing).moved = true;
        position.piece(attacker).moved = true;
        position.set_side_to_move(state.side);
        return position;
    }

    bool in_class(const Position& position) const {
        return position.has_real_king(Color::White) &&
               position.has_real_king(Color::Black) &&
               position.piece(2).alive && position.piece(2).onBoard &&
               position.piece(2).type == attackerType_;
    }

    std::uint32_t child_index(const Position& position) const {
        return encode({position.side_to_move(), position.piece(0).square,
                       position.piece(1).square, position.piece(2).square});
    }

    template<typename EdgeConsumer>
    void analyze_node(std::uint32_t index, bool initialize, EdgeConsumer&& consume) {
        Position position = make_position(decode(index));
        const auto moves = position.legal_moves();
        if (initialize) {
            Node& node = nodes_[index];
            node = {};
            node.remaining = static_cast<std::uint16_t>(moves.size());
            if (moves.empty()) {
                const auto winner = position.winner();
                node.wdl = winner && *winner != position.side_to_move() ? Wdl::Loss : Wdl::Draw;
            }
        }
        for (const Move& move : moves) {
            Position child = position;
            if (!child.apply_move_unchecked(move))
                throw std::runtime_error("legal tablebase move failed trusted application");
            if (!child.has_real_king(~position.side_to_move())) {
                if (initialize) {
                    nodes_[index].wdl = Wdl::Win;
                    nodes_[index].dtw = 1;
                }
                continue;
            }
            if (!in_class(child))
                continue;  // Rook capture enters the exact K-v-K draw class.
            const std::uint32_t successor = child_index(child);
            if (initialize)
                ++predecessorCounts_[successor];
            else
                consume(successor);
        }
    }

    void save_checkpoint(std::uint32_t processed) const {
        const std::string temporary = checkpoint_ + ".tmp";
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("cannot write tablebase checkpoint");
        const std::array<char, 8> magic{{'U','F','T','B','C','P','2','\0'}};
        const std::uint32_t piece = static_cast<std::uint32_t>(attackerType_);
        stream.write(magic.data(), magic.size());
        stream.write(reinterpret_cast<const char*>(&piece), sizeof(piece));
        stream.write(reinterpret_cast<const char*>(&processed), sizeof(processed));
        stream.write(reinterpret_cast<const char*>(nodes_.data()),
                     nodes_.size() * sizeof(Node));
        stream.write(reinterpret_cast<const char*>(predecessorCounts_.data()),
                     predecessorCounts_.size() * sizeof(std::uint32_t));
        stream.close();
        if (std::rename(temporary.c_str(), checkpoint_.c_str()) != 0)
            throw std::runtime_error("cannot install tablebase checkpoint");
    }

    std::uint32_t load_checkpoint() {
        std::ifstream stream(checkpoint_, std::ios::binary);
        if (!stream)
            return 0;
        std::array<char, 8> magic{};
        std::uint32_t piece = 0, processed = 0;
        stream.read(magic.data(), magic.size());
        stream.read(reinterpret_cast<char*>(&piece), sizeof(piece));
        stream.read(reinterpret_cast<char*>(&processed), sizeof(processed));
        const std::array<char, 8> expected{{'U','F','T','B','C','P','2','\0'}};
        if (magic != expected || piece != static_cast<std::uint32_t>(attackerType_) ||
            processed > StateCount)
            throw std::runtime_error("invalid tablebase checkpoint");
        stream.read(reinterpret_cast<char*>(nodes_.data()), nodes_.size() * sizeof(Node));
        stream.read(reinterpret_cast<char*>(predecessorCounts_.data()),
                    predecessorCounts_.size() * sizeof(std::uint32_t));
        if (!stream)
            throw std::runtime_error("truncated tablebase checkpoint");
        std::cout << "resume states " << processed << '\n';
        return processed;
    }

    void write_output(std::uint32_t edges) const {
        std::ofstream stream(output_, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("cannot write tablebase output");
        const std::array<char, 8> magic{{'U','F','T','B','1','\0','\0','\0'}};
        const std::uint32_t version = 2;
        const std::uint32_t piece = static_cast<std::uint32_t>(attackerType_);
        stream.write(magic.data(), magic.size());
        stream.write(reinterpret_cast<const char*>(&version), sizeof(version));
        stream.write(reinterpret_cast<const char*>(&piece), sizeof(piece));
        stream.write(reinterpret_cast<const char*>(&StateCount), sizeof(StateCount));
        stream.write(reinterpret_cast<const char*>(&edges), sizeof(edges));
        for (const Node& node : nodes_) {
            if (node.dtw >= (1u << 14))
                throw std::runtime_error("tablebase DTW exceeds packed file format");
            const std::uint16_t packed = static_cast<std::uint16_t>(
              (static_cast<std::uint16_t>(node.wdl) << 14) | node.dtw);
            stream.write(reinterpret_cast<const char*>(&packed), sizeof(packed));
        }
        std::array<std::uint64_t, 4> totals{};
        for (const Node& node : nodes_)
            ++totals[static_cast<std::size_t>(node.wdl)];
        std::cout << "output " << output_ << " edges " << edges;
        for (Wdl wdl : {Wdl::Win, Wdl::Loss, Wdl::Draw})
            std::cout << ' ' << wdl_name(wdl) << ' '
                      << totals[static_cast<std::size_t>(wdl)];
        std::cout << '\n';
    }

    void verify_solution() const {
        for (std::uint32_t index = 0; index < StateCount; ++index) {
            const Node node = nodes_[index];
            Position position = make_position(decode(index));
            const auto moves = position.legal_moves();
            bool hasLoss = false;
            bool hasDraw = false;
            bool allWin = !moves.empty();
            std::uint16_t shortestLoss = std::numeric_limits<std::uint16_t>::max();
            std::uint16_t longestWin = 0;
            for (const Move& move : moves) {
                Position child = position;
                if (!child.apply_move_unchecked(move))
                    throw std::runtime_error("verification move failed trusted application");
                if (!child.has_real_king(~position.side_to_move())) {
                    hasLoss = true;
                    shortestLoss = 0;
                    allWin = false;
                    continue;
                }
                if (!in_class(child)) {
                    hasDraw = true;
                    allWin = false;
                    continue;
                }
                const Node successor = nodes_[child_index(child)];
                if (successor.wdl == Wdl::Loss) {
                    hasLoss = true;
                    shortestLoss = std::min(shortestLoss, successor.dtw);
                    allWin = false;
                }
                else if (successor.wdl == Wdl::Draw) {
                    hasDraw = true;
                    allWin = false;
                }
                else if (successor.wdl == Wdl::Win)
                    longestWin = std::max(longestWin, successor.dtw);
                else
                    throw std::runtime_error("unknown state remains after retrograde");
            }
            bool valid = false;
            if (node.wdl == Wdl::Win)
                valid = hasLoss && node.dtw == shortestLoss + 1;
            else if (node.wdl == Wdl::Loss)
                valid = (moves.empty() && node.dtw == 0) ||
                        (allWin && node.dtw == longestWin + 1);
            else if (node.wdl == Wdl::Draw)
                valid = !hasLoss && (moves.empty() || hasDraw);
            if (!valid)
                throw std::runtime_error(
                  "retrograde Bellman verification failed at state " +
                  std::to_string(index));
        }
        std::cout << "verifyok states " << StateCount << '\n';
    }

    static void progress(const char* phase, std::uint32_t states,
                         std::chrono::steady_clock::time_point start) {
        const double elapsed = std::chrono::duration<double>(
          std::chrono::steady_clock::now() - start).count();
        std::cout << phase << " states " << states << '/' << StateCount
                  << " elapsed " << elapsed << "s\n" << std::flush;
    }

    PieceType attackerType_;
    std::string output_;
    std::string checkpoint_;
    std::uint32_t checkpointEvery_;
    std::vector<Node> nodes_;
    std::vector<std::uint32_t> predecessorCounts_;
};

}  // namespace Stockfish::Ultimate

int main(int argc, char** argv) {
    using namespace Stockfish::Ultimate;
    PieceType attackerType = PieceType::Rook;
    std::string output = "/tmp/ultimatefish-krk.uftb";
    std::string checkpoint = "/tmp/ultimatefish-krk.checkpoint";
    std::uint32_t checkpointEvery = 50'000;
    bool selfTest = false;
    std::uint32_t dryRun = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto value = [&](const char* option) -> std::string {
            if (index + 1 >= argc)
                throw std::runtime_error(std::string("missing value for ") + option);
            return argv[++index];
        };
        if (argument == "--output") output = value("--output");
        else if (argument == "--checkpoint") checkpoint = value("--checkpoint");
        else if (argument == "--piece") {
            const std::string name = value("--piece");
            const auto parsed = Position::type_from_name(name);
            if (!parsed || !closed_stateless_attacker(*parsed))
                throw std::runtime_error("piece is not a closed stateless tablebase class");
            attackerType = *parsed;
        }
        else if (argument == "--checkpoint-every")
            checkpointEvery = static_cast<std::uint32_t>(std::stoul(value("--checkpoint-every")));
        else if (argument == "--dry-run")
            dryRun = static_cast<std::uint32_t>(std::stoul(value("--dry-run")));
        else if (argument == "--self-test") selfTest = true;
        else throw std::runtime_error("unknown argument: " + argument);
    }
    try {
        TablebaseGenerator generator(attackerType, output, checkpoint, checkpointEvery);
        if (selfTest)
            generator.self_test();
        if (dryRun)
            generator.dry_run(dryRun);
        if (!selfTest && !dryRun)
            generator.generate();
    }
    catch (const std::exception& error) {
        std::cerr << "tablebase error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
