/* Ultimate Fish authenticated crossed Jester/Ghost lower oracles. GPLv3+. */

#include "crossed_jester_ghost_information_lower_oracle.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace Stockfish::Ultimate::CrossedJesterGhostSolver {
namespace {

namespace Model = CrossedJesterGhostInformation;
constexpr std::uint32_t Squares = Position::BoardSquares;
constexpr std::uint32_t NoIndex = std::numeric_limits<std::uint32_t>::max();

std::uint32_t u32(const std::uint8_t* p) {
    return p[0] | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}

void require_hash(const std::string& value, const char* label) {
    if (value.size() != 64 ||
        !std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }))
        throw std::invalid_argument(std::string(label) + " is not SHA-256");
}

class Sha256 {
   public:
    void update(const void* bytes, std::size_t count) {
        const auto* source = static_cast<const std::uint8_t*>(bytes);
        total_ += count;
        while (count) {
            const std::size_t take = std::min(count, block_.size() - used_);
            std::memcpy(block_.data() + used_, source, take);
            source += take;
            count -= take;
            used_ += take;
            if (used_ == block_.size()) {
                transform(block_.data());
                used_ = 0;
            }
        }
    }
    std::array<std::uint8_t, 32> finish() {
        const std::uint64_t bits = total_ * 8;
        block_[used_++] = 0x80;
        if (used_ > 56) {
            std::fill(block_.begin() + used_, block_.end(), 0);
            transform(block_.data());
            used_ = 0;
        }
        std::fill(block_.begin() + used_, block_.begin() + 56, 0);
        for (unsigned i = 0; i < 8; ++i)
            block_[63 - i] = static_cast<std::uint8_t>(bits >> (8 * i));
        transform(block_.data());
        std::array<std::uint8_t, 32> result{};
        for (unsigned w = 0; w < state_.size(); ++w)
            for (unsigned b = 0; b < 4; ++b)
                result[w * 4 + b] = static_cast<std::uint8_t>(
                  state_[w] >> (24 - 8 * b));
        return result;
    }

   private:
    static std::uint32_t rotate(std::uint32_t value, unsigned count) {
        return (value >> count) | (value << (32 - count));
    }
    void transform(const std::uint8_t* block) {
        static constexpr std::array<std::uint32_t, 64> constants{{
          0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
          0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
          0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
          0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
          0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
          0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
          0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
          0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2}};
        std::array<std::uint32_t, 64> words{};
        for (unsigned i = 0; i < 16; ++i)
            words[i] = (std::uint32_t(block[i * 4]) << 24) |
              (std::uint32_t(block[i * 4 + 1]) << 16) |
              (std::uint32_t(block[i * 4 + 2]) << 8) | block[i * 4 + 3];
        for (unsigned i = 16; i < 64; ++i) {
            const std::uint32_t a = rotate(words[i - 15], 7) ^
              rotate(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const std::uint32_t b = rotate(words[i - 2], 17) ^
              rotate(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + a + words[i - 7] + b;
        }
        auto [a,b,c,d,e,f,g,h] = state_;
        for (unsigned i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t first = h + s1 + ch + constants[i] + words[i];
            const std::uint32_t s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t second = s0 + maj;
            h=g;g=f;f=e;e=d+first;d=c;c=b;b=a;a=first+second;
        }
        state_[0]+=a;state_[1]+=b;state_[2]+=c;state_[3]+=d;
        state_[4]+=e;state_[5]+=f;state_[6]+=g;state_[7]+=h;
    }
    std::array<std::uint32_t,8> state_{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    std::array<std::uint8_t,64> block_{};
    std::size_t used_ = 0;
    std::uint64_t total_ = 0;
};

std::string sha256_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot hash " + path);
    Sha256 hash;
    std::array<char, 1 << 20> buffer{};
    while (input) {
        input.read(buffer.data(), buffer.size());
        if (input.gcount() > 0)
            hash.update(buffer.data(), static_cast<std::size_t>(input.gcount()));
    }
    const auto digest = hash.finish();
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const std::uint8_t byte : digest) result << std::setw(2) << unsigned(byte);
    return result.str();
}

std::string sha256_range(const std::string& path, std::uint64_t offset,
                         std::uint64_t count) {
    std::ifstream input(path, std::ios::binary);
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) throw std::runtime_error("cannot seek while hashing " + path);
    Sha256 hash;
    std::array<char, 1 << 20> buffer{};
    while (count) {
        const std::size_t take = static_cast<std::size_t>(
          std::min<std::uint64_t>(count, buffer.size()));
        input.read(buffer.data(), static_cast<std::streamsize>(take));
        if (input.gcount() != static_cast<std::streamsize>(take))
            throw std::runtime_error("truncated hash range " + path);
        hash.update(buffer.data(), take);
        count -= take;
    }
    const auto digest = hash.finish();
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const std::uint8_t byte : digest) result << std::setw(2) << unsigned(byte);
    return result.str();
}

