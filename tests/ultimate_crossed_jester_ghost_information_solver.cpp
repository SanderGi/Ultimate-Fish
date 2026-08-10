/* Exact crossed Jester/Ghost solver-core regression. GPLv3+. */

#include "crossed_jester_ghost_information_fixed_point.h"
#include "crossed_jester_ghost_information_lower_oracle.h"
#include "crossed_jester_ghost_information_sidecar.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace Stockfish::Ultimate {
namespace {

namespace Model = CrossedJesterGhostInformation;
namespace Solver = CrossedJesterGhostSolver;

[[nodiscard]] std::uint8_t square(const char* name) {
    const int result = Position::square_from_name(name);
    if (result == Position::NoSquare)
        throw std::runtime_error("invalid crossed solver test square");
    return static_cast<std::uint8_t>(result);
}

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

class CountingLowerOracle final : public Solver::LowerForceOracle {
   public:
    [[nodiscard]] bool force(
      const Solver::LowerJesterForceQuery& query) const override {
        ++jesterQueries;
        return query.targetOwnsJester && query.belief.cardinality == 1;
    }

    [[nodiscard]] bool force(
      const Solver::LowerGhostForceQuery& query) const override {
        ++ghostQueries;
        return query.targetRole == Model::Role::GhostOwner &&
               query.belief.locations.count() == 1;
    }