enum class Wdl : std::uint8_t { Invalid = 0, Win = 1, Loss = 2, Draw = 3 };

struct PackedTable {
    std::vector<std::uint8_t> bytes;
    std::size_t plane = 0;
    Wdl result(std::uint32_t index) const {
        return static_cast<Wdl>((bytes.at(plane + index / 4) >>
          (2 * (index % 4))) & 3);
    }
};

PackedTable load_jester_table(const LowerOracleOptions& options) {
    require_hash(options.jesterTableSha256, "lower Jester table SHA-256");
    if (sha256_file(options.jesterTable) != options.jesterTableSha256)
        throw std::runtime_error("lower Jester concrete full SHA mismatch");
    std::ifstream input(options.jesterTable, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 56)
        throw std::runtime_error("lower Jester concrete table is truncated");
    PackedTable table;
    table.bytes.resize(static_cast<std::size_t>(input.tellg()));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(table.bytes.data()), table.bytes.size());
    if (!input || std::memcmp(table.bytes.data(), "UFTB1\0\0\0", 8))
        throw std::runtime_error("lower Jester concrete magic mismatch");
    const std::uint32_t version = u32(table.bytes.data() + 8);
    const std::uint32_t count = u32(table.bytes.data() + 16);
    const std::uint32_t bytes = u32(table.bytes.data() + 28);
    if (version < 4 || version > 7 ||
        u32(table.bytes.data() + 12) != static_cast<std::uint32_t>(PieceType::Jester) ||
        count != Model::LowerJesterStateCount || u32(table.bytes.data() + 24) != 1 ||
        bytes != (count + 3) / 4 ||
        (version >= 5 &&
          (u32(table.bytes.data() + 40) != static_cast<std::uint32_t>(PieceType::Count) ||
           u32(table.bytes.data() + 44) != static_cast<std::uint32_t>(Color::White))))
        throw std::runtime_error("lower Jester concrete material/codec mismatch");
    table.plane = 40 + (version >= 5 ? 8 : 0) + (version >= 6 ? 8 : 0) +
                  (version >= 7 ? 8 : 0);
    if (table.plane + bytes > table.bytes.size())
        throw std::runtime_error("lower Jester concrete WDL plane is truncated");
    return table;
}

bool wdl_forces(Wdl result, Color side, Color target) {
    return (result == Wdl::Win && side == target) ||
           (result == Wdl::Loss && side != target);
}

struct Mask {
    std::uint64_t low = 0;
    std::uint16_t high = 0;
    bool test(unsigned square) const {
        return square < 64 ? (low >> square) & 1u :
               square < Squares ? (high >> (square - 64)) & 1u : false;
    }
    bool contains(const Model::GhostMask& other) const {
        return (other.low & ~low) == 0 && (other.high & ~high) == 0;
    }
};

struct Node { std::uint8_t variable=Squares; std::uint32_t low=0,high=0; };
struct Geometry {
    std::uint8_t side=0,ownerKing=0,observerKing=0,visible=0;
    Mask live,terminal,terminalOwner,terminalObserver;
    std::array<std::uint32_t,Squares> actualStratum{};
    std::array<std::uint32_t,Squares> ownerRoot{};
    std::array<std::uint8_t,Squares> visibleOwner{},visibleObserver{};
};
struct Stratum { std::uint32_t geometry=NoIndex; Mask live; std::uint32_t root=0; };

}  // namespace

std::string authenticated_file_sha256(const std::string& path) {
    return sha256_file(path);
}

std::string authenticated_file_range_sha256(
  const std::string& path, std::uint64_t offset, std::uint64_t count) {
    return sha256_range(path, offset, count);
}

class AuthenticatedLowerForceOracle::Impl {
   public:
    explicit Impl(const LowerOracleOptions& options)
      : jester_(load_jester_table(options)) {
        load_jester_overlay(options);
        load_ghost(options);
        certificate_.jesterTableSha256 = options.jesterTableSha256;
        certificate_.jesterOverlaySha256 = options.jesterOverlaySha256;
        certificate_.ghostSidecarSha256 = options.ghostSidecarSha256;
        certificate_.jesterStates = Model::LowerJesterStateCount;
        certificate_.ghostGeometries = geometries_.size();
        certificate_.ghostStrata = strata_.size();
        certificate_.ghostNodes = nodes_.size();
    }