    mutable std::uint64_t jesterQueries = 0;
    mutable std::uint64_t ghostQueries = 0;
};

void put_u32(std::ostream& output, std::uint32_t value) {
    for (unsigned byte = 0; byte < 4; ++byte)
        output.put(static_cast<char>(value >> (8 * byte)));
}

void put_u64(std::ostream& output, std::uint64_t value) {
    put_u32(output, static_cast<std::uint32_t>(value));
    put_u32(output, static_cast<std::uint32_t>(value >> 32));
}

void put_mask(std::ostream& output, std::uint64_t low,
              std::uint16_t high = 0) {
    put_u64(output, low);
    output.put(static_cast<char>(high));
    output.put(static_cast<char>(high >> 8));
}

void authenticated_lower_oracle_test() {
    const std::string prefix = "/tmp/ultimate-crossed-lower-" +
      std::to_string(static_cast<unsigned long>(::getpid()));
    const std::string table = prefix + ".uftb";
    const std::string overlay = prefix + ".ufiw";
    const std::string ghost = prefix + ".ufgm";
    const std::string jesterModel(64, 'a');
    const std::string ghostSource(64, 'b');
    const std::string ghostModel(64, 'c');
    const std::string observation(64, 'd');
    const std::uint32_t count = Model::LowerJesterStateCount;
    {
        std::ofstream output(table, std::ios::binary);
        output.write("UFTB1\0\0\0", 8);
        for (const std::uint32_t value : {4u,
              static_cast<std::uint32_t>(PieceType::Jester), count, 0u, 1u,
              (count + 3) / 4})
            put_u32(output, value);
        put_u64(output, 0);
        std::vector<char> wins((count + 3) / 4, char(0x55));
        output.write(wins.data(), wins.size());
    }
    const std::string tableSha = Solver::authenticated_file_sha256(table);
    {
        std::ofstream output(overlay, std::ios::binary);
        output.write("UFIW2\0\0\0", 8);
        for (const std::uint32_t value : {2u,
              static_cast<std::uint32_t>(PieceType::Jester),
              static_cast<std::uint32_t>(PieceType::Count),
              static_cast<std::uint32_t>(Color::White), count, 1u})
            put_u32(output, value);
        output.write(tableSha.data(), tableSha.size());
        output.write(jesterModel.data(), jesterModel.size());
        std::vector<char> flags(count, char(7));
        output.write(flags.data(), flags.size());
    }
    {
        std::ofstream output(ghost, std::ios::binary);
        output.write("UFGM1\0\0\0", 8);
        for (const std::uint32_t value : {1u,320u,
              static_cast<std::uint32_t>(PieceType::Ghost),
              static_cast<std::uint32_t>(Color::White),8u,10u,80u,
              Model::LowerGhostStateCount,2u,1u,1u,3u,9u,844u,18u,0u})
            put_u32(output, value);
        put_u64(output, 320);
        put_u64(output, 320 + 3 * 9);
        put_u64(output, 320 + 3 * 9 + 844);
        output.write(ghostSource.data(), ghostSource.size());
        output.write(ghostModel.data(), ghostModel.size());
        output.write(observation.data(), observation.size());
        std::array<char,32> semantics{};
        std::memcpy(semantics.data(), "history-mask-public-view-v2", 27);
        output.write(semantics.data(), semantics.size());
        output.put(char(80)); put_u32(output,0); put_u32(output,0);
        output.put(char(80)); put_u32(output,1); put_u32(output,1);
        output.put(char(2)); put_u32(output,0); put_u32(output,1);
        output.put(char(0));output.put(char(0));output.put(char(1));output.put(char(0));
        put_mask(output, (std::uint64_t(1)<<2)|(std::uint64_t(1)<<3));
        put_mask(output,0);put_mask(output,0);put_mask(output,0);
        for (unsigned square = 0; square < 80; ++square)
            put_u32(output, square == 2 || square == 3 ? 0u :
                                                    std::uint32_t(-1));
        for (unsigned square = 0; square < 80; ++square)
            put_u32(output, square == 2 || square == 3 ? 2u : 0u);
        std::array<char,80> visible{};
        output.write(visible.data(), visible.size());
        output.write(visible.data(), visible.size());
        put_u32(output,0);
        put_mask(output,(std::uint64_t(1)<<2)|(std::uint64_t(1)<<3));
        put_u32(output,2);
    }
    Solver::LowerOracleOptions options;
    options.jesterTable=table;
    options.jesterTableSha256=tableSha;
    options.jesterOverlay=overlay;
    options.jesterOverlaySha256=Solver::authenticated_file_sha256(overlay);
    options.jesterModelSha256=jesterModel;
    options.ghostSidecar=ghost;
    options.ghostSidecarSha256=Solver::authenticated_file_sha256(ghost);
    options.ghostSourceSha256=ghostSource;
    options.ghostModelSha256=ghostModel;
    options.ghostObservationSha256=observation;
    Solver::AuthenticatedLowerForceOracle oracle(options);
    const Model::LowerJesterState first = Model::decode_lower_jester(0);
    Model::LowerJesterState swapped = first;
    std::swap(swapped.whiteKing, swapped.jester);
    const std::uint32_t second = Model::encode_lower_jester(swapped);
    Solver::LowerJesterForceQuery jester;
    jester.target=Color::White;jester.targetOwnsJester=true;jester.actual=0;
    jester.belief=Model::inherited_lower_jester_set({
      {Model::ChildDomain::LowerJester,0,{}},
      {Model::ChildDomain::LowerJester,second,{}}});
    require(oracle.force(jester), "authenticated lower Jester pair failed");
    Solver::LowerGhostForceQuery lowerGhost;
    lowerGhost.target=Color::Black;
    lowerGhost.targetRole=Model::Role::GhostOwner;
    lowerGhost.actual={Model::Role::GhostOwner,0,1,2,false};
    lowerGhost.belief={Model::Role::GhostOwner,0,1,false,{}};
    lowerGhost.belief.locations.set(2);lowerGhost.belief.locations.set(3);
    require(oracle.force(lowerGhost),
            "authenticated lower Ghost arbitrary mask failed");
    bool rejected=false;
    lowerGhost.belief.locations.set(4);
    try{(void)oracle.force(lowerGhost);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"authenticated lower Ghost accepted a stratum-spanning mask");
    const auto& certificate=oracle.certificate();
    require(certificate.jesterStates==Model::LowerJesterStateCount&&
              certificate.ghostGeometries==1&&certificate.ghostStrata==1&&
              certificate.ghostNodes==3,
            "authenticated lower oracle certificate mismatch");
    std::remove(table.c_str());std::remove(overlay.c_str());std::remove(ghost.c_str());
    std::cout << "crossed_lower_oracle jester_states " << certificate.jesterStates
              << " ghost_nodes " << certificate.ghostNodes
              << " arbitrary_mask 1 stratum_reject 1 residual 0\n";
}

void public_frame_codec_test() {
    require(Model::RawPublicFrameCount == 38'450'880,
            "crossed public frame domain changed");
    const std::array<std::uint32_t, 9> witnesses{
      0, 1, 76, 77, 78, 19'225'439, 19'225'440,
      Model::RawPublicFrameCount - 2, Model::RawPublicFrameCount - 1};
    for (const std::uint32_t raw : witnesses) {
        const Model::PublicFrame frame = Model::decode_public_frame(raw);
        require(Model::encode_public_frame(frame) == raw,
                "crossed public frame witness did not round-trip");
    }

    // Exhaust the encoder in its declared dense order. This proves both the
    // exact count and that every valid hidden/visible frame occupies one slot.
    std::uint32_t expected = 0;
    for (const Color side : {Color::White, Color::Black})
        for (std::uint8_t black = 0; black < Position::BoardSquares; ++black) {
            std::vector<std::uint8_t> free;
            for (std::uint8_t candidate = 0;
                 candidate < Position::BoardSquares; ++candidate)
                if (candidate != black)
                    free.push_back(candidate);
            for (std::size_t first = 0; first + 1 < free.size(); ++first)
                for (std::size_t second = first + 1; second < free.size();
                     ++second) {
                    Model::PublicFrame frame{side, black, free[first],
                                             free[second], {}};
                    require(Model::encode_public_frame(frame) == expected++,
                            "crossed hidden public frame order is not dense");
                    for (const std::uint8_t ghost : free)
                        if (ghost != frame.royalFirst &&
                            ghost != frame.royalSecond) {
                            frame.visibleGhost = ghost;
                            require(Model::encode_public_frame(frame) ==
                                      expected++,
                                    "crossed visible public frame order is not dense");
                        }
                }
        }
    require(expected == Model::RawPublicFrameCount,
            "crossed public frame enumeration did not conserve the domain");
    bool rejected = false;
    try {
        (void)Model::decode_public_frame(Model::RawPublicFrameCount);
    }
    catch (const std::out_of_range&) {
        rejected = true;
    }
    require(rejected, "crossed public frame codec accepted an out-of-range ID");
}

Model::KnowledgeState fixture() {
    Model::KnowledgeState state;
    state.frame = {Color::Black, square("h10"), square("a1"),
                   square("c1"), {}};
    state.atoms = {{{true, square("a5")}},
                   {{false, square("a5")}},
                   {{true, square("c5")}},
                   {{false, square("c5")}}};
    for (const Model::HistoryAtom& atom : state.atoms)
        state.worlds.set(Model::world_variable(state.frame, atom.world));
    state.white.cells = {{0, 2}, {1, 3}};
    state.black.cells = {{0, 1}, {2, 3}};
    Model::validate_knowledge_state(state);
    return state;
}

Model::KnowledgeState external_fixture() {
    Model::KnowledgeState state;
    state.frame = {Color::Black, square("e5"), square("a1"),
                   square("e4"), {}};
    const std::array<const char*, 8> ghosts{
      "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"};
    for (const char* name : ghosts) {
        state.atoms.push_back({{true, square(name)}});
        state.atoms.push_back({{false, square(name)}});
    }
    for (const Model::HistoryAtom& atom : state.atoms)
        state.worlds.set(Model::world_variable(state.frame, atom.world));
    Model::KnowledgeCell first;
    Model::KnowledgeCell second;
    for (std::uint32_t atom = 0; atom < state.atoms.size(); ++atom)
        (atom % 2 ? second : first).push_back(atom);
    state.white.cells = {first, second};
    for (std::uint32_t atom = 0; atom < state.atoms.size(); atom += 2)
        state.black.cells.push_back({atom, atom + 1});
    Model::validate_knowledge_state(state);
    return state;
}

Model::KnowledgeState lower_jester_fixture() {
    Model::KnowledgeState state;
    state.frame = {Color::White, square("h10"), square("a1"),
                   square("e4"), square("e5")};
    state.atoms = {{{true, square("e5")}},
                   {{false, square("e5")}}};
    for (const Model::HistoryAtom& atom : state.atoms)
        state.worlds.set(Model::world_variable(state.frame, atom.world));
    state.white.cells = {{0}, {1}};
    state.black.cells = {{0, 1}};
    Model::validate_knowledge_state(state);
    return state;
}

void solver_arena_test() {
    Solver::Arena arena;
    const std::uint32_t seedBegin = Model::encode_public_frame(fixture().frame);
    const Solver::FreshSeedResult seeded = arena.seed_fresh_range(seedBegin, 512);
    require(seeded.roots.size() == 512 &&
             seeded.certificate.rawBegin == seedBegin &&
             seeded.certificate.rawCount == 512 &&
             seeded.certificate.admitted > 0 &&
             seeded.certificate.admitted + seeded.certificate.empty == 512 &&
             seeded.certificate.unique == arena.size() &&
             seeded.certificate.codecResidual == 0,
            "crossed fresh root seed certificate has a residual");
    const Solver::FreshSeedResult replaySeed = arena.seed_fresh_range(
      seedBegin, 512);
    require(replaySeed.roots == seeded.roots &&
             replaySeed.certificate.unique == 0 &&
             replaySeed.certificate.duplicate ==
               replaySeed.certificate.admitted,
            "crossed fresh root seed replay changed stable IDs");
    bool badSeedRange = false;
    try {
        (void)arena.seed_fresh_range(Model::RawPublicFrameCount, 1);
    }
    catch (const std::out_of_range&) {
        badSeedRange = true;
    }
    require(badSeedRange, "crossed fresh root seed accepted a bad range");

    Solver::GraphDiscovery discovery = Solver::GraphDiscovery::seed(
      seedBegin, 64);
    Solver::GraphDiscovery segmented = Solver::GraphDiscovery::seed(
      seedBegin, 32);
    const Solver::FreshSeedCertificate appended = segmented.append_seed(32);
    require(appended.rawBegin == seedBegin + 32 && appended.rawCount == 32 &&
             segmented.roots() == discovery.roots() &&
             segmented.arena().size() == discovery.arena().size(),
            "crossed segmented root seed changed the graph");
    for (Solver::NodeId node = 0; node < segmented.arena().size(); ++node)
        require(segmented.arena().node(node) == discovery.arena().node(node),
                "crossed segmented root seed changed stable node IDs");
    const Solver::DiscoveryCertificate progress = discovery.advance(2);
    require(progress.expanded == 2 && progress.nodesBefore > 0 &&
             progress.nodesAfter >= progress.nodesBefore &&
             progress.actions > 0 && progress.observations > 0 &&
             progress.outcomes > 0 && progress.residual == 0 &&
             discovery.expanded() == 2 && discovery.roots().size() == 64,
            "crossed graph discovery progress has a residual");
    bool lateSeedRejected = false;
    try {
        (void)discovery.append_seed(1);
    }
    catch (const std::logic_error&) {
        lateSeedRejected = true;
    }
    require(lateSeedRejected,
            "crossed graph discovery accepted roots after expansion");
    std::ostringstream checkpoint(std::ios::binary);
    const Solver::DiscoveryArchiveCertificate checkpointWritten =
      discovery.write(checkpoint);
    std::istringstream checkpointInput(checkpoint.str(), std::ios::binary);
    auto [resumed, checkpointRead] = Solver::GraphDiscovery::read(
      checkpointInput);
    require(checkpointWritten.roots == 64 &&
             checkpointWritten.expanded == 2 &&
             checkpointWritten.rootBoundsResidual == 0 &&
             checkpointWritten.cursorResidual == 0 &&
             checkpointWritten.extentResidual == 0 &&
             checkpointRead.roots == checkpointWritten.roots &&
             checkpointRead.emptyRoots == checkpointWritten.emptyRoots &&
             checkpointRead.expanded == checkpointWritten.expanded &&
             checkpointRead.graph.nodes == checkpointWritten.graph.nodes &&
             resumed.raw_begin() == seedBegin && resumed.expanded() == 2 &&
             resumed.roots() == discovery.roots() &&
             resumed.arena().size() == discovery.arena().size(),
            "crossed graph discovery checkpoint did not restore exactly");
    const auto corrupt_u32 = [](std::string& bytes, std::size_t offset,
                                std::uint32_t value) {
        for (unsigned byte = 0; byte < 4; ++byte)
            bytes.at(offset + byte) = static_cast<char>(value >> (8 * byte));
    };
    bool badCheckpointRejected = false;
    try {
        std::string badExtent = checkpoint.str();
        corrupt_u32(badExtent, 16, Model::RawPublicFrameCount);
        std::istringstream invalid(badExtent, std::ios::binary);
        (void)Solver::GraphDiscovery::read(invalid);
    }
    catch (const std::exception&) {
        badCheckpointRejected = true;
    }
    require(badCheckpointRejected,
            "crossed discovery accepted an invalid raw extent");
    badCheckpointRejected = false;
    try {
        std::string badCursor = checkpoint.str();
        for (std::size_t byte = 24; byte < 32; ++byte)
            badCursor.at(byte) = static_cast<char>(0xff);
        std::istringstream invalid(badCursor, std::ios::binary);
        (void)Solver::GraphDiscovery::read(invalid);
    }
    catch (const std::exception&) {
        badCheckpointRejected = true;
    }
    require(badCheckpointRejected,
            "crossed discovery accepted an invalid expansion cursor");
    const auto nonempty = std::find_if(discovery.roots().begin(),
      discovery.roots().end(), [](Solver::NodeId root) {
          return root != std::numeric_limits<Solver::NodeId>::max();
      });
    require(nonempty != discovery.roots().end(),
            "crossed discovery corruption fixture has no root");
    badCheckpointRejected = false;
    try {
        std::string badRoot = checkpoint.str();
        const std::size_t rootIndex = static_cast<std::size_t>(
          nonempty - discovery.roots().begin());
        corrupt_u32(badRoot, 32 + 4 * rootIndex,
                    static_cast<std::uint32_t>(discovery.arena().size()));
        std::istringstream invalid(badRoot, std::ios::binary);
        (void)Solver::GraphDiscovery::read(invalid);
    }
    catch (const std::exception&) {
        badCheckpointRejected = true;
    }
    require(badCheckpointRejected,
            "crossed discovery accepted an out-of-bounds root");
    const Solver::DiscoveryCertificate resumedProgress = resumed.advance(1);
    require(resumedProgress.expanded == 1 && resumed.expanded() == 3 &&
             resumedProgress.residual == 0,
            "crossed graph discovery did not resume at its exact cursor");

    Solver::Arena focused;
    const Model::KnowledgeState root = fixture();
    const Solver::NodeId rootId = focused.intern(root);
    require(rootId == 0 && focused.intern(root) == rootId && focused.size() == 1,
            "crossed solver interner is not collision-free/idempotent");
    const Model::KnowledgeState reflected = Model::transform_state(
      root, Model::RectangleTransform::Both);
    require(focused.intern(reflected) == rootId,
            "crossed solver did not D2-canonicalize a root");

    const Solver::NodeExpansion first = focused.regenerate(rootId, true);
    require(first.certificate.actions == 19 &&
             first.certificate.observations == 4 &&
             first.certificate.outcomes == 38 &&
             first.certificate.sameClassObservations > 0 &&
             first.certificate.newlyInterned > 0 &&
             first.certificate.keyRoundtripResidual == 0,
            "crossed solver first expansion certificate has a residual");
    const std::size_t discovered = focused.size();
    const Solver::NodeExpansion replay = focused.regenerate(rootId, false);
    require(focused.size() == discovered &&
             replay.certificate.actions == first.certificate.actions &&
             replay.certificate.observations ==
               first.certificate.observations &&
             replay.certificate.outcomes == first.certificate.outcomes &&
             replay.certificate.newlyInterned == 0 &&
             replay.sameClassChildren == first.sameClassChildren,
            "crossed solver closed replay differs from discovery");

    std::ostringstream archive(std::ios::binary);
    const Solver::GraphArchiveCertificate written = focused.write(archive);
    const std::string graphBytes = archive.str();
    std::istringstream restoredInput(graphBytes, std::ios::binary);
    auto [restoredArena, restored] = Solver::Arena::read(restoredInput);
    require(written.nodes == discovered && restored.nodes == discovered &&
             written.payloadBytes == restored.payloadBytes &&
             written.keyRoundtripResidual == 0 &&
             written.canonicalResidual == 0 &&
             restored.keyRoundtripResidual == 0 &&
             restored.canonicalResidual == 0 &&
             restored.duplicateResidual == 0 &&
             restoredArena.size() == focused.size(),
            "crossed graph archive round-trip has a residual");
    for (Solver::NodeId node = 0; node < discovered; ++node)
        require(restoredArena.node(node) == focused.node(node),
                "crossed graph archive changed a stable node ID");
    bool rejected = false;
    try {
        std::string trailing = graphBytes;
        trailing.push_back('x');
        std::istringstream invalid(trailing, std::ios::binary);
        (void)Solver::Arena::read(invalid);
    }
    catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, "crossed graph archive accepted trailing bytes");
    rejected = false;
    try {
        std::string corrupt = graphBytes;
        corrupt.front() ^= 1;
        std::istringstream invalid(corrupt, std::ios::binary);
        (void)Solver::Arena::read(invalid);
    }
    catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, "crossed graph archive accepted a bad magic byte");
    const auto little_word = [](const std::string& bytes, std::size_t offset) {
        return static_cast<std::uint32_t>(
          static_cast<std::uint8_t>(bytes.at(offset))) |
          (static_cast<std::uint32_t>(
             static_cast<std::uint8_t>(bytes.at(offset + 1))) << 8) |
          (static_cast<std::uint32_t>(
             static_cast<std::uint8_t>(bytes.at(offset + 2))) << 16) |
          (static_cast<std::uint32_t>(
             static_cast<std::uint8_t>(bytes.at(offset + 3))) << 24);
    };
    const auto put_word = [](std::string& bytes, std::size_t offset,
                             std::uint32_t value) {
        for (unsigned byte = 0; byte < 4; ++byte)
            bytes.at(offset + byte) = static_cast<char>(value >> (8 * byte));
    };
    const std::uint32_t firstKeyBytes = little_word(graphBytes, 20);
    const std::string firstRecord = graphBytes.substr(20, 4 + firstKeyBytes);
    rejected = false;
    try {
        std::string duplicate = graphBytes.substr(0, 20);
        put_word(duplicate, 16, 2);
        duplicate += firstRecord;
        duplicate += firstRecord;
        std::istringstream invalid(duplicate, std::ios::binary);
        (void)Solver::Arena::read(invalid);
    }
    catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, "crossed graph archive accepted a duplicate node key");
    rejected = false;
    try {
        const Model::KnowledgeState canonical = focused.node(rootId);
        std::optional<Model::KnowledgeState> noncanonical;
        for (const Model::RectangleTransform transform : {
               Model::RectangleTransform::Horizontal,
               Model::RectangleTransform::Vertical,
               Model::RectangleTransform::Both}) {
            Model::KnowledgeState candidate = Model::transform_state(
              canonical, transform);
            if (!(candidate == canonical) &&
                Model::canonicalize_state(candidate).value == canonical) {
                noncanonical = std::move(candidate);
                break;
            }
        }
        require(noncanonical.has_value(),
                "crossed archive fixture has no noncanonical D2 representative");
        const std::vector<std::uint8_t> key = Model::serialize_state(
          *noncanonical);
        std::string invalidBytes = graphBytes.substr(0, 20);
        put_word(invalidBytes, 16, 1);
        std::string record(4, '\0');
        put_word(record, 0, static_cast<std::uint32_t>(key.size()));
        record.append(reinterpret_cast<const char*>(key.data()), key.size());
        invalidBytes += record;
        std::istringstream invalid(invalidBytes, std::ios::binary);
        (void)Solver::Arena::read(invalid);
    }
    catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, "crossed graph archive accepted a noncanonical D2 key");
    for (std::size_t bucket = 0;
         bucket < first.transitions.buckets.size(); ++bucket) {
        const auto& transition = first.transitions.buckets[bucket];
        require((transition.domain == Model::ChildDomain::SameClass) ==
                  first.sameClassChildren[bucket].has_value(),
                "crossed solver bucket/child domain mapping is incomplete");
    }
    const Solver::TargetBellmanPlan black = focused.bellman_plan(
      rootId, Color::Black, false);
    require(black.mover == Color::Black &&
             black.certificate.atoms == root.atoms.size() &&
             black.certificate.gates == first.certificate.actions &&
             black.certificate.childReferences ==
               first.certificate.outcomes &&
             black.certificate.internalReferences +
                 black.certificate.externalReferences ==
               black.certificate.childReferences &&
             black.certificate.sourceCoverageResidual == 0 &&
             black.certificate.cellUniformityResidual == 0,
            "crossed informed-mover Bellman plan has a residual");
    for (const Solver::AtomEquationPlan& equation : black.atoms)
        require(equation.kind == Solver::EquationKind::Or &&
                  !equation.gates.empty() && equation.children.empty(),
                "crossed informed atom is not an OR of action gates");
    for (const Solver::ActionGatePlan& gate : black.gates)
        require(!gate.children.empty(),
                "crossed informed action gate lacks cell outcomes");

    const Solver::TargetBellmanPlan white = focused.bellman_plan(
      rootId, Color::White, false);
    require(white.mover == Color::Black && white.gates.empty() &&
             white.certificate.atoms == root.atoms.size() &&
             white.certificate.gates == 0 &&
             white.certificate.childReferences ==
               2 * first.certificate.outcomes &&
             white.certificate.internalReferences +
                 white.certificate.externalReferences ==
               white.certificate.childReferences &&
             white.certificate.sourceCoverageResidual == 0 &&
             white.certificate.cellUniformityResidual == 0,
            "crossed uninformed-target Bellman plan has a residual");
    for (const Solver::AtomEquationPlan& equation : white.atoms)
        require(equation.kind == Solver::EquationKind::And &&
                  equation.gates.empty() && !equation.children.empty(),
                "crossed opponent atom is not an AND of compatible outcomes");
    CountingLowerOracle closureOracle;
    bool incompleteRejected = false;
    try {
        (void)Solver::solve_closed_graph(
          focused, Color::Black, closureOracle, "/tmp");
    }
    catch (const std::exception&) {
        incompleteRejected = true;
    }
    require(incompleteRejected,
            "crossed fixed point accepted an incomplete graph closure");
    bool preflightRejected = false;
    try {
        (void)Solver::preflight_closed_graph(focused, Color::Black);
    }
    catch (const std::exception&) {
        preflightRejected = true;
    }
    require(preflightRejected,
            "crossed preflight accepted an incomplete graph closure");
    std::cout << "crossed_solver_arena nodes " << focused.size()
              << " actions " << first.certificate.actions
              << " observations " << first.certificate.observations
              << " outcomes " << first.certificate.outcomes
              << " black_gates " << black.certificate.gates
              << " white_children " << white.certificate.childReferences
              << " replay_new_nodes 0 bellman_residual 0\n";

    Solver::Arena externalArena;
    const Solver::NodeId externalId = externalArena.intern(
      external_fixture());
    const Solver::NodeExpansion externalExpansion = externalArena.regenerate(
      externalId, true);
    const Solver::TargetBellmanPlan external = externalArena.bellman_plan(
      externalId, Color::Black, false);
    require(external.certificate.externalReferences > 0 &&
             external.certificate.internalReferences > 0,
            "crossed external fixture did not retain mixed child domains");
    std::uint64_t lowerGhost = 0;
    std::uint64_t exactTerminal = 0;
    std::uint64_t blackTerminalForces = 0;
    std::size_t largestBlackLowerBelief = 0;
    CountingLowerOracle oracle;
    for (const Solver::ActionGatePlan& gate : external.gates)
        for (const Solver::ChildReference& child : gate.children) {
            if (child.domain == Model::ChildDomain::SameClass)
                require(child.node.has_value(),
                        "crossed internal reference lost its node");
            else {
                require(!child.node.has_value() &&
                          child.domain != Model::ChildDomain::Invalid,
                        "crossed external reference was interned or invalid");
                lowerGhost += child.domain == Model::ChildDomain::LowerGhost;
                exactTerminal +=
                  child.domain == Model::ChildDomain::ExactTerminal;
                if (child.domain == Model::ChildDomain::ExactTerminal)
                    require(child.child.winner.has_value(),
                            "crossed terminal reference lost its winner");
                const Solver::ExternalForceQuery query =
                  Solver::external_force_query(
                    externalExpansion, Color::Black, child);
                require(!query.belief.empty() &&
                          query.domain == child.domain &&
                          query.actual.domain == child.domain,
                        "crossed external force query lost its inherited cell");
                if (child.domain == Model::ChildDomain::ExactTerminal)
                    blackTerminalForces +=
                      Solver::resolve_external_force(query, oracle);
                else if (child.domain == Model::ChildDomain::LowerGhost) {
                    const Solver::LowerGhostForceQuery lower =
                      Solver::lower_ghost_force_query(query);
                    require(lower.targetRole == Model::Role::GhostOwner,
                            "crossed Black lower query lost Ghost ownership");
                    largestBlackLowerBelief = std::max(
                      largestBlackLowerBelief,
                      static_cast<std::size_t>(lower.belief.locations.count()));
                    require(Solver::resolve_external_force(query, oracle),
                            "crossed lower-Ghost edge bypassed its owner oracle");
                }
            }
        }
    require(lowerGhost > 0 && exactTerminal > 0,
            "crossed Bellman plan lost lower-Ghost or terminal references");
    require(blackTerminalForces == exactTerminal,
            "crossed Black terminal force did not reproduce the public winner");
    require(largestBlackLowerBelief == 1,
            "crossed Ghost owner did not retain its singleton lower location");
    const Solver::TargetBellmanPlan externalWhite =
      externalArena.bellman_plan(externalId, Color::White, false);
    std::size_t largestWhiteLowerBelief = 0;
    for (const Solver::AtomEquationPlan& equation : externalWhite.atoms)
        for (const Solver::ChildReference& child : equation.children)
            if (child.domain == Model::ChildDomain::LowerGhost) {
                const Solver::ExternalForceQuery query =
                  Solver::external_force_query(
                    externalExpansion, Color::White, child);
                largestWhiteLowerBelief = std::max(
                  largestWhiteLowerBelief, query.belief.size());
                const Solver::LowerGhostForceQuery lower =
                  Solver::lower_ghost_force_query(query);
                require(lower.targetRole == Model::Role::Observer &&
                          lower.belief.locations.count() == 8,
                        "crossed White lower query lost the arbitrary Ghost mask");
                require(!Solver::resolve_external_force(query, oracle),
                        "crossed lower-Ghost observer query used an owner result");
            }
            else if (child.domain == Model::ChildDomain::ExactTerminal) {
                const Solver::ExternalForceQuery query =
                  Solver::external_force_query(
                    externalExpansion, Color::White, child);
                require(!Solver::resolve_external_force(query, oracle),
                        "crossed White force accepted a Black terminal win");
            }
    require(largestWhiteLowerBelief == 8,
            "crossed lower-Ghost query freshened or singletonized the White belief");
    std::cout << "crossed_solver_external lower_ghost " << lowerGhost
              << " exact_terminal " << exactTerminal
              << " inherited_white_belief " << largestWhiteLowerBelief
              << " unresolved_as_draw 0 residual 0\n";

    Solver::Arena jesterArena;
    const Solver::NodeId jesterId = jesterArena.intern(
      lower_jester_fixture());
    const Solver::NodeExpansion jesterExpansion = jesterArena.regenerate(
      jesterId, true);
    const Solver::TargetBellmanPlan whiteJester = jesterArena.bellman_plan(
      jesterId, Color::White, false);
    std::uint64_t whiteSingletons = 0;
    for (const Solver::ActionGatePlan& gate : whiteJester.gates)
        for (const Solver::ChildReference& child : gate.children)
            if (child.domain == Model::ChildDomain::LowerJester) {
                const Solver::LowerJesterForceQuery lower =
                  Solver::lower_jester_force_query(
                    Solver::external_force_query(
                      jesterExpansion, Color::White, child));
                require(lower.targetOwnsJester &&
                          lower.belief.cardinality == 1,
                        "crossed Jester owner did not retain its singleton assignment");
                require(Solver::resolve_external_force(
                          Solver::external_force_query(
                            jesterExpansion, Color::White, child), oracle),
                        "crossed lower-Jester edge bypassed its owner oracle");
                ++whiteSingletons;
            }
    const Solver::TargetBellmanPlan blackJester = jesterArena.bellman_plan(
      jesterId, Color::Black, false);
    std::uint64_t blackPairs = 0;
    for (const Solver::AtomEquationPlan& equation : blackJester.atoms)
        for (const Solver::ChildReference& child : equation.children)
            if (child.domain == Model::ChildDomain::LowerJester) {
                const Solver::LowerJesterForceQuery lower =
                  Solver::lower_jester_force_query(
                    Solver::external_force_query(
                      jesterExpansion, Color::Black, child));
                require(!lower.targetOwnsJester &&
                          lower.belief.cardinality == 2,
                        "crossed Jester observer lost the inherited royal pair");
                require(!Solver::resolve_external_force(
                          Solver::external_force_query(
                            jesterExpansion, Color::Black, child), oracle),
                        "crossed lower-Jester observer used an owner result");
                ++blackPairs;
            }
    require(whiteSingletons > 0 && blackPairs > 0,
            "crossed Bellman plan did not expose lower-Jester queries");
    require(oracle.jesterQueries == whiteSingletons + blackPairs &&
             oracle.ghostQueries > 0,
            "crossed lower oracle dispatch did not cover every tested edge");
    std::cout << "crossed_solver_lower_jester owner_singletons "
              << whiteSingletons << " observer_pairs " << blackPairs
              << " fresh_remaximized 0 residual 0\n";
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main() {
    try {
        Stockfish::Ultimate::public_frame_codec_test();
        Stockfish::Ultimate::authenticated_lower_oracle_test();
        Stockfish::Ultimate::CrossedJesterGhostSolver::arbitrary_sidecar_format_self_test(
          "/tmp/ultimate-crossed-sidecar-format-test.ufcs");
        std::cout << "crossed_sidecar full_roots 38450880 canonical_lookup 1 "
                     "dual_force 0 corruption_rejected 1 residual 0\n";
        Stockfish::Ultimate::solver_arena_test();
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Crossed Jester/Ghost solver test failed: "
                  << error.what() << '\n';
        return 1;
    }
}