    bool force(const LowerJesterForceQuery& query) const {
        if (query.targetOwnsJester != (query.target == Color::White) ||
            !query.belief.cardinality || query.belief.cardinality > 2)
            throw std::invalid_argument("invalid authenticated lower Jester query");
        bool actual = false;
        for (std::uint8_t i = 0; i < query.belief.cardinality; ++i) {
            if (query.belief.concrete[i] >= Model::LowerJesterStateCount)
                throw std::out_of_range("lower Jester belief index");
            actual |= query.belief.concrete[i] == query.actual;
        }
        if (!actual) throw std::runtime_error("lower Jester actual is outside belief");
        if (query.belief.cardinality == 1)
            return wdl_forces(jester_.result(query.actual),
              Model::decode_lower_jester(query.actual).side, query.target);
        const std::uint8_t flags = jesterFlags_.at(query.actual);
        const std::uint8_t other = jesterFlags_.at(
          query.belief.concrete[0] == query.actual ? query.belief.concrete[1] :
                                                    query.belief.concrete[0]);
        if (!(flags & 4) || !(other & 4) || (flags & 2) != (other & 2))
            throw std::runtime_error("lower Jester pair overlay residual");
        return (flags & (query.target == Color::White ? 1 : 2)) != 0;
    }

    bool force(const LowerGhostForceQuery& query) const {
        const auto& actual = query.actual;
        const auto& belief = query.belief;
        if (!belief.locations.count() || !belief.locations.test(actual.ghost) ||
            actual.side != belief.side || actual.ownerKing != belief.ownerKing ||
            actual.observerKing != belief.observerKing || actual.visible != belief.visible)
            throw std::invalid_argument("invalid authenticated lower Ghost query");
        const std::uint32_t gid = geometry_id(actual);
        const Geometry& geometry = geometries_.at(gid);
        const Mask assignment{belief.locations.low, belief.locations.high};
        if (belief.visible) {
            if (belief.locations.count() != 1)
                throw std::runtime_error("visible lower Ghost belief is not singleton");
            if (geometry.live.test(actual.ghost))
                return query.targetRole == Model::Role::GhostOwner
                     ? geometry.visibleOwner[actual.ghost] != 0
                     : geometry.visibleObserver[actual.ghost] != 0;
        }
        const bool actualLive = geometry.live.test(actual.ghost);
        const bool actualTerminal = geometry.terminal.test(actual.ghost);
        if (actualLive == actualTerminal)
            throw std::runtime_error("lower Ghost actual is neither uniquely live nor terminal");
        if (actualTerminal) {
            if (!geometry.terminal.contains(belief.locations) ||
                geometry.live.contains(belief.locations))
                throw std::runtime_error("lower Ghost terminal belief mixes live outcomes");
            const Mask& forces = query.targetRole == Model::Role::GhostOwner
                               ? geometry.terminalOwner : geometry.terminalObserver;
            const bool value = forces.test(actual.ghost);
            for (unsigned square = 0; square < Squares; ++square)
                if (belief.locations.test(square) && forces.test(square) != value)
                    throw std::runtime_error("lower Ghost terminal belief mixes public outcomes");
            return value;
        }
        if (!geometry.live.contains(belief.locations))
            throw std::runtime_error("lower Ghost live belief contains a nonlive world");
        const std::uint32_t sid = geometry.actualStratum[actual.ghost];
        if (sid == NoIndex || sid >= strata_.size() ||
            strata_[sid].geometry != gid || !strata_[sid].live.contains(belief.locations))
            throw std::runtime_error("lower Ghost belief spans private decision strata");
        const std::uint32_t root = query.targetRole == Model::Role::GhostOwner
                                 ? geometry.ownerRoot[actual.ghost]
                                 : strata_[sid].root;
        return evaluate(root, assignment);
    }

    const LowerOracleCertificate& certificate() const { return certificate_; }

   private:
    void load_jester_overlay(const LowerOracleOptions& options) {
        require_hash(options.jesterOverlaySha256, "lower Jester overlay SHA-256");
        require_hash(options.jesterModelSha256, "lower Jester model SHA-256");
        if (sha256_file(options.jesterOverlay) != options.jesterOverlaySha256)
            throw std::runtime_error("lower Jester overlay full SHA mismatch");
        std::ifstream input(options.jesterOverlay, std::ios::binary);
        std::array<std::uint8_t,160> header{};
        input.read(reinterpret_cast<char*>(header.data()), header.size());
        if (!input || std::memcmp(header.data(), "UFIW2\0\0\0", 8) ||
            u32(header.data()+8)!=2 ||
            u32(header.data()+12)!=static_cast<std::uint32_t>(PieceType::Jester) ||
            u32(header.data()+16)!=static_cast<std::uint32_t>(PieceType::Count) ||
            u32(header.data()+20)!=static_cast<std::uint32_t>(Color::White) ||
            u32(header.data()+24)!=Model::LowerJesterStateCount ||
            u32(header.data()+28)!=1 ||
            std::string(reinterpret_cast<char*>(header.data()+32),64)!=options.jesterTableSha256 ||
            std::string(reinterpret_cast<char*>(header.data()+96),64)!=options.jesterModelSha256)
            throw std::runtime_error("lower Jester UFIW2 binding mismatch");
        jesterFlags_.resize(Model::LowerJesterStateCount);
        input.read(reinterpret_cast<char*>(jesterFlags_.data()), jesterFlags_.size());
        if (!input || input.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("lower Jester UFIW2 extent mismatch");
        for (const std::uint8_t flags : jesterFlags_)
            if ((flags & ~std::uint8_t{7}) || (!(flags & 4) && (flags & 3)))
                throw std::runtime_error("lower Jester UFIW2 flag residual");
    }

    void load_ghost(const LowerOracleOptions& options) {
        require_hash(options.ghostSidecarSha256, "lower Ghost sidecar SHA-256");
        require_hash(options.ghostSourceSha256, "lower Ghost source SHA-256");
        require_hash(options.ghostModelSha256, "lower Ghost model SHA-256");
        require_hash(options.ghostObservationSha256, "lower Ghost observation SHA-256");
        if (sha256_file(options.ghostSidecar) != options.ghostSidecarSha256)
            throw std::runtime_error("lower Ghost sidecar full SHA mismatch");
        std::ifstream input(options.ghostSidecar, std::ios::binary);
        auto byte=[&](){const int c=input.get();if(c<0)throw std::runtime_error("truncated UFGM");return std::uint8_t(c);};
        auto word=[&](){std::array<std::uint8_t,4>b{};input.read(reinterpret_cast<char*>(b.data()),4);if(!input)throw std::runtime_error("truncated UFGM");return u32(b.data());};
        auto wide=[&](){const std::uint64_t lo=word();return lo|(std::uint64_t(word())<<32);};
        auto text=[&](std::size_t n){std::string s(n,'\0');input.read(s.data(),n);if(!input)throw std::runtime_error("truncated UFGM");return s;};
        auto mask=[&](){Mask m;m.low=wide();m.high=std::uint16_t(byte())|(std::uint16_t(byte())<<8);return m;};
        const std::string magic=text(8);const auto version=word(),header=word();
        const auto piece=word(),owner=word(),files=word(),ranks=word(),squares=word();
        const auto concrete=word(),substates=word(),geometryCount=word();
        const auto stratumCount=word(),nodeCount=word(),nodeBytes=word();
        const auto geometryBytes=word(),stratumBytes=word();(void)word();
        const auto nodeOffset=wide(),geometryOffset=wide(),stratumOffset=wide();
        const std::string source=text(64),model=text(64),observation=text(64),semantics=text(32);
        if(magic!=std::string("UFGM1\0\0\0",8)||version!=1||header!=320||
           piece!=static_cast<std::uint32_t>(PieceType::Ghost)||
           owner!=static_cast<std::uint32_t>(Color::White)||files!=Position::BoardFiles||
           ranks!=Position::BoardRanks||squares!=Squares||concrete!=Model::LowerGhostStateCount||
           substates!=2||nodeCount<2||nodeBytes!=9||geometryBytes!=844||stratumBytes!=18||
           nodeOffset!=320||geometryOffset!=nodeOffset+std::uint64_t(nodeCount)*9||
           stratumOffset!=geometryOffset+std::uint64_t(geometryCount)*844||
           source!=options.ghostSourceSha256||model!=options.ghostModelSha256||
           observation!=options.ghostObservationSha256||
           semantics.c_str()!=std::string("history-mask-public-view-v2"))
            throw std::runtime_error("lower Ghost UFGM binding mismatch");
        input.seekg(nodeOffset);nodes_.resize(nodeCount);
        std::set<std::tuple<std::uint8_t,std::uint32_t,std::uint32_t>> unique;
        for(std::uint32_t id=0;id<nodeCount;++id){Node&n=nodes_[id];n.variable=byte();n.low=word();n.high=word();
            if(id<2){if(n.variable!=Squares||n.low!=id||n.high!=id)throw std::runtime_error("bad UFGM terminal");}
            else{if(n.variable>=Squares||n.low>=id||n.high>=id||n.low==n.high||!unique.emplace(n.variable,n.low,n.high).second)throw std::runtime_error("bad UFGM tuple");
                const auto child=[&](std::uint32_t c){return c<2?Squares:nodes_[c].variable;};
                if(child(n.low)<=n.variable||child(n.high)<=n.variable)throw std::runtime_error("unordered UFGM tuple");}}
        input.seekg(geometryOffset);geometries_.resize(geometryCount);
        for(std::uint32_t id=0;id<geometryCount;++id){Geometry&g=geometries_[id];g.side=byte();g.ownerKing=byte();g.observerKing=byte();g.visible=byte();
            g.live=mask();g.terminal=mask();g.terminalOwner=mask();g.terminalObserver=mask();
            for(auto&v:g.actualStratum)v=word();for(auto&v:g.ownerRoot)v=word();
            input.read(reinterpret_cast<char*>(g.visibleOwner.data()),Squares);input.read(reinterpret_cast<char*>(g.visibleObserver.data()),Squares);
            if(!input||g.side>1||g.ownerKing>=Squares||g.observerKing>=Squares||g.ownerKing==g.observerKing||g.visible>1)throw std::runtime_error("bad UFGM geometry");
            for(unsigned square=0;square<Squares;++square)if(g.ownerRoot[square]>=nodeCount||g.visibleOwner[square]>1||g.visibleObserver[square]>1||(g.live.test(square)&&g.terminal.test(square)))throw std::runtime_error("bad UFGM geometry roots");
            const std::uint32_t code=geometry_code(g.side,g.ownerKing,g.observerKing,g.visible);if(!geometryIndex_.emplace(code,id).second)throw std::runtime_error("duplicate UFGM geometry");}
        input.seekg(stratumOffset);strata_.resize(stratumCount);
        for(auto&s:strata_){s.geometry=word();s.live=mask();s.root=word();if(s.geometry>=geometryCount||s.root>=nodeCount)throw std::runtime_error("bad UFGM stratum");}
        if(input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("UFGM trailing bytes");
    }

    static std::uint32_t geometry_code(std::uint8_t side,std::uint8_t owner,
      std::uint8_t observer,bool visible){return side|(std::uint32_t(owner)<<1)|
      (std::uint32_t(observer)<<8)|(std::uint32_t(visible)<<15);}
    std::uint32_t geometry_id(const Model::LowerGhostState& state) const {
        const auto found=geometryIndex_.find(geometry_code(
          static_cast<std::uint8_t>(state.side),state.ownerKing,state.observerKing,state.visible));
        if(found==geometryIndex_.end())throw std::runtime_error("missing lower Ghost geometry");
        return found->second;
    }
    bool evaluate(std::uint32_t root,const Mask& assignment) const {
        if(root>=nodes_.size())throw std::runtime_error("lower Ghost root out of range");
        while(root>=2){const Node&node=nodes_[root];root=assignment.test(node.variable)?node.high:node.low;}
        return root!=0;
    }

    PackedTable jester_;
    std::vector<std::uint8_t> jesterFlags_;
    std::vector<Node> nodes_;
    std::vector<Geometry> geometries_;
    std::vector<Stratum> strata_;
    std::map<std::uint32_t,std::uint32_t> geometryIndex_;
    LowerOracleCertificate certificate_;
};

AuthenticatedLowerForceOracle::AuthenticatedLowerForceOracle(
  const LowerOracleOptions& options) : impl_(std::make_unique<Impl>(options)) {}
AuthenticatedLowerForceOracle::~AuthenticatedLowerForceOracle() = default;
AuthenticatedLowerForceOracle::AuthenticatedLowerForceOracle(
  AuthenticatedLowerForceOracle&&) noexcept = default;
AuthenticatedLowerForceOracle& AuthenticatedLowerForceOracle::operator=(
  AuthenticatedLowerForceOracle&&) noexcept = default;
bool AuthenticatedLowerForceOracle::force(
  const LowerJesterForceQuery& query) const { return impl_->force(query); }
bool AuthenticatedLowerForceOracle::force(
  const LowerGhostForceQuery& query) const { return impl_->force(query); }
const LowerOracleCertificate& AuthenticatedLowerForceOracle::certificate() const {
    return impl_->certificate();
}

}  // namespace Stockfish::Ultimate::CrossedJesterGhostSolver
