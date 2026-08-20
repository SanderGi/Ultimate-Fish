/*
  Ultimate Fish - exact K+Ghost+Ghost versus K information solver
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.

  This proof kernel deliberately remains outside the normal build while its
  full transition graph is resource-gated.  Beliefs are subsets of unordered
  Ghost-square pairs; no square marginal, cap, sample, or search horizon is
  used anywhere in the production equations.
*/

#include "ghost_pair_information_solver.h"

#include "ghost_information_probe.h"
#include "information.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace Stockfish::Ultimate::GhostPairInformation {
namespace {

[[nodiscard]] std::uint32_t encode_geometry(const PublicFrame& frame);
[[nodiscard]] PublicFrame decode_geometry(std::uint32_t index);
[[nodiscard]] std::pair<std::uint32_t, RectangleTransform>
canonical_geometry(const PublicFrame& frame);
[[nodiscard]] std::uint8_t terminal_flags(const Position& position);
[[nodiscard]] std::optional<Move> find_action(
  const Position& position, const ActionKey& action);

constexpr std::uint8_t Squares = Position::BoardSquares;
constexpr std::uint32_t NoIndex = std::numeric_limits<std::uint32_t>::max();
constexpr char TransitionMagic[8] = {'U','F','G','G','T','1','\0','\0'};
constexpr char VerifiedMagic[8] = {'U','F','G','G','V','1','\0','\0'};
constexpr char OverlayMagic[8] = {'U','F','I','W','2','\0','\0','\0'};
constexpr std::uint32_t TransitionVersion = 1;

[[noreturn]] void system_error(const std::string& operation,
                               const std::string& path) {
    throw std::runtime_error(operation + " " + path + ": " +
                             std::strerror(errno));
}

[[nodiscard]] bool valid_sha256(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(),
      [](unsigned char character) {
          return (character >= '0' && character <= '9') ||
                 (character >= 'a' && character <= 'f');
      });
}

void require_hash(const std::string& value, const char* label) {
    if (!valid_sha256(value))
        throw std::invalid_argument(std::string(label) +
                                    " is not lowercase SHA-256");
}

template<typename Value>
void write_value(std::ostream& output, const Value& value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

template<typename Value>
[[nodiscard]] Value read_value(std::istream& input) {
    Value result{};
    input.read(reinterpret_cast<char*>(&result), sizeof(result));
    if (!input)
        throw std::runtime_error("truncated exact Ghost-pair certificate");
    return result;
}

[[nodiscard]] std::uint64_t file_bytes(const std::string& path) {
    struct stat status{};
    if (::stat(path.c_str(), &status)) system_error("cannot stat", path);
    return static_cast<std::uint64_t>(status.st_size);
}

class Sha256 {
  public:
    void update(const void* bytes, std::size_t count) {
        const auto* source = static_cast<const std::uint8_t*>(bytes);
        total_ += count;
        while (count) {
            const std::size_t take = std::min(count, block_.size() - used_);
            std::memcpy(block_.data() + used_, source, take);
            source += take; count -= take; used_ += take;
            if (used_ == block_.size()) { transform(block_.data()); used_ = 0; }
        }
    }
    [[nodiscard]] std::array<std::uint8_t,32> finish() {
        const std::uint64_t bits = total_ * 8;
        block_[used_++] = 0x80;
        if (used_ > 56) {
            std::fill(block_.begin()+used_, block_.end(), 0);
            transform(block_.data()); used_ = 0;
        }
        std::fill(block_.begin()+used_, block_.begin()+56, 0);
        for (unsigned byte=0; byte<8; ++byte)
            block_[63-byte] = static_cast<std::uint8_t>(bits >> (8*byte));
        transform(block_.data());
        std::array<std::uint8_t,32> result{};
        for (unsigned word=0; word<state_.size(); ++word)
            for (unsigned byte=0; byte<4; ++byte)
                result[word*4+byte] = static_cast<std::uint8_t>(
                  state_[word] >> (24-8*byte));
        return result;
    }
  private:
    static std::uint32_t rotate(std::uint32_t value, unsigned count) {
        return (value >> count) | (value << (32-count));
    }
    void transform(const std::uint8_t* block) {
        static constexpr std::array<std::uint32_t,64> constants{{
          0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
          0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
          0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
          0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
          0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
          0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
          0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
          0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2}};
        std::array<std::uint32_t,64> words{};
        for (unsigned i=0;i<16;++i)
            words[i]=(std::uint32_t(block[i*4])<<24)|
              (std::uint32_t(block[i*4+1])<<16)|
              (std::uint32_t(block[i*4+2])<<8)|block[i*4+3];
        for (unsigned i=16;i<64;++i) {
            const std::uint32_t s0=rotate(words[i-15],7)^
              rotate(words[i-15],18)^(words[i-15]>>3);
            const std::uint32_t s1=rotate(words[i-2],17)^
              rotate(words[i-2],19)^(words[i-2]>>10);
            words[i]=words[i-16]+s0+words[i-7]+s1;
        }
        auto [a,b,c,d,e,f,g,h]=state_;
        for (unsigned i=0;i<64;++i) {
            const std::uint32_t s1=rotate(e,6)^rotate(e,11)^rotate(e,25);
            const std::uint32_t first=h+s1+((e&f)^(~e&g))+constants[i]+words[i];
            const std::uint32_t s0=rotate(a,2)^rotate(a,13)^rotate(a,22);
            const std::uint32_t second=s0+((a&b)^(a&c)^(b&c));
            h=g;g=f;f=e;e=d+first;d=c;c=b;b=a;a=first+second;
        }
        state_[0]+=a;state_[1]+=b;state_[2]+=c;state_[3]+=d;
        state_[4]+=e;state_[5]+=f;state_[6]+=g;state_[7]+=h;
    }
    std::array<std::uint32_t,8> state_{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    std::array<std::uint8_t,64> block_{};
    std::size_t used_=0;
    std::uint64_t total_=0;
};

[[nodiscard]] std::string hex_digest(const std::array<std::uint8_t,32>& value) {
    std::ostringstream output; output<<std::hex<<std::setfill('0');
    for (std::uint8_t byte:value) output<<std::setw(2)<<unsigned(byte);
    return output.str();
}

[[nodiscard]] std::string sha256_file(const std::string& path) {
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("cannot hash "+path);
    Sha256 hash;std::array<char,1<<20>buffer{};
    while(input){input.read(buffer.data(),buffer.size());if(input.gcount()>0)
        hash.update(buffer.data(),static_cast<std::size_t>(input.gcount()));}
    return hex_digest(hash.finish());
}

[[nodiscard]] std::string sha256_range(const std::string& path,
                                       std::uint64_t offset,
                                       std::uint64_t bytes) {
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("cannot hash "+path);
    input.seekg(static_cast<std::streamoff>(offset));
    if(!input)throw std::runtime_error("cannot seek while hashing "+path);
    Sha256 hash;std::array<char,1<<20>buffer{};
    while(bytes){const std::size_t take=static_cast<std::size_t>(
        std::min<std::uint64_t>(bytes,buffer.size()));
        input.read(buffer.data(),static_cast<std::streamsize>(take));
        if(input.gcount()!=static_cast<std::streamsize>(take))
            throw std::runtime_error("truncated range while hashing "+path);
        hash.update(buffer.data(),take);bytes-=take;}
    return hex_digest(hash.finish());
}

[[nodiscard]] std::uint64_t mix64(std::uint64_t value) {
    value^=value>>30;value*=0xbf58476d1ce4e5b9ULL;
    value^=value>>27;value*=0x94d049bb133111ebULL;
    return value^(value>>31);
}

template<typename Value>
class MmapFile {
  public:
    MmapFile()=default;
    MmapFile(const std::string&path,std::uint64_t count,bool create,
             bool readOnly=false){open(path,count,create,readOnly);}
    ~MmapFile(){close();}
    MmapFile(MmapFile&&other)noexcept{swap(other);}
    MmapFile&operator=(MmapFile&&other)noexcept{
        if(this!=&other){close();swap(other);}return *this;}
    MmapFile(const MmapFile&)=delete;MmapFile&operator=(const MmapFile&)=delete;
    void open(const std::string&path,std::uint64_t count,bool create,
              bool readOnly=false){
        close();if(!count||count>std::numeric_limits<std::size_t>::max()/sizeof(Value))
            throw std::runtime_error("invalid mmap extent for "+path);
        path_=path;count_=count;bytes_=count*sizeof(Value);
        int flags=readOnly?O_RDONLY:O_RDWR;if(create)flags|=O_CREAT|O_TRUNC;
        descriptor_=::open(path.c_str(),flags,0600);
        if(descriptor_<0)system_error("cannot open",path);
        if(create&&::ftruncate(descriptor_,static_cast<off_t>(bytes_)))
            system_error("cannot size",path);
        struct stat status{};if(::fstat(descriptor_,&status)||
          static_cast<std::uint64_t>(status.st_size)!=bytes_)
            throw std::runtime_error("mmap extent mismatch for "+path);
        const int protection=readOnly?PROT_READ:PROT_READ|PROT_WRITE;
        data_=static_cast<Value*>(::mmap(nullptr,bytes_,protection,MAP_SHARED,
                                         descriptor_,0));
        if(data_==MAP_FAILED){data_=nullptr;system_error("cannot mmap",path);}
    }
    [[nodiscard]] Value&operator[](std::uint64_t index){
        if(index>=count_)throw std::out_of_range("mmap index");return data_[index];}
    [[nodiscard]] const Value&operator[](std::uint64_t index)const{
        if(index>=count_)throw std::out_of_range("mmap index");return data_[index];}
    [[nodiscard]] Value*begin(){return data_;}[[nodiscard]] Value*end(){return data_+count_;}
    [[nodiscard]] std::uint64_t size()const{return count_;}
    void fill(Value value){std::fill(begin(),end(),value);}
    void flush(){if(data_&&::msync(data_,bytes_,MS_SYNC))system_error("cannot flush",path_);}
  private:
    void close()noexcept{if(data_)::munmap(data_,bytes_);if(descriptor_>=0)::close(descriptor_);
        data_=nullptr;descriptor_=-1;bytes_=count_=0;}
    void swap(MmapFile&other)noexcept{std::swap(path_,other.path_);std::swap(descriptor_,other.descriptor_);
        std::swap(data_,other.data_);std::swap(bytes_,other.bytes_);std::swap(count_,other.count_);}
    std::string path_;int descriptor_=-1;Value*data_=nullptr;
    std::uint64_t bytes_=0,count_=0;
};

struct PairNode { std::uint16_t variable=0; PairRobdd::Id low=0,high=0; };
static_assert(sizeof(PairNode)==12);

struct ApplyKey { std::uint8_t op=0;PairRobdd::Id lhs=0,rhs=0;
    friend bool operator==(const ApplyKey&a,const ApplyKey&b){return a.op==b.op&&a.lhs==b.lhs&&a.rhs==b.rhs;}};
struct ApplyHash { std::size_t operator()(const ApplyKey&key)const{return static_cast<std::size_t>(
    mix64(key.lhs^(mix64(key.rhs)<<1)^key.op));}};
struct UnaryKey { PairRobdd::Id root=0;std::uint64_t relation=0;
    friend bool operator==(const UnaryKey&a,const UnaryKey&b){return a.root==b.root&&a.relation==b.relation;}};
struct UnaryHash { std::size_t operator()(const UnaryKey&key)const{return static_cast<std::size_t>(mix64(key.root^mix64(key.relation)));}};

struct DomainKey {
    PairMask mask;
    std::uint32_t variableCount=0;
    friend bool operator==(const DomainKey&lhs,const DomainKey&rhs){
        return lhs.variableCount==rhs.variableCount&&lhs.mask==rhs.mask;}
};

struct DomainHash {
    std::size_t operator()(const DomainKey&key)const noexcept {
        std::uint64_t value=mix64(key.variableCount);
        for(std::uint64_t word:key.mask.words)
            value=mix64(value^mix64(word));
        return static_cast<std::size_t>(value);
    }
};

}  // namespace

namespace {

#pragma pack(push,1)
struct TransitionHeaderDisk {
    std::array<char,8> magic{};
    std::uint32_t version=TransitionVersion;
    std::uint32_t headerBytes=sizeof(TransitionHeaderDisk);
    std::uint32_t rawBegin=0;
    std::uint32_t rawCount=0;
    std::uint32_t rawDomain=RawGeometryCount;
    std::uint32_t complete=0;
    std::uint64_t geometries=0;
    std::uint64_t worlds=0;
    std::uint64_t liveWorlds=0;
    std::uint64_t ownerRoots=0;
    std::uint64_t admitted=0;
    std::uint64_t terminal=0;
    std::uint64_t actions=0;
    std::uint64_t observations=0;
    std::uint64_t edges=0;
    std::uint64_t strata=0;
    std::uint64_t actuals=0;
    std::uint64_t blockBytes=0;
    std::array<char,64> sourceSha{};
    std::array<char,64> modelSha{};
    std::array<char,64> observationSha{};
    std::array<char,64> payloadSha{};
};

struct VerifiedDisk {
    std::array<char,8> magic{};
    std::uint32_t version=1;
    std::uint32_t bytes=sizeof(VerifiedDisk);
    std::uint32_t rawBegin=0;
    std::uint32_t rawCount=0;
    std::array<char,64> sourceSha{};
    std::array<char,64> modelSha{};
    std::array<char,64> observationSha{};
    std::array<char,64> payloadSha{};
    std::array<char,64> headerSha{};
};
#pragma pack(pop)

struct GeometryDisk {
    std::uint32_t raw=0;
    std::uint32_t variableCount=0;
    std::uint64_t ownerBase=0;
    std::uint64_t actualBase=0;
    std::uint32_t stratumBase=0;
    std::uint32_t stratumCount=0;
    std::uint32_t liveCount=0;
    PairMask live;
    PairMask terminal;
    PairMask terminalWhite;
    PairMask terminalBlack;
    PairMask admittedFresh;
};

struct ActionDisk { ActionKey key; };
struct EdgeDisk { CompiledEdge edge; };
struct BlockHeaderDisk {
    std::uint32_t variableCount=0;
    std::uint32_t actionCount=0;
    std::uint32_t edgeCount=0;
};

#pragma pack(push,1)
struct ArbitraryGeometryDisk {
    std::uint32_t raw=0;
    std::uint32_t variableCount=0;
    std::uint64_t ownerBase=0;
    std::uint64_t actualBase=0;
    std::uint64_t stratumBase=0;
    std::uint32_t stratumCount=0;
    std::uint32_t liveCount=0;
};

struct ArbitraryHeaderDisk {
    std::array<char,8> magic{{'U','F','G','G','1','\0','\0','\0'}};
    std::uint32_t version=1;
    std::uint32_t headerBytes=sizeof(ArbitraryHeaderDisk);
    std::uint32_t primary=static_cast<std::uint32_t>(PieceType::Ghost);
    std::uint32_t secondary=static_cast<std::uint32_t>(PieceType::Ghost);
    std::uint32_t owner=static_cast<std::uint32_t>(Color::White);
    std::uint32_t files=Position::BoardFiles;
    std::uint32_t ranks=Position::BoardRanks;
    std::uint32_t squares=Squares;
    std::uint32_t variables=MaximumWorlds;
    std::uint32_t stateCount=StateCount;
    std::uint32_t nodeBytes=sizeof(PairRobdd::NodeRecord);
    std::uint32_t geometryBytes=sizeof(ArbitraryGeometryDisk);
    std::uint32_t maskBytes=sizeof(PairMask);
    std::uint32_t reserved=0;
    std::uint64_t nodes=0;
    std::uint64_t geometries=0;
    std::uint64_t strata=0;
    std::uint64_t actuals=0;
    std::uint64_t ownerRoots=0;
    std::uint64_t nodeOffset=0;
    std::uint64_t geometryOffset=0;
    std::uint64_t stratumOffset=0;
    std::uint64_t actualOffset=0;
    std::uint64_t ownerOffset=0;
    std::uint64_t observerOffset=0;
    std::uint64_t payloadBytes=0;
    std::array<char,64> sourceSha{};
    std::array<char,64> modelSha{};
    std::array<char,64> observationSha{};
    std::array<char,64> transitionPayloadSha{};
    std::array<char,64> transitionHeaderSha{};
    std::array<char,64> transitionMarkerSha{};
    std::array<char,64> lowerSidecarSha{};
    std::array<char,64> lowerSourceSha{};
    std::array<char,64> lowerModelSha{};
    std::array<char,64> lowerObservationSha{};
    std::array<char,64> payloadSha{};
    std::array<char,64> semantics{};
};
#pragma pack(pop)
static_assert(sizeof(PairRobdd::NodeRecord)==12);

struct BuiltBlock {
    GeometryDisk meta;
    std::vector<PairMask> strata;
    std::vector<std::uint32_t> actualStratum;
    BlockHeaderDisk header;
    std::vector<std::uint32_t> edgeOffsets;
    std::vector<ActionDisk> actions;
    std::vector<EdgeDisk> edges;
    std::uint64_t observationCount=0;
};

[[nodiscard]] std::string combined_payload_sha(const std::string&prefix){
    Sha256 hash;for(const char*suffix:{".meta",".strata",".actual",".index",".blocks"}){
        std::ifstream input(prefix+suffix,std::ios::binary);if(!input)throw std::runtime_error("cannot hash Ghost-pair transition payload");
        std::array<char,1<<20>buffer{};while(input){input.read(buffer.data(),buffer.size());if(input.gcount()>0)
            hash.update(buffer.data(),static_cast<std::size_t>(input.gcount()));}}
    return hex_digest(hash.finish());
}

void write_verified_marker(const std::string&prefix,const TransitionHeaderDisk&header){
    VerifiedDisk marker;std::copy(std::begin(VerifiedMagic),std::end(VerifiedMagic),marker.magic.begin());
    marker.rawBegin=header.rawBegin;marker.rawCount=header.rawCount;marker.sourceSha=header.sourceSha;
    marker.modelSha=header.modelSha;marker.observationSha=header.observationSha;marker.payloadSha=header.payloadSha;
    const std::string headerSha=sha256_file(prefix+".header");std::copy(headerSha.begin(),headerSha.end(),marker.headerSha.begin());
    std::ofstream output(prefix+".verified",std::ios::binary|std::ios::trunc);write_value(output,marker);
    if(!output)throw std::runtime_error("cannot write Ghost-pair regeneration marker");
}

[[nodiscard]] VerifiedDisk read_verified_marker(const std::string&prefix,const TransitionHeaderDisk&header){
    std::ifstream input(prefix+".verified",std::ios::binary);const VerifiedDisk marker=read_value<VerifiedDisk>(input);
    if(marker.magic!=std::array<char,8>{'U','F','G','G','V','1',0,0}||marker.version!=1||
       marker.bytes!=sizeof(marker)||marker.rawBegin!=header.rawBegin||marker.rawCount!=header.rawCount||
       marker.sourceSha!=header.sourceSha||marker.modelSha!=header.modelSha||marker.observationSha!=header.observationSha||
       marker.payloadSha!=header.payloadSha||std::string(marker.headerSha.data(),64)!=sha256_file(prefix+".header")||
       input.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("Ghost-pair shard lacks exhaustive-regeneration marker");
    return marker;
}

void verify_transition_storage(const std::string&prefix,const TransitionHeaderDisk&header){
    if(combined_payload_sha(prefix)!=std::string(header.payloadSha.data(),64))
        throw std::runtime_error("Ghost-pair transition payload SHA mismatch");
    if(file_bytes(prefix+".meta")!=header.geometries*sizeof(GeometryDisk)||
       file_bytes(prefix+".strata")!=header.strata*sizeof(PairMask)||
       file_bytes(prefix+".actual")!=header.actuals*sizeof(std::uint32_t)||
       file_bytes(prefix+".index")!=(header.geometries+1)*sizeof(std::uint64_t)||
       file_bytes(prefix+".blocks")!=header.blockBytes)
        throw std::runtime_error("Ghost-pair transition extent residual");
    std::ifstream index(prefix+".index",std::ios::binary);std::uint64_t previous=read_value<std::uint64_t>(index);
    if(previous)throw std::runtime_error("Ghost-pair transition index does not begin at zero");
    for(std::uint64_t id=0;id<header.geometries;++id){const std::uint64_t next=read_value<std::uint64_t>(index);
        if(next<previous||next>header.blockBytes)throw std::runtime_error("Ghost-pair transition index not monotonic");previous=next;}
    if(previous!=header.blockBytes||index.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("Ghost-pair transition index coverage residual");
}

[[nodiscard]] TransitionHeaderDisk read_header(const std::string&prefix){
    std::ifstream input(prefix+".header",std::ios::binary);const TransitionHeaderDisk header=read_value<TransitionHeaderDisk>(input);
    if(header.magic!=std::array<char,8>{'U','F','G','G','T','1',0,0}||header.version!=TransitionVersion||
       header.headerBytes!=sizeof(header)||header.rawDomain!=RawGeometryCount||
       input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("invalid Ghost-pair transition header");
    return header;
}

[[nodiscard]] TransitionHeaderDisk authenticate_transition_database(const std::string&prefix,
  const std::string&source,const std::string&model,const std::string&observation,bool complete){
    const TransitionHeaderDisk header=read_header(prefix);
    if((complete&&(!header.complete||header.rawBegin||header.rawCount!=RawGeometryCount))||
       std::string(header.sourceSha.data(),64)!=source||std::string(header.modelSha.data(),64)!=model||
       std::string(header.observationSha.data(),64)!=observation)
        throw std::runtime_error("Ghost-pair transition authenticated header mismatch");
    verify_transition_storage(prefix,header);(void)read_verified_marker(prefix,header);return header;
}

[[nodiscard]] CompiledEdge encode_child(const Position&child,const std::optional<FramedWorld>&physical){
    const ClassifiedChild classified=classify_child(child);CompiledEdge result;result.childConcrete=classified.index;
    switch(classified.domain){
      case ChildDomain::SameClass:{if(!physical)throw std::runtime_error("same-class pair child lacks physical state");
        const auto[raw,transform]=canonical_geometry(physical->frame);const FramedWorld mapped=transform_world(
          physical->frame,physical->world,transform);result.domain=CompiledChildDomain::SameClass;
        result.childGeometry=raw;result.childActual=static_cast<std::uint16_t>(pair_variable(mapped.frame,mapped.world));break;}
      case ChildDomain::LowerGhost:result.domain=CompiledChildDomain::LowerGhost;break;
      case ChildDomain::ExactTerminal:result.domain=CompiledChildDomain::ExactTerminal;result.terminalForces=terminal_flags(child);break;
      default:throw std::runtime_error("Ghost-pair transition escaped exact closure");
    }return result;
}

[[nodiscard]] std::string black_transition_observation(
  const Position&before,const Move&move,const Position&child){
    std::string key=transition_observation_key(before,move,child,
                                               {Color::Black,false});
    if(!child.game_over()&&child.side_to_move()==Color::Black)
        key+=decision_observation_key(child,{Color::Black,false});
    if(key.empty())throw std::runtime_error("empty Ghost-pair observation");
    return key;
}

[[nodiscard]] BuiltBlock build_block(const PublicFrame&frame,bool symmetryCertificate,
                                     TransitionCertificate&certificate){
    BuiltBlock result{};
    // GeometryDisk is naturally aligned so PairMask is safe on every target.
    // Clear the complete record, including ABI padding, before hashing/writing
    // so independent shard processes emit byte-identical payloads.
    std::memset(&result.meta,0,sizeof(result.meta));
    result.meta.raw=encode_geometry(frame);result.meta.variableCount=variable_count(frame);
    result.header.variableCount=result.meta.variableCount;result.actualStratum.assign(result.meta.variableCount,NoIndex);
    const std::vector<PairWorld>worlds=geometric_worlds(frame);std::vector<PairWorld>live;
    for(const PairWorld&world:worlds){const unsigned variable=pair_variable(frame,world);const Position position=make_position(frame,world);
        if(position.game_over()){result.meta.terminal.set(variable);const std::uint8_t flags=terminal_flags(position);
            if(flags&1)result.meta.terminalWhite.set(variable);if(flags&2)result.meta.terminalBlack.set(variable);++certificate.terminalWorlds;}
        else{result.meta.live.set(variable);live.push_back(world);}
        if(fresh_world_admission(frame,world)==AdmissionVerdict::Admit){result.meta.admittedFresh.set(variable);++certificate.admittedFreshWorlds;}}
    if(result.meta.live.count()+result.meta.terminal.count()!=
         result.meta.variableCount)
        throw std::runtime_error("Ghost-pair live/terminal partition residual");
    result.meta.liveCount=result.meta.live.count();
    const std::vector<DecisionBucket>decisions=live.empty()?std::vector<DecisionBucket>{}:decision_partition(frame,live);
    if(frame.side==Color::Black){for(const DecisionBucket&bucket:decisions){const std::uint32_t local=result.strata.size();
        result.strata.push_back(bucket.worlds);for(unsigned variable=0;variable<result.meta.variableCount;++variable)
            if(bucket.worlds.test(variable))result.actualStratum[variable]=local;}}
    else{result.strata.push_back(result.meta.live);for(unsigned variable=0;variable<result.meta.variableCount;++variable)
        if(result.meta.live.test(variable))result.actualStratum[variable]=0;}
    result.meta.stratumCount=result.strata.size();
    PairMask decisionUnion;std::uint64_t decisionMembers=0;
    for(const PairMask&stratum:result.strata){decisionMembers+=stratum.count();
        for(unsigned variable=0;variable<result.meta.variableCount;++variable)
            if(stratum.test(variable)){if(decisionUnion.test(variable))
                throw std::runtime_error("overlapping Ghost-pair decision cells");
                decisionUnion.set(variable);}}
    if(decisionMembers!=result.meta.liveCount||
       !(decisionUnion==result.meta.live))
        throw std::runtime_error("Ghost-pair decision partition residual");
    std::map<ActionKey,std::vector<PairWorld>>byAction;
    for(const PairWorld&world:live){const Position position=make_position(frame,world);const std::vector<ActionKey>actions=legal_actions(position);
        if(symmetryCertificate){for(std::uint8_t raw=0;raw<4;++raw){const auto transform=static_cast<RectangleTransform>(raw);
            const FramedWorld mapped=transform_world(frame,world,transform);std::vector<ActionKey>expected;
            for(ActionKey action:actions)expected.push_back(transform_action(action,transform));std::sort(expected.begin(),expected.end());
            if(expected!=legal_actions(make_position(mapped.frame,mapped.world)))throw std::runtime_error("Ghost-pair D2 action residual");}}
        for(const ActionKey&action:actions)byAction[action].push_back(world);}
    struct Binding{std::uint32_t id=0;CompiledChildDomain domain=CompiledChildDomain::ExactTerminal;std::uint32_t frame=0;};
    std::map<std::string,Binding>relations;std::vector<std::vector<EdgeDisk>>sourceEdges(result.meta.variableCount);
    std::uint32_t actionId=0;
    for(const auto&[action,legalWorlds]:byAction){result.actions.push_back({action});
        std::map<std::string,std::vector<PairWorld>>partitions;
        for(const PairWorld&world:legalWorlds){Position before=make_position(frame,world);const auto move=find_action(before,action);
            if(!move)throw std::runtime_error("Ghost-pair action disappeared");Position child=before;Undo undo;
            if(!child.make_move(*move,undo))throw std::runtime_error("Ghost-pair native move failed");
            const std::string key=black_transition_observation(before,*move,
                                                                child);
            partitions[key].push_back(world);}
        for(const auto&[key,partition]:partitions){std::optional<CompiledChildDomain>domain;std::uint32_t childFrame=0;
            std::vector<std::pair<unsigned,CompiledEdge>>encoded;
            for(const PairWorld&world:partition){Position before=make_position(frame,world);const auto move=find_action(before,action);
                Position child=before;Undo undo;if(!move||!child.make_move(*move,undo))throw std::runtime_error("Ghost-pair partition move residual");
                const ClassifiedChild classified=classify_child(child);const std::optional<FramedWorld>physical=
                  classified.domain==ChildDomain::SameClass?same_class_product(child):std::nullopt;
                CompiledEdge edge=encode_child(child,physical);if(!domain){domain=edge.domain;childFrame=edge.childGeometry;}
                else if(*domain!=edge.domain||(edge.domain==CompiledChildDomain::SameClass&&edge.childGeometry!=childFrame))
                    throw std::runtime_error("one Ghost-pair observation mixes child domains/frames");
                encoded.emplace_back(pair_variable(frame,world),edge);}
            const auto[binding,inserted]=relations.emplace(key,Binding{static_cast<std::uint32_t>(relations.size()),*domain,childFrame});
            if(!inserted&&(binding->second.domain!=*domain||binding->second.frame!=childFrame))
                throw std::runtime_error("one Ghost-pair observation has inconsistent public child");
            for(auto[source,edge]:encoded){edge.action=actionId;edge.relation=binding->second.id;sourceEdges[source].push_back({edge});
                switch(edge.domain){case CompiledChildDomain::SameClass:++certificate.sameClass;break;
                  case CompiledChildDomain::LowerGhost:++certificate.lowerGhost;break;
                  case CompiledChildDomain::ExactTerminal:++certificate.exactTerminal;break;}}
        }++actionId;}
    result.observationCount=relations.size();result.header.actionCount=result.actions.size();
    result.edgeOffsets.resize(result.meta.variableCount+1);
    for(unsigned source=0;source<result.meta.variableCount;++source){result.edgeOffsets[source]=result.edges.size();
        result.edges.insert(result.edges.end(),sourceEdges[source].begin(),sourceEdges[source].end());}
    result.edgeOffsets.back()=result.edges.size();result.header.edgeCount=result.edges.size();return result;
}

void write_block(std::ostream&output,const BuiltBlock&block){write_value(output,block.header);
    if(!block.edgeOffsets.empty())output.write(reinterpret_cast<const char*>(block.edgeOffsets.data()),block.edgeOffsets.size()*sizeof(std::uint32_t));
    if(!block.actions.empty())output.write(reinterpret_cast<const char*>(block.actions.data()),block.actions.size()*sizeof(ActionDisk));
    if(!block.edges.empty())output.write(reinterpret_cast<const char*>(block.edges.data()),block.edges.size()*sizeof(EdgeDisk));}

[[nodiscard]] std::uint64_t block_size(const BuiltBlock&block){return sizeof(BlockHeaderDisk)+
  block.edgeOffsets.size()*sizeof(std::uint32_t)+block.actions.size()*sizeof(ActionDisk)+block.edges.size()*sizeof(EdgeDisk);}

}  // namespace

TransitionCertificate compile_transition_database(const TransitionCompileOptions&options){
    require_hash(options.sourceSha256,"source SHA-256");require_hash(options.modelSha256,"model SHA-256");
    require_hash(options.observationSha256,"observation SHA-256");
    if(options.prefix.empty()||options.rawGeometryBegin>=RawGeometryCount)throw std::invalid_argument("invalid Ghost-pair transition range");
    const std::uint32_t remaining=RawGeometryCount-options.rawGeometryBegin;const std::uint32_t count=options.rawGeometryCount?
      std::min(options.rawGeometryCount,remaining):remaining;
    std::ofstream meta(options.prefix+".meta",std::ios::binary|std::ios::trunc),
      strata(options.prefix+".strata",std::ios::binary|std::ios::trunc),actual(options.prefix+".actual",std::ios::binary|std::ios::trunc),
      index(options.prefix+".index",std::ios::binary|std::ios::trunc),blocks(options.prefix+".blocks",std::ios::binary|std::ios::trunc);
    if(!meta||!strata||!actual||!index||!blocks)throw std::runtime_error("cannot create Ghost-pair transition database");
    TransitionCertificate certificate;certificate.rawGeometries=count;std::uint64_t blockOffset=0,stratumBase=0,actualBase=0,ownerBase=0;
    write_value(index,blockOffset);
    for(std::uint32_t offset=0;offset<count;++offset){const std::uint32_t raw=options.rawGeometryBegin+offset;
        const PublicFrame frame=decode_geometry(raw);if(canonical_geometry(frame).first!=raw)continue;
        BuiltBlock block=build_block(frame,options.exhaustiveSymmetryCertificate,certificate);
        if(stratumBase>std::numeric_limits<std::uint32_t>::max()||block.strata.size()>std::numeric_limits<std::uint32_t>::max()-stratumBase)
            throw std::overflow_error("Ghost-pair stratum codec overflow");
        block.meta.stratumBase=static_cast<std::uint32_t>(stratumBase);block.meta.actualBase=actualBase;block.meta.ownerBase=ownerBase;
        write_value(meta,block.meta);if(!block.strata.empty())strata.write(reinterpret_cast<const char*>(block.strata.data()),block.strata.size()*sizeof(PairMask));
        if(!block.actualStratum.empty())actual.write(reinterpret_cast<const char*>(block.actualStratum.data()),block.actualStratum.size()*sizeof(std::uint32_t));
        write_block(blocks,block);blockOffset+=block_size(block);write_value(index,blockOffset);
        stratumBase+=block.strata.size();actualBase+=block.actualStratum.size();ownerBase+=block.meta.liveCount;
        ++certificate.canonicalGeometries;certificate.worlds+=block.meta.variableCount;certificate.liveWorlds+=block.meta.liveCount;
        certificate.actions+=block.actions.size();certificate.observations+=block.observationCount;certificate.edges+=block.edges.size();}
    meta.close();strata.close();actual.close();index.close();blocks.close();TransitionHeaderDisk header;
    std::copy(std::begin(TransitionMagic),std::end(TransitionMagic),header.magic.begin());header.rawBegin=options.rawGeometryBegin;
    header.rawCount=count;header.complete=options.rawGeometryBegin==0&&count==RawGeometryCount;header.geometries=certificate.canonicalGeometries;
    header.worlds=certificate.worlds;header.liveWorlds=certificate.liveWorlds;header.ownerRoots=ownerBase;
    header.admitted=certificate.admittedFreshWorlds;header.terminal=certificate.terminalWorlds;header.actions=certificate.actions;
    header.observations=certificate.observations;header.edges=certificate.edges;header.strata=stratumBase;header.actuals=actualBase;
    header.blockBytes=blockOffset;std::copy(options.sourceSha256.begin(),options.sourceSha256.end(),header.sourceSha.begin());
    std::copy(options.modelSha256.begin(),options.modelSha256.end(),header.modelSha.begin());
    std::copy(options.observationSha256.begin(),options.observationSha256.end(),header.observationSha.begin());
    certificate.payloadSha256=combined_payload_sha(options.prefix);std::copy(certificate.payloadSha256.begin(),certificate.payloadSha256.end(),header.payloadSha.begin());
    std::ofstream headerFile(options.prefix+".header",std::ios::binary|std::ios::trunc);write_value(headerFile,header);
    if(!headerFile)throw std::runtime_error("cannot write Ghost-pair transition header");headerFile.close();
    const TransitionCertificate verified=verify_transition_database(options.prefix,options.sourceSha256,options.modelSha256,options.observationSha256,false);
    if(verified.canonicalGeometries!=certificate.canonicalGeometries||verified.edges!=certificate.edges||verified.payloadSha256!=certificate.payloadSha256)
        throw std::runtime_error("Ghost-pair compile/regeneration certificate mismatch");
    write_verified_marker(options.prefix,header);return certificate;
}

TransitionCertificate verify_transition_database(const std::string&prefix,const std::string&sourceSha256,
  const std::string&modelSha256,const std::string&observationSha256,bool requireComplete){
    require_hash(sourceSha256,"source SHA-256");require_hash(modelSha256,"model SHA-256");require_hash(observationSha256,"observation SHA-256");
    const TransitionHeaderDisk header=read_header(prefix);if((requireComplete&&(!header.complete||header.rawBegin||header.rawCount!=RawGeometryCount))||
       std::string(header.sourceSha.data(),64)!=sourceSha256||std::string(header.modelSha.data(),64)!=modelSha256||
       std::string(header.observationSha.data(),64)!=observationSha256)throw std::runtime_error("Ghost-pair transition binding mismatch");
    verify_transition_storage(prefix,header);std::ifstream metas(prefix+".meta",std::ios::binary),strata(prefix+".strata",std::ios::binary),
      actual(prefix+".actual",std::ios::binary),indices(prefix+".index",std::ios::binary),blocks(prefix+".blocks",std::ios::binary);
    std::uint64_t previous=read_value<std::uint64_t>(indices),ordinal=0,stratumBase=0,actualBase=0,ownerBase=0;
    TransitionCertificate regenerated;regenerated.rawGeometries=header.rawCount;
    for(std::uint32_t offset=0;offset<header.rawCount;++offset){const std::uint32_t raw=header.rawBegin+offset;
        const PublicFrame frame=decode_geometry(raw);if(canonical_geometry(frame).first!=raw)continue;
        BuiltBlock expected=build_block(frame,true,regenerated);
        if(stratumBase>std::numeric_limits<std::uint32_t>::max()||
           expected.strata.size()>std::numeric_limits<std::uint32_t>::max()-stratumBase)
            throw std::overflow_error("regenerated Ghost-pair stratum overflow");
        expected.meta.stratumBase=static_cast<std::uint32_t>(stratumBase);
        expected.meta.actualBase=actualBase;expected.meta.ownerBase=ownerBase;const GeometryDisk stored=read_value<GeometryDisk>(metas);
        if(std::memcmp(&stored,&expected.meta,sizeof(stored)))throw std::runtime_error("Ghost-pair metadata regeneration residual");
        for(const PairMask&mask:expected.strata)if(!(read_value<PairMask>(strata)==mask))throw std::runtime_error("Ghost-pair stratum regeneration residual");
        for(std::uint32_t value:expected.actualStratum)if(read_value<std::uint32_t>(actual)!=value)throw std::runtime_error("Ghost-pair actual-stratum regeneration residual");
        const std::uint64_t next=read_value<std::uint64_t>(indices);blocks.clear();blocks.seekg(static_cast<std::streamoff>(previous));
        const BlockHeaderDisk storedHeader=read_value<BlockHeaderDisk>(blocks);if(std::memcmp(&storedHeader,&expected.header,sizeof(storedHeader)))
            throw std::runtime_error("Ghost-pair block-header regeneration residual");
        std::vector<std::uint32_t>offsets(expected.edgeOffsets.size());std::vector<ActionDisk>actions(expected.actions.size());std::vector<EdgeDisk>edges(expected.edges.size());
        blocks.read(reinterpret_cast<char*>(offsets.data()),offsets.size()*sizeof(std::uint32_t));
        blocks.read(reinterpret_cast<char*>(actions.data()),actions.size()*sizeof(ActionDisk));blocks.read(reinterpret_cast<char*>(edges.data()),edges.size()*sizeof(EdgeDisk));
        if(!blocks||offsets!=expected.edgeOffsets||actions.size()!=expected.actions.size()||edges.size()!=expected.edges.size()||
           (!actions.empty()&&std::memcmp(actions.data(),expected.actions.data(),actions.size()*sizeof(ActionDisk)))||
           (!edges.empty()&&std::memcmp(edges.data(),expected.edges.data(),edges.size()*sizeof(EdgeDisk)))||
           static_cast<std::uint64_t>(blocks.tellg())!=next)throw std::runtime_error("Ghost-pair action/edge regeneration residual");
        previous=next;stratumBase+=expected.strata.size();actualBase+=expected.actualStratum.size();ownerBase+=expected.meta.liveCount;
        ++ordinal;++regenerated.canonicalGeometries;regenerated.worlds+=expected.meta.variableCount;regenerated.liveWorlds+=expected.meta.liveCount;
        regenerated.actions+=expected.actions.size();regenerated.observations+=expected.observationCount;regenerated.edges+=expected.edges.size();}
    if(ordinal!=header.geometries||stratumBase!=header.strata||actualBase!=header.actuals||ownerBase!=header.ownerRoots||
       previous!=header.blockBytes||regenerated.worlds!=header.worlds||regenerated.liveWorlds!=header.liveWorlds||
       regenerated.admittedFreshWorlds!=header.admitted||regenerated.terminalWorlds!=header.terminal||regenerated.actions!=header.actions||
       regenerated.observations!=header.observations||regenerated.edges!=header.edges||metas.peek()!=std::char_traits<char>::eof()||
       strata.peek()!=std::char_traits<char>::eof()||actual.peek()!=std::char_traits<char>::eof()||indices.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("Ghost-pair transition global regeneration residual");
    regenerated.payloadSha256.assign(header.payloadSha.data(),64);return regenerated;
}

TransitionCertificate merge_transition_databases(const std::vector<std::string>&prefixes,const std::string&outputPrefix,
  const std::string&sourceSha256,const std::string&modelSha256,const std::string&observationSha256,bool requireComplete){
    require_hash(sourceSha256,"source SHA-256");require_hash(modelSha256,"model SHA-256");require_hash(observationSha256,"observation SHA-256");
    if(prefixes.empty()||outputPrefix.empty())throw std::invalid_argument("Ghost-pair merge needs shards/output");
    struct Shard{std::string prefix;TransitionHeaderDisk header;};std::vector<Shard>shards;
    for(const std::string&prefix:prefixes){if(prefix==outputPrefix)throw std::invalid_argument("Ghost-pair merge aliases input");
        shards.push_back({prefix,authenticate_transition_database(prefix,sourceSha256,modelSha256,observationSha256,false)});}
    std::sort(shards.begin(),shards.end(),[](const Shard&a,const Shard&b){return a.header.rawBegin<b.header.rawBegin;});
    const std::uint32_t begin=shards.front().header.rawBegin;std::uint32_t expected=begin;
    for(const Shard&shard:shards){if(shard.header.rawBegin!=expected||shard.header.rawCount>RawGeometryCount-expected)
            throw std::runtime_error("Ghost-pair shards are not exact gap-free coverage");
        expected+=shard.header.rawCount;}
    if(requireComplete&&(begin||expected!=RawGeometryCount))throw std::runtime_error("Ghost-pair shards do not cover full domain");
    std::ofstream meta(outputPrefix+".meta",std::ios::binary|std::ios::trunc),strata(outputPrefix+".strata",std::ios::binary|std::ios::trunc),
      actual(outputPrefix+".actual",std::ios::binary|std::ios::trunc),index(outputPrefix+".index",std::ios::binary|std::ios::trunc),
      blocks(outputPrefix+".blocks",std::ios::binary|std::ios::trunc);if(!meta||!strata||!actual||!index||!blocks)
        throw std::runtime_error("cannot create merged Ghost-pair transitions");
    TransitionHeaderDisk merged;std::copy(std::begin(TransitionMagic),std::end(TransitionMagic),merged.magic.begin());merged.rawBegin=begin;
    merged.rawCount=expected-begin;merged.complete=begin==0&&expected==RawGeometryCount;std::copy(sourceSha256.begin(),sourceSha256.end(),merged.sourceSha.begin());
    std::copy(modelSha256.begin(),modelSha256.end(),merged.modelSha.begin());std::copy(observationSha256.begin(),observationSha256.end(),merged.observationSha.begin());
    std::uint64_t blockBase=0,stratumBase=0,actualBase=0,ownerBase=0;write_value(index,blockBase);std::array<char,1<<20>buffer{};
    for(const Shard&shard:shards){std::ifstream sourceMeta(shard.prefix+".meta",std::ios::binary),sourceStrata(shard.prefix+".strata",std::ios::binary),
        sourceActual(shard.prefix+".actual",std::ios::binary),sourceIndex(shard.prefix+".index",std::ios::binary),sourceBlocks(shard.prefix+".blocks",std::ios::binary);
        (void)read_value<std::uint64_t>(sourceIndex);for(std::uint64_t id=0;id<shard.header.geometries;++id){GeometryDisk item=read_value<GeometryDisk>(sourceMeta);
            if(stratumBase>std::numeric_limits<std::uint32_t>::max()||item.stratumBase>std::numeric_limits<std::uint32_t>::max()-stratumBase)
                throw std::overflow_error("merged Ghost-pair stratum overflow");
            item.stratumBase=static_cast<std::uint32_t>(item.stratumBase+stratumBase);
            item.actualBase+=actualBase;item.ownerBase+=ownerBase;write_value(meta,item);write_value(index,blockBase+read_value<std::uint64_t>(sourceIndex));}
        const auto copy=[&](std::istream&input,std::ostream&output){while(input){input.read(buffer.data(),buffer.size());if(input.gcount()>0)output.write(buffer.data(),input.gcount());}};
        copy(sourceStrata,strata);copy(sourceActual,actual);copy(sourceBlocks,blocks);blockBase+=shard.header.blockBytes;stratumBase+=shard.header.strata;
        actualBase+=shard.header.actuals;ownerBase+=shard.header.ownerRoots;merged.geometries+=shard.header.geometries;merged.worlds+=shard.header.worlds;
        merged.liveWorlds+=shard.header.liveWorlds;merged.admitted+=shard.header.admitted;merged.terminal+=shard.header.terminal;
        merged.actions+=shard.header.actions;merged.observations+=shard.header.observations;merged.edges+=shard.header.edges;}
    merged.strata=stratumBase;merged.actuals=actualBase;merged.ownerRoots=ownerBase;merged.blockBytes=blockBase;
    meta.close();strata.close();actual.close();index.close();blocks.close();const std::string payload=combined_payload_sha(outputPrefix);
    std::copy(payload.begin(),payload.end(),merged.payloadSha.begin());std::ofstream headerFile(outputPrefix+".header",std::ios::binary|std::ios::trunc);
    write_value(headerFile,merged);if(!headerFile)throw std::runtime_error("cannot write merged Ghost-pair header");headerFile.close();
    const TransitionCertificate result=verify_transition_database(outputPrefix,sourceSha256,modelSha256,observationSha256,requireComplete);
    write_verified_marker(outputPrefix,merged);return result;
}

class PairRobdd::Impl {
  public:
    Impl(std::string path,Limits configured,bool create)
      :prefix(std::move(path)),limits(configured),
       nodes(prefix+".nodes",limits.maxNodes,create),
       unique(prefix+".unique",limits.uniqueSlots,create){
        if(!create)throw std::runtime_error("PairRobdd reload needs a root manifest");
        if(limits.variables!=MaximumWorlds||!limits.maxNodes||
           !limits.uniqueSlots||(limits.uniqueSlots&(limits.uniqueSlots-1)))
            throw std::invalid_argument("invalid PairRobdd limits");
        if(required_bytes(limits)>limits.budgetBytes)
            throw std::runtime_error("PairRobdd byte gate exceeded");
        unique.fill(Invalid);count=2;
        nodes[False]={static_cast<std::uint16_t>(limits.variables),False,False};
        nodes[True]={static_cast<std::uint16_t>(limits.variables),True,True};
        apply.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
          limits.applyCacheEntries,1'000'000)));
        unary.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
          limits.unaryCacheEntries,1'000'000)));
    }
    [[nodiscard]] const PairNode&node(Id id)const{
        if(id>=count)throw std::runtime_error("invalid PairRobdd node");return nodes[id];}
    [[nodiscard]] std::uint16_t top(Id id)const{return id<=True?limits.variables:node(id).variable;}
    [[nodiscard]] Id make(std::uint16_t variable,Id low,Id high){
        if(variable>=limits.variables||low>=count||high>=count)
            throw std::out_of_range("invalid PairRobdd tuple");
        if(low==high)return low;
        if((low>True&&node(low).variable<=variable)||(high>True&&node(high).variable<=variable))
            throw std::runtime_error("PairRobdd ordering violation");
        const std::uint64_t hash=mix64(variable^mix64(low)^(mix64(high)<<1));
        const std::uint64_t mask=limits.uniqueSlots-1;
        for(std::uint64_t probe=0;probe<limits.uniqueSlots;++probe){
            Id&slot=unique[(hash+probe)&mask];
            if(slot==Invalid){if(count>=limits.maxNodes)throw std::runtime_error("PairRobdd node gate exceeded");
                nodes[count]={variable,low,high};slot=count;return count++;}
            const PairNode&existing=node(slot);
            if(existing.variable==variable&&existing.low==low&&existing.high==high)return slot;
        }
        throw std::runtime_error("PairRobdd unique table is full");
    }
    [[nodiscard]] Id binary(std::uint8_t op,Id lhs,Id rhs){
        if(op==0){if(lhs==False||rhs==False)return False;if(lhs==True)return rhs;if(rhs==True)return lhs;}
        else{if(lhs==True||rhs==True)return True;if(lhs==False)return rhs;if(rhs==False)return lhs;}
        if(lhs==rhs)return lhs;if(lhs>rhs)std::swap(lhs,rhs);
        const ApplyKey key{op,lhs,rhs};if(const auto it=apply.find(key);it!=apply.end())return it->second;
        const std::uint16_t variable=std::min(top(lhs),top(rhs));
        const auto split=[&](Id root,bool high){return top(root)==variable?(high?node(root).high:node(root).low):root;};
        const Id result=make(variable,binary(op,split(lhs,false),split(rhs,false)),
          binary(op,split(lhs,true),split(rhs,true)));
        if(apply.size()<limits.applyCacheEntries)apply.emplace(key,result);return result;
    }
    [[nodiscard]] Id negate(Id root){
        if(root==False)return True;if(root==True)return False;
        const UnaryKey key{root,0};if(const auto it=unary.find(key);it!=unary.end())return it->second;
        const PairNode source=node(root);const Id result=make(source.variable,negate(source.low),negate(source.high));
        if(unary.size()<limits.unaryCacheEntries)unary.emplace(key,result);return result;
    }
    [[nodiscard]] Id ternary(Id condition,Id yes,Id no){
        return binary(1,binary(0,condition,yes),binary(0,negate(condition),no));}
    [[nodiscard]] Id compose(Id root,const std::vector<Id>&image,std::uint64_t relation){
        if(root<=True)return root;const UnaryKey key{root,relation+1};
        if(const auto it=unary.find(key);it!=unary.end())return it->second;
        const PairNode source=node(root);if(source.variable>=image.size())throw std::runtime_error("PairRobdd compose image too short");
        const Id result=ternary(image[source.variable],compose(source.high,image,relation),
          compose(source.low,image,relation));
        if(unary.size()<limits.unaryCacheEntries)unary.emplace(key,result);return result;
    }
    [[nodiscard]] Id subset_of(const PairMask&variables,unsigned variableCount){
        if(variableCount>limits.variables)
            throw std::out_of_range("PairRobdd domain size");
        // Build the canonical conjunction directly from high variables to
        // low.  The previous logical_and(logical_not(variable)) loop replayed
        // an ever-growing prefix through Apply for every excluded variable.
        // This emits the identical Boolean function with one unique-table
        // lookup per required node and no throwaway Apply nodes.
        Id result=True;
        for(unsigned variable=limits.variables;variable-->0;)
            if(variable>=variableCount||!variables.test(variable))
                result=make(static_cast<std::uint16_t>(variable),result,False);
        return result;
    }
    [[nodiscard]] bool eval(Id root,const PairMask&assignment)const{
        while(root>True){const PairNode source=node(root);root=assignment.test(source.variable)?source.high:source.low;}
        return root==True;
    }
    std::string prefix;Limits limits;MmapFile<PairNode>nodes;MmapFile<Id>unique;
    Id count=0;std::unordered_map<ApplyKey,Id,ApplyHash>apply;
    std::unordered_map<UnaryKey,Id,UnaryHash>unary;
};

PairRobdd::PairRobdd(const std::string&prefix,Limits limits,bool create)
  :impl_(new Impl(prefix,limits,create)){}
PairRobdd::~PairRobdd(){delete impl_;}
PairRobdd::PairRobdd(PairRobdd&&other)noexcept:impl_(std::exchange(other.impl_,nullptr)){}
PairRobdd&PairRobdd::operator=(PairRobdd&&other)noexcept{if(this!=&other){delete impl_;impl_=std::exchange(other.impl_,nullptr);}return *this;}
PairRobdd::Id PairRobdd::variable(unsigned variableIndex){if(variableIndex>=MaximumWorlds)throw std::out_of_range("PairRobdd variable");return impl_->make(static_cast<std::uint16_t>(variableIndex),False,True);}
PairRobdd::Id PairRobdd::logical_not(Id root){return impl_->negate(root);}
PairRobdd::Id PairRobdd::logical_and(Id lhs,Id rhs){return impl_->binary(0,lhs,rhs);}
PairRobdd::Id PairRobdd::logical_or(Id lhs,Id rhs){return impl_->binary(1,lhs,rhs);}
PairRobdd::Id PairRobdd::ite(Id condition,Id yes,Id no){return impl_->ternary(condition,yes,no);}
PairRobdd::Id PairRobdd::any(const PairMask&variables){Id result=False;
    for(unsigned word=0;word<MaskWords;++word){std::uint64_t bits=variables.words[word];
        while(bits){const unsigned bit=static_cast<unsigned>(__builtin_ctzll(bits));
            const unsigned index=word*64+bit;if(index<MaximumWorlds)
                result=logical_or(result,variable(index));
            bits&=bits-1;}}
    return result;}
PairRobdd::Id PairRobdd::subset_of(const PairMask&variables,unsigned count){
    return impl_->subset_of(variables,count);
}
PairRobdd::Id PairRobdd::compose(Id root,const std::vector<Id>&image,std::uint64_t relationId){return impl_->compose(root,image,relationId);}
bool PairRobdd::evaluate(Id root,const PairMask&assignment)const{return impl_->eval(root,assignment);}
bool PairRobdd::is_upward_closed(Id root,const PairMask&allowed){
    std::unordered_map<Id,bool>memo;std::function<bool(Id)>check=[&](Id current){
        if(current<=True)return true;if(const auto it=memo.find(current);it!=memo.end())return it->second;
        const PairNode node=impl_->node(current);bool result=check(node.low)&&check(node.high);
        if(result&&allowed.test(node.variable))result=logical_and(node.low,logical_not(node.high))==False;
        memo.emplace(current,result);return result;};return check(root);
}
bool PairRobdd::is_downward_closed(Id root){
    std::unordered_map<Id,bool>memo;std::function<bool(Id)>check=[&](Id current){
        if(current<=True)return true;if(const auto it=memo.find(current);it!=memo.end())return it->second;
        const PairNode node=impl_->node(current);const bool result=check(node.low)&&check(node.high)&&
          logical_and(node.high,logical_not(node.low))==False;memo.emplace(current,result);return result;};return check(root);
}
std::uint32_t PairRobdd::node_count()const{return impl_->count;}
PairRobdd::NodeRecord PairRobdd::node_record(Id id)const{
    const PairNode&node=impl_->node(id);return{node.variable,node.low,node.high};
}
std::uint64_t PairRobdd::required_bytes(const Limits&limits){return std::uint64_t(limits.maxNodes)*sizeof(PairNode)+limits.uniqueSlots*sizeof(Id);}

class DomainRootCache {
  public:
    explicit DomainRootCache(PairRobdd&bdd):bdd_(bdd){}

    [[nodiscard]] PairRobdd::Id root(const PairMask&mask,
                                     std::uint32_t variableCount){
        const DomainKey key{mask,variableCount};
        if(const auto found=roots_.find(key);found!=roots_.end()){
            ++hits_;return found->second;
        }
        ++misses_;
        const PairRobdd::Id result=bdd_.subset_of(mask,variableCount);
        if(roots_.size()<MaximumEntries)roots_.emplace(key,result);
        return result;
    }

    [[nodiscard]] std::uint64_t hits()const{return hits_;}
    [[nodiscard]] std::uint64_t misses()const{return misses_;}
    [[nodiscard]] std::uint64_t entries()const{return roots_.size();}

  private:
    // The production Ghost-pair graph currently has only three distinct
    // (mask, variable-count) domains among 9,739,120 strata.  The bound keeps
    // the optimization safe for future rule changes without turning an
    // unexpectedly diverse graph into an unbudgeted multi-gigabyte cache.
    static constexpr std::size_t MaximumEntries=1U<<16;
    PairRobdd&bdd_;
    std::unordered_map<DomainKey,PairRobdd::Id,DomainHash>roots_;
    std::uint64_t hits_=0;
    std::uint64_t misses_=0;
};

std::pair<PairRobdd,PairRobdd::CompactionCertificate>PairRobdd::compact(
  const std::string&replacementPrefix,const std::string&remapPath,
  std::vector<Id>&roots){
    PairRobdd target(replacementPrefix,impl_->limits,true);
    MmapFile<Id>remap(remapPath,std::max<std::uint32_t>(2,impl_->count),true);
    remap.fill(Invalid);remap[False]=False;remap[True]=True;std::uint64_t residual=0;
    std::function<Id(Id)>copy=[&](Id source){if(remap[source]!=Invalid)return remap[source];
        const PairNode node=impl_->node(source);const Id result=target.impl_->make(node.variable,copy(node.low),copy(node.high));
        remap[source]=result;const PairNode check=target.impl_->node(result);residual+=check.variable!=node.variable||check.low!=remap[node.low]||check.high!=remap[node.high];return result;};
    for(Id&root:roots)root=copy(root);remap.flush();CompactionCertificate cert;
    cert.oldNodes=impl_->count;cert.newNodes=target.impl_->count;cert.roots=roots.size();
    cert.structuralResidual=residual;if(residual)throw std::runtime_error("PairRobdd compaction residual");
    return {std::move(target),cert};
}

namespace {

[[nodiscard]] std::uint32_t rank_excluding(std::uint8_t square,
                                            std::initializer_list<std::uint8_t> occupied){
    std::uint32_t rank=square;for(std::uint8_t used:occupied)rank-=used<square;return rank;}
[[nodiscard]] std::uint8_t unrank_excluding(std::uint32_t rank,
  std::initializer_list<std::uint8_t> occupied){for(std::uint8_t square=0;square<Squares;++square){
    bool used=false;for(std::uint8_t item:occupied)used|=item==square;if(!used&&rank--==0)return square;}
    throw std::runtime_error("Ghost-pair public rank is invalid");}
[[nodiscard]] std::uint32_t pair_rank(std::uint32_t first,std::uint32_t second,std::uint32_t count){
    if(first>=second||second>=count)throw std::invalid_argument("invalid pair rank");
    return first*(2*count-first-1)/2+second-first-1;}
[[nodiscard]] std::pair<std::uint32_t,std::uint32_t>pair_unrank(std::uint32_t rank,std::uint32_t count){
    for(std::uint32_t first=0;first+1<count;++first){const std::uint32_t width=count-first-1;
        if(rank<width)return{first,first+1+rank};rank-=width;}throw std::runtime_error("pair rank decode failed");}

[[nodiscard]] std::uint32_t encode_geometry(const PublicFrame&frame){
    if(frame.whiteKing>=Squares||frame.blackKing>=Squares||frame.whiteKing==frame.blackKing)
        throw std::invalid_argument("invalid Ghost-pair public Kings");
    std::uint32_t visibility=0;
    if(frame.visibleCount==1){visibility=1+rank_excluding(frame.visibleGhosts[0],{frame.whiteKing,frame.blackKing});}
    else if(frame.visibleCount==2){
        const std::uint32_t first=rank_excluding(frame.visibleGhosts[0],{frame.whiteKing,frame.blackKing});
        const std::uint32_t second=rank_excluding(frame.visibleGhosts[1],{frame.whiteKing,frame.blackKing});
        visibility=1+(Squares-2)+pair_rank(first,second,Squares-2);
    }else if(frame.visibleCount!=0)throw std::invalid_argument("invalid Ghost-pair visibility class");
    const std::uint32_t blackRank=frame.blackKing-(frame.blackKing>frame.whiteKing?1u:0u);
    return ((static_cast<std::uint32_t>(frame.side)*Squares+frame.whiteKing)*(Squares-1)+blackRank)*
      VisibilityGeometryCount+visibility;
}

[[nodiscard]] PublicFrame decode_geometry(std::uint32_t index){
    if(index>=RawGeometryCount)throw std::out_of_range("Ghost-pair geometry index");
    const std::uint32_t original=index;const std::uint32_t visibility=index%VisibilityGeometryCount;index/=VisibilityGeometryCount;
    const std::uint32_t blackRank=index%(Squares-1);index/=Squares-1;
    PublicFrame frame;frame.whiteKing=static_cast<std::uint8_t>(index%Squares);
    frame.side=static_cast<Color>(index/Squares);frame.blackKing=unrank_excluding(blackRank,{frame.whiteKing});
    if(visibility&&visibility<=Squares-2){frame.visibleCount=1;
        frame.visibleGhosts[0]=unrank_excluding(visibility-1,{frame.whiteKing,frame.blackKing});}
    else if(visibility){frame.visibleCount=2;const auto[first,second]=pair_unrank(
          visibility-1-(Squares-2),Squares-2);
        frame.visibleGhosts[0]=unrank_excluding(first,{frame.whiteKing,frame.blackKing});
        frame.visibleGhosts[1]=unrank_excluding(second,{frame.whiteKing,frame.blackKing});}
    if(encode_geometry(frame)!=original)throw std::runtime_error("Ghost-pair geometry involution residual");return frame;
}

[[nodiscard]] std::pair<std::uint32_t,RectangleTransform>canonical_geometry(const PublicFrame&frame){
    std::uint32_t best=encode_geometry(frame);RectangleTransform transform=RectangleTransform::Identity;
    PairMask seed;seed.set(0);
    for(std::uint8_t raw=1;raw<4;++raw){const auto candidateTransform=static_cast<RectangleTransform>(raw);
        const PairSet mapped=transform_set(frame,seed,candidateTransform);const std::uint32_t candidate=encode_geometry(mapped.frame);
        if(candidate<best){best=candidate;transform=candidateTransform;}}
    return{best,transform};
}

[[nodiscard]] std::uint8_t terminal_flags(const Position&position){
    if(!position.game_over())throw std::runtime_error("terminal flags on live position");
    const std::optional<Color>winner=position.winner();return !winner?0:*winner==Color::White?1:2;
}

[[nodiscard]] std::optional<Move>find_action(const Position&position,const ActionKey&action){
    std::optional<Move>result;for(const Move&move:position.legal_moves())if(action_key(move)==action){
        if(result)throw std::runtime_error("complete Ghost-pair action collision");
        result=move;}
    return result;
}

}  // namespace

namespace {

[[nodiscard]] std::uint64_t checked_add(std::uint64_t first,
                                        std::uint64_t second) {
    if (first > std::numeric_limits<std::uint64_t>::max() - second)
        throw std::overflow_error("Ghost-pair resource estimate overflow");
    return first + second;
}

[[nodiscard]] std::uint64_t checked_mul(std::uint64_t first,
                                        std::uint64_t second) {
    if (first && second > std::numeric_limits<std::uint64_t>::max() / first)
        throw std::overflow_error("Ghost-pair resource estimate overflow");
    return first * second;
}

[[nodiscard]] std::uint64_t physical_memory() {
    const long pages=::sysconf(_SC_PHYS_PAGES),page=::sysconf(_SC_PAGESIZE);
    if(pages<=0||page<=0)throw std::runtime_error("cannot determine physical memory");
    return checked_mul(static_cast<std::uint64_t>(pages),static_cast<std::uint64_t>(page));
}

[[nodiscard]] ResourceEstimate estimate_resources(std::uint64_t geometries,
  std::uint64_t strata,std::uint64_t worlds,std::uint64_t ownerRoots,
  std::uint64_t transitionBytes,const PairRobdd::Limits&bdd,
  const ResourceLimits&limits){
    if(bdd.variables!=MaximumWorlds||!bdd.maxNodes||!bdd.uniqueSlots||
       (bdd.uniqueSlots&(bdd.uniqueSlots-1)))
        throw std::invalid_argument("invalid Ghost-pair BDD resource limits");
    ResourceEstimate result;result.canonicalGeometries=geometries;
    result.correlatedWorlds=worlds;result.transitionBytes=transitionBytes;
    result.rootBytes=checked_add(checked_mul(ownerRoots,4*sizeof(PairRobdd::Id)),
                                 checked_mul(strata,3*sizeof(PairRobdd::Id)));
    // A live exact key contains the full 376-byte correlated PairMask plus its
    // canonical geometry.  512 bytes per concrete realization bounds the
    // std::set node, string allocation, allocator metadata, and terminal key;
    // no probabilistic fingerprint is substituted for this catalog.
    result.rootCatalogBytes=checked_mul(StateCount,512);
    result.bddBytes=PairRobdd::required_bytes(bdd);
    result.compactionBytes=checked_mul(bdd.maxNodes,sizeof(PairRobdd::Id));
    // The retained UFGG1 probe is written while both fixed-capacity BDD arenas
    // and the root mmaps still exist.  Budget its worst-case compact node
    // stream plus every geometry/stratum/reverse-map/root record; unlike the
    // transition database, this is the permanent all-beliefs S3 artifact.
    const std::uint64_t arbitraryBytes=checked_add(sizeof(ArbitraryHeaderDisk),
      checked_add(checked_mul(bdd.maxNodes,sizeof(PairRobdd::NodeRecord)),
      checked_add(checked_mul(geometries,sizeof(ArbitraryGeometryDisk)),
      checked_add(checked_mul(strata,sizeof(PairMask)),
      checked_add(checked_mul(worlds,sizeof(std::uint32_t)),
      checked_add(checked_mul(ownerRoots,sizeof(PairRobdd::Id)),
                  checked_mul(strata,sizeof(PairRobdd::Id))))))));
    result.peakDiskBytes=checked_add(transitionBytes,checked_add(
      checked_mul(result.bddBytes,2),checked_add(result.compactionBytes,
        checked_add(result.rootBytes,checked_add(StateCount,arbitraryBytes)))));
    const std::uint64_t cacheResident=checked_mul(checked_add(
      bdd.applyCacheEntries,bdd.unaryCacheEntries),64);
    result.peakResidentBytes=checked_add(64ULL<<20,checked_add(
      checked_mul(bdd.uniqueSlots,sizeof(PairRobdd::Id)),
      checked_add(result.rootBytes,checked_add(result.rootCatalogBytes,
        checked_add(StateCount,checked_add(cacheResident,checked_add(
          checked_mul(bdd.maxNodes,sizeof(std::uint16_t)),512ULL<<20)))))));
    const bool nodeGate=(!limits.maxBddNodes||
      bdd.maxNodes<=limits.maxBddNodes)&&result.bddBytes<=bdd.budgetBytes;
    result.admitted=nodeGate&&limits.maxDiskBytes&&limits.maxResidentBytes&&
      result.peakDiskBytes<=limits.maxDiskBytes&&result.peakResidentBytes<=limits.maxResidentBytes;
    return result;
}

}  // namespace

ResourceEstimate sampled_resource_preflight(const TransitionCertificate&sample,
  std::uint64_t sampledRawGeometries,const PairRobdd::Limits&bdd,
  const ResourceLimits&limits){
    if(!sampledRawGeometries||sampledRawGeometries>RawGeometryCount||
       sample.rawGeometries!=sampledRawGeometries||!sample.canonicalGeometries)
        throw std::invalid_argument("invalid Ghost-pair sampled certificate");
    const auto scale=[&](std::uint64_t value){return (checked_mul(value,RawGeometryCount)+sampledRawGeometries-1)/sampledRawGeometries;};
    const std::uint64_t geometries=scale(sample.canonicalGeometries),worlds=scale(sample.worlds);
    const std::uint64_t strata=checked_add(geometries,scale(sample.observations));
    const std::uint64_t transitionBytes=checked_add(checked_mul(geometries,
      sizeof(GeometryDisk)+sizeof(std::uint64_t)+sizeof(BlockHeaderDisk)),
      checked_add(checked_mul(strata,sizeof(PairMask)),checked_add(
        checked_mul(scale(sample.worlds),sizeof(std::uint32_t)),checked_add(
          checked_mul(scale(sample.actions),sizeof(ActionDisk)),checked_mul(scale(sample.edges),sizeof(EdgeDisk))))));
    return estimate_resources(geometries,strata,worlds,scale(sample.liveWorlds),transitionBytes,bdd,limits);
}

ResourceEstimate full_domain_preflight(const std::string&prefix,
  const PairRobdd::Limits&bdd,const ResourceLimits&limits,
  const std::string&scratchPrefix){
    const TransitionHeaderDisk header=read_header(prefix);
    if(!header.complete||header.rawBegin||header.rawCount!=RawGeometryCount)
        throw std::runtime_error("Ghost-pair resource gate needs complete transitions");
    verify_transition_storage(prefix,header);(void)read_verified_marker(prefix,header);
    const std::uint64_t bytes=checked_add(file_bytes(prefix+".header"),
      checked_add(file_bytes(prefix+".meta"),checked_add(file_bytes(prefix+".strata"),
        checked_add(file_bytes(prefix+".actual"),checked_add(file_bytes(prefix+".index"),
          file_bytes(prefix+".blocks"))))));
    ResourceEstimate result=estimate_resources(header.geometries,header.strata,
      header.worlds,header.ownerRoots,bytes,bdd,limits);
    const std::size_t slash=scratchPrefix.rfind('/');const std::string directory=
      slash==std::string::npos?".":slash==0?"/":scratchPrefix.substr(0,slash);
    struct statvfs space{};if(::statvfs(directory.c_str(),&space))system_error("cannot query free space",directory);
    const std::uint64_t available=checked_mul(space.f_bavail,space.f_frsize);
    if(available<std::max(limits.minFreeDiskBytes,result.peakDiskBytes)||
       result.peakResidentBytes>physical_memory()*7/10)result.admitted=false;
    if(!result.admitted)throw std::runtime_error("exact Ghost-pair solve rejected by resource preflight");
    return result;
}

namespace {

enum class Wdl : std::uint8_t { Invalid=0,Win=1,Loss=2,Draw=3 };

struct PackedTable {
    std::vector<std::uint8_t> bytes;
    std::size_t plane=0;
    std::uint32_t count=0;
    std::string sha;
    [[nodiscard]] Wdl result(std::uint32_t index)const{
        if(index>=count)throw std::out_of_range("Ghost-pair concrete index");
        return static_cast<Wdl>((bytes.at(plane+index/4)>>(2*(index%4)))&3);
    }
};

[[nodiscard]] std::uint32_t little_u32(const std::uint8_t*bytes){return bytes[0]|
  (std::uint32_t(bytes[1])<<8)|(std::uint32_t(bytes[2])<<16)|(std::uint32_t(bytes[3])<<24);}

[[nodiscard]] std::uint64_t little_u64(const std::uint8_t*bytes){
    return little_u32(bytes)|(std::uint64_t(little_u32(bytes+4))<<32);
}

void validate_packed_planes(const std::vector<std::uint8_t>&bytes,
  std::uint64_t plane,std::uint32_t count,std::uint32_t wdlBytes,
  std::uint32_t dtwBytes,std::uint32_t exceptionCount){
    const std::uint64_t exceptionOffset=checked_add(plane,
      checked_add(wdlBytes,dtwBytes));
    const std::uint64_t expectedExtent=checked_add(exceptionOffset,
      checked_mul(exceptionCount,sizeof(std::uint32_t)+sizeof(std::uint16_t)));
    if(expectedExtent!=bytes.size())
        throw std::runtime_error("Ghost-pair concrete extent mismatch");
    std::uint64_t saturatedDistances=0;
    for(std::uint32_t index=0;index<count;++index){
        const unsigned value=(bytes.at(plane+index/4)>>
                              (2*(index%4)))&3;
        if(value<static_cast<unsigned>(Wdl::Win)||
           value>static_cast<unsigned>(Wdl::Draw))
            throw std::runtime_error("Ghost-pair concrete WDL value is invalid");
        saturatedDistances+=bytes.at(plane+wdlBytes+index)==255;
    }
    if(saturatedDistances!=exceptionCount)
        throw std::runtime_error("Ghost-pair concrete DTW exception residual");
    std::uint32_t previous=0;
    for(std::uint32_t item=0;item<exceptionCount;++item){
        const std::uint8_t*record=bytes.data()+exceptionOffset+
          std::uint64_t(item)*6;const std::uint32_t index=little_u32(record);
        const std::uint16_t distance=record[4]|(std::uint16_t(record[5])<<8);
        if(index>=count||(item&&index<=previous)||distance<255||
           bytes.at(plane+wdlBytes+index)!=255)
            throw std::runtime_error("Ghost-pair concrete exception index residual");
        previous=index;
    }
}

[[nodiscard]] PackedTable load_concrete(const std::string&path){
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("cannot open Ghost-pair concrete table");
    input.seekg(0,std::ios::end);const std::streamoff end=input.tellg();if(end<56)throw std::runtime_error("Ghost-pair table truncated");
    PackedTable result;result.bytes.resize(static_cast<std::size_t>(end));input.seekg(0);
    input.read(reinterpret_cast<char*>(result.bytes.data()),result.bytes.size());if(!input||std::memcmp(result.bytes.data(),"UFTB1\0\0\0",8))
        throw std::runtime_error("invalid Ghost-pair table magic");
    const std::uint32_t version=little_u32(result.bytes.data()+8);result.count=little_u32(result.bytes.data()+16);
    const std::uint32_t wdlBytes=little_u32(result.bytes.data()+28);
    const std::uint32_t dtwBytes=little_u32(result.bytes.data()+32);
    const std::uint32_t exceptionCount=little_u32(result.bytes.data()+36);
    if((version!=5&&version!=6)||little_u32(result.bytes.data()+12)!=static_cast<std::uint32_t>(PieceType::Ghost)||
       result.count!=StateCount||little_u32(result.bytes.data()+24)!=4||wdlBytes!=(StateCount+3)/4||
       dtwBytes!=StateCount||
       little_u32(result.bytes.data()+40)!=static_cast<std::uint32_t>(PieceType::Ghost)||
       little_u32(result.bytes.data()+44)!=static_cast<std::uint32_t>(Color::White)||
       (version==6&&(result.bytes.size()<56||little_u64(result.bytes.data()+48)<=
          std::numeric_limits<std::uint32_t>::max())))
        throw std::runtime_error("Ghost-pair concrete codec/material mismatch");
    result.plane=40+(version>=5?8:0)+(version>=6?8:0)+(version>=7?8:0);
    validate_packed_planes(result.bytes,result.plane,StateCount,wdlBytes,
                           dtwBytes,exceptionCount);
    result.sha=sha256_file(path);return result;
}

[[nodiscard]] bool wdl_forces(Wdl result,Color side,Color target){return
  (result==Wdl::Win&&side==target)||(result==Wdl::Loss&&side!=target);}

class LowerGhostSidecar {
  public:
    struct Node{std::uint8_t variable=Squares;std::uint32_t low=0,high=0;};
    struct Mask{std::uint64_t low=0;std::uint16_t high=0;};
    struct Geometry{
        std::uint8_t side=0,ownerKing=0,observerKing=0,visible=0;
        Mask live,terminal,terminalOwner,terminalObserver;
        std::array<std::uint32_t,Squares>actualStratum{},ownerRoot{};
        std::array<std::uint8_t,Squares>visibleOwner{},visibleObserver{};
    };
    struct Stratum{std::uint32_t geometry=NoIndex;Mask live;std::uint32_t root=0;};

    explicit LowerGhostSidecar(const SolveOptions&options){
        for(const auto&[hash,label]:std::array<std::pair<std::string,const char*>,4>{{
          {options.lowerGhostSourceSha256,"lower Ghost source SHA-256"},
          {options.lowerGhostModelSha256,"lower Ghost model SHA-256"},
          {options.lowerGhostObservationSha256,"lower Ghost observation SHA-256"},
          {options.lowerGhostSidecarSha256,"lower Ghost sidecar SHA-256"}}})require_hash(hash,label);
        if(sha256_file(options.lowerGhostSidecar)!=options.lowerGhostSidecarSha256)
            throw std::runtime_error("lower Ghost sidecar full SHA mismatch");
        GhostInformationProbe independent(options.lowerGhostSidecar,
          options.lowerGhostSourceSha256,options.lowerGhostModelSha256,
          options.lowerGhostObservationSha256);
        std::ifstream input(options.lowerGhostSidecar,std::ios::binary);
        auto u8=[&](){const int value=input.get();if(value<0)throw std::runtime_error("truncated UFGM");return std::uint8_t(value);};
        auto u32=[&](){std::array<std::uint8_t,4>bytes{};input.read(reinterpret_cast<char*>(bytes.data()),4);
            if(!input)throw std::runtime_error("truncated UFGM");return little_u32(bytes.data());};
        auto u64=[&](){const std::uint64_t low=u32();return low|(std::uint64_t(u32())<<32);};
        auto text=[&](std::size_t size){std::string value(size,'\0');input.read(value.data(),size);if(!input)throw std::runtime_error("truncated UFGM");return value;};
        auto mask=[&](){Mask value;value.low=u64();value.high=std::uint16_t(u8())|(std::uint16_t(u8())<<8);return value;};
        const std::string magic=text(8);const std::uint32_t version=u32(),header=u32(),piece=u32(),owner=u32(),files=u32(),ranks=u32(),squares=u32();
        const std::uint32_t concreteCount=u32(),substates=u32(),geometryCount=u32(),stratumCount=u32(),nodeCount=u32(),nodeBytes=u32();
        const std::uint32_t geometryBytes=u32(),stratumBytes=u32();(void)u32();const std::uint64_t nodeOffset=u64(),geometryOffset=u64(),stratumOffset=u64();
        const std::string source=text(64),model=text(64),observation=text(64),semantics=text(32);
        if(magic!=std::string("UFGM1\0\0\0",8)||version!=1||header!=320||piece!=static_cast<std::uint32_t>(PieceType::Ghost)||
           owner!=static_cast<std::uint32_t>(Color::White)||files!=Position::BoardFiles||ranks!=Position::BoardRanks||squares!=Squares||
           concreteCount!=LowerGhostStateCount||substates!=2||nodeBytes!=9||geometryBytes!=844||stratumBytes!=18||nodeCount<2||
           source!=options.lowerGhostSourceSha256||model!=options.lowerGhostModelSha256||observation!=options.lowerGhostObservationSha256||
           semantics.c_str()!=std::string("history-mask-public-view-v2")||nodeOffset!=320||
           geometryOffset!=nodeOffset+std::uint64_t(nodeCount)*9||stratumOffset!=geometryOffset+std::uint64_t(geometryCount)*844||
           independent.node_count()!=nodeCount||independent.geometry_count()!=geometryCount||independent.stratum_count()!=stratumCount)
            throw std::runtime_error("lower Ghost UFGM binding residual");
        input.seekg(nodeOffset);nodes_.resize(nodeCount);std::set<std::tuple<std::uint8_t,std::uint32_t,std::uint32_t>>unique;
        for(std::uint32_t id=0;id<nodeCount;++id){Node&node=nodes_[id];node.variable=u8();node.low=u32();node.high=u32();
            if(id<2){if(node.variable!=Squares||node.low!=id||node.high!=id)throw std::runtime_error("bad UFGM terminals");}
            else if(node.variable>=Squares||node.low>=id||node.high>=id||node.low==node.high||!unique.emplace(node.variable,node.low,node.high).second)
                throw std::runtime_error("bad UFGM node tuple");
            if(id>=2){const auto variable=[&](std::uint32_t child){return child<2?Squares:nodes_[child].variable;};
                if(variable(node.low)<=node.variable||variable(node.high)<=node.variable)throw std::runtime_error("unordered UFGM node tuple");}}
        input.seekg(geometryOffset);geometries_.resize(geometryCount);
        for(std::uint32_t id=0;id<geometryCount;++id){Geometry&g=geometries_[id];g.side=u8();g.ownerKing=u8();g.observerKing=u8();g.visible=u8();
            g.live=mask();g.terminal=mask();g.terminalOwner=mask();g.terminalObserver=mask();for(auto&value:g.actualStratum)value=u32();
            for(auto&value:g.ownerRoot)value=u32();input.read(reinterpret_cast<char*>(g.visibleOwner.data()),Squares);
            input.read(reinterpret_cast<char*>(g.visibleObserver.data()),Squares);if(!input||g.side>1||g.ownerKing>=Squares||g.observerKing>=Squares||
               g.ownerKing==g.observerKing||g.visible>1)throw std::runtime_error("invalid UFGM geometry");
            for(unsigned actual=0;actual<Squares;++actual){if(g.ownerRoot[actual]>=nodeCount||g.visibleOwner[actual]>1||g.visibleObserver[actual]>1)
                    throw std::runtime_error("invalid UFGM force field");
                const bool live=test(g.live,actual),terminal=test(g.terminal,actual);
                if(live&&terminal)throw std::runtime_error("UFGM live/terminal overlap");if((!live||g.visible)&&g.actualStratum[actual]!=NoIndex)
                    throw std::runtime_error("invalid UFGM reverse map");}
            const std::uint32_t code=geometry_code(g.side,g.ownerKing,g.observerKing,g.visible);
            if(!geometryIndex_.emplace(code,id).second)throw std::runtime_error("duplicate UFGM geometry");}
        input.seekg(stratumOffset);strata_.resize(stratumCount);for(Stratum&s:strata_){s.geometry=u32();s.live=mask();s.root=u32();
            if(s.geometry>=geometryCount||s.root>=nodeCount)throw std::runtime_error("invalid UFGM stratum");}
        if(input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("UFGM trailing bytes");
        for(std::uint32_t gid=0;gid<geometries_.size();++gid){const Geometry&g=geometries_[gid];for(unsigned actual=0;actual<Squares;++actual)
            if(test(g.live,actual)&&!g.visible){const std::uint32_t sid=g.actualStratum[actual];if(sid==NoIndex||sid>=strata_.size()||
               strata_[sid].geometry!=gid||!test(strata_[sid].live,actual))throw std::runtime_error("UFGM reverse-map residual");}}
    }
    [[nodiscard]] const Node&node(std::uint32_t id)const{return nodes_.at(id);}
    [[nodiscard]] std::uint32_t geometry(const LowerGhostState&state)const{const auto it=geometryIndex_.find(geometry_code(
      static_cast<std::uint8_t>(state.side),state.ownerKing,state.observerKing,state.visible));
      if(it==geometryIndex_.end())throw std::runtime_error("missing lower Ghost geometry");return it->second;}
    [[nodiscard]] const Geometry&geometry(std::uint32_t id)const{return geometries_.at(id);}
    [[nodiscard]] const Stratum&stratum(std::uint32_t id)const{return strata_.at(id);}
    [[nodiscard]] std::size_t node_count()const{return nodes_.size();}
    [[nodiscard]] static bool test(const Mask&mask,unsigned square){return square<64?(mask.low>>square)&1u:(mask.high>>(square-64))&1u;}
  private:
    [[nodiscard]] static std::uint32_t geometry_code(std::uint8_t side,std::uint8_t owner,std::uint8_t observer,bool visible){
        return side|(std::uint32_t(owner)<<1)|(std::uint32_t(observer)<<8)|(std::uint32_t(visible)<<15);}
    std::vector<Node>nodes_;std::vector<Geometry>geometries_;std::vector<Stratum>strata_;
    std::unordered_map<std::uint32_t,std::uint32_t>geometryIndex_;
};

class TransitionDatabase {
  public:
    explicit TransitionDatabase(const std::string&prefix):header_(read_header(prefix)){
        if(!header_.complete||header_.rawBegin||header_.rawCount!=RawGeometryCount)throw std::runtime_error("solver needs complete Ghost-pair transitions");
        meta_.open(prefix+".meta",header_.geometries,false,true);strata_.open(prefix+".strata",header_.strata,false,true);
        actual_.open(prefix+".actual",header_.actuals,false,true);index_.open(prefix+".index",header_.geometries+1,false,true);
        blocks_.open(prefix+".blocks",std::ios::binary);if(!blocks_)throw std::runtime_error("cannot open Ghost-pair transition blocks");
    }
    [[nodiscard]] std::uint64_t geometries()const{return header_.geometries;}
    [[nodiscard]] std::uint64_t strata()const{return header_.strata;}
    [[nodiscard]] std::uint64_t actuals()const{return header_.actuals;}
    [[nodiscard]] std::uint64_t owner_roots()const{return header_.ownerRoots;}
    [[nodiscard]] const GeometryDisk&meta(std::uint64_t id)const{return meta_[id];}
    [[nodiscard]] const PairMask&stratum(std::uint64_t id)const{return strata_[id];}
    [[nodiscard]] std::uint32_t actual(std::uint64_t id)const{return actual_[id];}
    [[nodiscard]] std::uint32_t actual_stratum(const GeometryDisk&meta,unsigned actual)const{
        if(actual>=meta.variableCount)throw std::out_of_range("Ghost-pair actual variable");return actual_[meta.actualBase+actual];}
    [[nodiscard]] std::uint32_t ordinal(std::uint32_t raw)const{std::uint64_t low=0,high=header_.geometries;
        while(low<high){const std::uint64_t middle=(low+high)/2;if(meta_[middle].raw<raw)low=middle+1;else high=middle;}
        if(low>=header_.geometries||meta_[low].raw!=raw)throw std::runtime_error("same-class Ghost-pair child absent");return static_cast<std::uint32_t>(low);}
    struct Block{BlockHeaderDisk header;std::vector<std::uint32_t>offsets;std::vector<ActionDisk>actions;std::vector<EdgeDisk>edges;};
    [[nodiscard]] Block block(std::uint32_t id){const std::uint64_t begin=index_[id],end=index_[id+1];blocks_.clear();blocks_.seekg(begin);
        Block result;result.header=read_value<BlockHeaderDisk>(blocks_);result.offsets.resize(result.header.variableCount+1);
        result.actions.resize(result.header.actionCount);result.edges.resize(result.header.edgeCount);
        blocks_.read(reinterpret_cast<char*>(result.offsets.data()),result.offsets.size()*sizeof(std::uint32_t));
        blocks_.read(reinterpret_cast<char*>(result.actions.data()),result.actions.size()*sizeof(ActionDisk));
        blocks_.read(reinterpret_cast<char*>(result.edges.data()),result.edges.size()*sizeof(EdgeDisk));
        if(!blocks_||result.offsets.empty()||result.offsets.front()||result.offsets.back()!=result.edges.size()||
           static_cast<std::uint64_t>(blocks_.tellg())!=end)
            throw std::runtime_error("malformed Ghost-pair transition block");
        return result;}
    [[nodiscard]] const TransitionHeaderDisk&header()const{return header_;}
  private:
    TransitionHeaderDisk header_{};MmapFile<GeometryDisk>meta_;MmapFile<PairMask>strata_;MmapFile<std::uint32_t>actual_;
    MmapFile<std::uint64_t>index_;std::ifstream blocks_;
};

[[nodiscard]] std::uint64_t owner_root_index(const GeometryDisk&meta,unsigned actual){
    if(actual>=meta.variableCount||!meta.live.test(actual))throw std::runtime_error("invalid compact Ghost-pair owner root");
    std::uint64_t rank=0;for(unsigned word=0;word<actual/64;++word)rank+=static_cast<unsigned>(__builtin_popcountll(meta.live.words[word]));
    const unsigned bit=actual%64;if(bit)rank+=static_cast<unsigned>(__builtin_popcountll(meta.live.words[actual/64]&((std::uint64_t{1}<<bit)-1)));
    if(rank>=meta.liveCount)throw std::runtime_error("Ghost-pair owner rank residual");return meta.ownerBase+rank;
}

struct RuntimeRelation {
    bool initialized=false;
    CompiledChildDomain domain=CompiledChildDomain::ExactTerminal;
    std::uint32_t childGeometry=NoIndex,childStratum=NoIndex;
    bool childTerminal=false;
    PairMask possibleSources,badWhiteSources,badBlackSources;
    std::map<std::uint16_t,PairMask>pairImage;
    std::map<std::uint8_t,PairMask>lowerImage;
    std::map<std::uint32_t,PairMask>lowerChildren;
};
struct RuntimeActionObservation{std::uint32_t relation=0;PairMask sources;};
struct RuntimeAction{PairMask legalSources;std::vector<RuntimeActionObservation>observations;};
struct RuntimeBlock{std::vector<std::vector<CompiledEdge>>edges;std::vector<RuntimeRelation>relations;std::vector<RuntimeAction>actions;};

void add_lower_image(RuntimeRelation&relation,std::uint32_t childConcrete,
                     unsigned source){
    const LowerGhostState child=decode_lower_ghost(childConcrete);
    relation.lowerImage[child.ghost].set(source);
    relation.lowerChildren[childConcrete].set(source);
}

[[nodiscard]] RuntimeBlock runtime_block(std::uint32_t geometry,TransitionDatabase&database,
                                         const LowerGhostSidecar&lower){
    const TransitionDatabase::Block stored=database.block(geometry);RuntimeBlock result;
    result.edges.resize(stored.header.variableCount);result.actions.resize(stored.actions.size());std::uint32_t relationCount=0;
    for(const EdgeDisk&item:stored.edges)relationCount=std::max(relationCount,item.edge.relation+1);result.relations.resize(relationCount);
    std::vector<std::map<std::uint32_t,PairMask>>actionObservations(result.actions.size());
    for(unsigned source=0;source<stored.header.variableCount;++source){std::vector<bool>seen(result.actions.size());
        for(std::uint32_t cursor=stored.offsets[source];cursor<stored.offsets[source+1];++cursor){const CompiledEdge&edge=stored.edges[cursor].edge;
            if(edge.action>=result.actions.size()||seen[edge.action])throw std::runtime_error("duplicate/bad Ghost-pair action edge");seen[edge.action]=true;
            result.edges[source].push_back(edge);result.actions[edge.action].legalSources.set(source);actionObservations[edge.action][edge.relation].set(source);
            RuntimeRelation&relation=result.relations.at(edge.relation);relation.possibleSources.set(source);
            std::uint32_t childGeometry=NoIndex,childStratum=NoIndex;bool terminal=false;
            if(edge.domain==CompiledChildDomain::SameClass){childGeometry=database.ordinal(edge.childGeometry);const GeometryDisk&child=database.meta(childGeometry);
                terminal=child.terminal.test(edge.childActual);if(!terminal){const std::uint32_t local=database.actual_stratum(child,edge.childActual);
                    if(local==NoIndex||local>=child.stratumCount)throw std::runtime_error("same-class pair child lacks decision cell");childStratum=child.stratumBase+local;}
                relation.pairImage[edge.childActual].set(source);if(terminal){if(!child.terminalWhite.test(edge.childActual))relation.badWhiteSources.set(source);
                    if(!child.terminalBlack.test(edge.childActual))relation.badBlackSources.set(source);}}
            else if(edge.domain==CompiledChildDomain::LowerGhost){const LowerGhostState child=decode_lower_ghost(edge.childConcrete);
                childGeometry=lower.geometry(child);const auto&g=lower.geometry(childGeometry);terminal=LowerGhostSidecar::test(g.terminal,child.ghost);
                if(!terminal&&!child.visible){childStratum=g.actualStratum[child.ghost];if(childStratum==NoIndex)throw std::runtime_error("lower Ghost child lacks stratum");}
                add_lower_image(relation,edge.childConcrete,source);
                if(terminal){if(!LowerGhostSidecar::test(g.terminalOwner,child.ghost))relation.badWhiteSources.set(source);
                    if(!LowerGhostSidecar::test(g.terminalObserver,child.ghost))relation.badBlackSources.set(source);}}
            else{if(!(edge.terminalForces&1))relation.badWhiteSources.set(source);if(!(edge.terminalForces&2))relation.badBlackSources.set(source);}
            if(!relation.initialized){relation.initialized=true;relation.domain=edge.domain;relation.childGeometry=childGeometry;
                relation.childStratum=childStratum;relation.childTerminal=terminal;}
            else if(relation.domain!=edge.domain||relation.childGeometry!=childGeometry||relation.childStratum!=childStratum||relation.childTerminal!=terminal)
                throw std::runtime_error("one Ghost-pair observation mixes epistemic domains");}
    }
    for(std::uint32_t action=0;action<result.actions.size();++action)for(const auto&[relation,sources]:actionObservations[action])
        result.actions[action].observations.push_back({relation,sources});
    for(const RuntimeRelation&relation:result.relations){if(!relation.initialized)throw std::runtime_error("sparse Ghost-pair relation IDs");
        if(relation.domain==CompiledChildDomain::LowerGhost&&relation.lowerChildren.empty())throw std::runtime_error("empty lower Ghost image");}
    return result;
}

}  // namespace

namespace {

[[nodiscard]] PairMask mask_and(const PairMask&first,const PairMask&second){PairMask result;
    for(unsigned word=0;word<MaskWords;++word)result.words[word]=first.words[word]&second.words[word];return result;}

[[nodiscard]] PairRobdd::Id subset_within(PairRobdd&bdd,
  const PairMask&allowed,unsigned variableCount){
    PairRobdd::Id result=PairRobdd::True;
    for(unsigned variable=0;variable<variableCount;++variable)
        if(!allowed.test(variable))result=bdd.logical_and(result,
          bdd.logical_not(bdd.variable(variable)));
    return result;
}

class ExactKernel {
  public:
    ExactKernel(TransitionDatabase&database,const LowerGhostSidecar&lower,
      PairRobdd&bdd,MmapFile<PairRobdd::Id>&owner,
      MmapFile<PairRobdd::Id>&observer,MmapFile<PairRobdd::Id>&domains,
      SolveCertificate*certificate)
      :database_(database),lower_(lower),bdd_(bdd),owner_(owner),
       observer_(observer),domains_(domains),certificate_(certificate){}

    [[nodiscard]] PairRobdd::Id relation_compose(
      const RuntimeRelation&relation,PairRobdd::Id child,
      std::uint64_t relationId){
        std::vector<PairRobdd::Id>image(MaximumWorlds,PairRobdd::False);
        for(const auto&[target,sources]:relation.pairImage)
            image.at(target)=bdd_.any(sources);
        return bdd_.compose(child,image,relationId);
    }

    [[nodiscard]] PairRobdd::Id compose_lower_node(
      const RuntimeRelation&relation,std::uint32_t root,
      std::unordered_map<std::uint32_t,PairRobdd::Id>&memo){
        if(root<=1)return root;
        if(const auto found=memo.find(root);found!=memo.end())return found->second;
        const LowerGhostSidecar::Node node=lower_.node(root);
        const auto image=relation.lowerImage.find(node.variable);
        const PairRobdd::Id condition=image==relation.lowerImage.end()?
          PairRobdd::False:bdd_.any(image->second);
        const PairRobdd::Id value=bdd_.ite(condition,
          compose_lower_node(relation,node.high,memo),
          compose_lower_node(relation,node.low,memo));
        memo.emplace(root,value);return value;
    }

    [[nodiscard]] PairRobdd::Id lower_formula(
      const RuntimeRelation&relation,Color target,
      const CompiledEdge*actual){
        if(certificate_)++certificate_->lowerGhostMaskProbes;
        if(relation.childTerminal){const PairMask&bad=target==Color::White?
            relation.badWhiteSources:relation.badBlackSources;
            if(actual)return bdd_.logical_not(bdd_.any(mask_and(
              relation.lowerChildren.at(actual->childConcrete),bad)));
            return bdd_.logical_not(bdd_.any(bad));}
        const LowerGhostState first=decode_lower_ghost(
          relation.lowerChildren.begin()->first);
        const std::uint32_t geometry=lower_.geometry(first);
        const auto&g=lower_.geometry(geometry);std::uint32_t root=0;
        if(actual){const LowerGhostState child=decode_lower_ghost(actual->childConcrete);
            if(child.visible)return target==Color::White?
              g.visibleOwner[child.ghost]:g.visibleObserver[child.ghost];
            root=target==Color::White?g.ownerRoot[child.ghost]:
              lower_.stratum(g.actualStratum[child.ghost]).root;}
        else{if(first.visible)return target==Color::White?
              g.visibleOwner[first.ghost]:g.visibleObserver[first.ghost];
            root=target==Color::White?g.ownerRoot[first.ghost]:
              lower_.stratum(relation.childStratum).root;}
        std::unordered_map<std::uint32_t,PairRobdd::Id>memo;
        return compose_lower_node(relation,root,memo);
    }

    [[nodiscard]] PairRobdd::Id successor(const RuntimeRelation&relation,
      const CompiledEdge*edge,Color target,std::uint64_t relationId){
        if(relation.domain==CompiledChildDomain::SameClass){
            if(relation.childTerminal){const PairMask&bad=target==Color::White?
                relation.badWhiteSources:relation.badBlackSources;
                if(edge)return bdd_.logical_not(bdd_.any(mask_and(
                  relation.pairImage.at(edge->childActual),bad)));
                return bdd_.logical_not(bdd_.any(bad));}
            const GeometryDisk&child=database_.meta(relation.childGeometry);
            const PairRobdd::Id root=edge?owner_[owner_root_index(child,
              edge->childActual)]:observer_[relation.childStratum];
            return relation_compose(relation,root,relationId);
        }
        if(relation.domain==CompiledChildDomain::LowerGhost)
            return lower_formula(relation,target,edge);
        if(edge)return (edge->terminalForces&(target==Color::White?1:2))?
          PairRobdd::True:PairRobdd::False;
        const PairMask&bad=target==Color::White?relation.badWhiteSources:
          relation.badBlackSources;
        return bdd_.logical_not(bdd_.any(bad));
    }

    void sweep(MmapFile<PairRobdd::Id>&ownerOut,
               MmapFile<PairRobdd::Id>&observerOut){
        ownerOut.fill(PairRobdd::False);observerOut.fill(PairRobdd::False);
        for(std::uint32_t gid=0;gid<database_.geometries();++gid){
            const GeometryDisk&meta=database_.meta(gid);
            const Color mover=decode_geometry(meta.raw).side;
            const RuntimeBlock block=runtime_block(gid,database_,lower_);
            for(unsigned actual=0;actual<meta.variableCount;++actual){
                if(!meta.live.test(actual))continue;
                const std::uint32_t local=database_.actual_stratum(meta,actual);
                if(local==NoIndex||local>=meta.stratumCount)
                    throw std::runtime_error("live Ghost-pair world has no decision stratum");
                const std::uint32_t stratum=meta.stratumBase+local;
                PairRobdd::Id value=mover==Color::White?
                  PairRobdd::False:PairRobdd::True;
                if(mover==Color::White){
                    for(const CompiledEdge&edge:block.edges[actual])
                        value=bdd_.logical_or(value,successor(
                          block.relations[edge.relation],&edge,Color::White,
                          (std::uint64_t(gid)<<32)|edge.relation));
                }else{
                    for(std::uint32_t action=0;action<block.actions.size();++action){
                        const PairRobdd::Id common=bdd_.logical_and(
                          domains_[stratum],subset_within(bdd_,
                            block.actions[action].legalSources,
                            meta.variableCount));
                        const auto found=std::find_if(block.edges[actual].begin(),
                          block.edges[actual].end(),[&](const CompiledEdge&edge){
                              return edge.action==action;});
                        const PairRobdd::Id child=found==block.edges[actual].end()?
                          PairRobdd::False:successor(block.relations[found->relation],
                            &*found,Color::White,(std::uint64_t(gid)<<32)|
                            found->relation);
                        value=bdd_.logical_and(value,bdd_.logical_or(
                          bdd_.logical_not(common),child));
                    }
                }
                ownerOut[owner_root_index(meta,actual)]=bdd_.logical_and(
                  domains_[stratum],bdd_.logical_and(bdd_.variable(actual),value));
            }
            for(std::uint32_t local=0;local<meta.stratumCount;++local){
                const std::uint32_t stratum=meta.stratumBase+local;
                const PairMask&worlds=database_.stratum(stratum);
                PairRobdd::Id value=mover==Color::White?
                  PairRobdd::True:PairRobdd::False;
                if(mover==Color::White){
                    // Owner actions are private.  Relations are globally
                    // keyed only by Black's resulting observation, so every
                    // source/action policy image in that observation is
                    // composed as one correlated successor belief.
                    for(unsigned actual=0;actual<meta.variableCount;++actual)
                        if(worlds.test(actual))
                            for(const CompiledEdge&edge:block.edges[actual]){
                                const PairRobdd::Id child=successor(
                                  block.relations[edge.relation],nullptr,
                                  Color::Black,(std::uint64_t(gid)<<32)|
                                  edge.relation);
                                value=bdd_.logical_and(value,bdd_.logical_or(
                                  bdd_.logical_not(bdd_.variable(actual)),child));
                            }
                }else{
                    for(std::uint32_t action=0;action<block.actions.size();++action){
                        PairRobdd::Id gate=bdd_.logical_and(domains_[stratum],
                          subset_within(bdd_,block.actions[action].legalSources,
                                         meta.variableCount));
                        for(const RuntimeActionObservation&observation:
                            block.actions[action].observations){
                            const PairRobdd::Id possible=bdd_.any(mask_and(
                              observation.sources,worlds));
                            const PairRobdd::Id child=successor(
                              block.relations[observation.relation],nullptr,
                              Color::Black,(std::uint64_t(gid)<<32)|
                              observation.relation);
                            gate=bdd_.logical_and(gate,bdd_.logical_or(
                              bdd_.logical_not(possible),child));
                        }
                        value=bdd_.logical_or(value,gate);
                    }
                }
                observerOut[stratum]=bdd_.logical_and(domains_[stratum],value);
            }
        }
    }
  private:
    TransitionDatabase&database_;const LowerGhostSidecar&lower_;PairRobdd&bdd_;
    MmapFile<PairRobdd::Id>&owner_;MmapFile<PairRobdd::Id>&observer_;
    MmapFile<PairRobdd::Id>&domains_;SolveCertificate*certificate_;
};

[[nodiscard]] bool same_arrays(const MmapFile<PairRobdd::Id>&first,
                               const MmapFile<PairRobdd::Id>&second){
    if(first.size()!=second.size())return false;
    for(std::uint64_t index=0;index<first.size();++index)
        if(first[index]!=second[index])return false;
    return true;
}

[[nodiscard]] PairMask fresh_root(const PublicFrame&frame,
                                  const PairWorld&actual){
    const std::vector<PairWorld>admitted=admitted_fresh_worlds(frame);
    if(frame.side==Color::Black){const std::vector<DecisionBucket>buckets=
        decision_partition(frame,admitted);const unsigned variable=
        pair_variable(frame,actual);for(const DecisionBucket&bucket:buckets)
            if(bucket.worlds.test(variable))return bucket.worlds;
        return {};}
    PairMask result;for(const PairWorld&world:admitted)
        result.set(pair_variable(frame,world));
    return result;
}

[[nodiscard]] std::string canonical_terminal_view(const PublicFrame&frame,
                                                   const PairWorld&world){
    std::optional<std::string>best;for(std::uint8_t raw=0;raw<4;++raw){
        const FramedWorld transformed=transform_world(frame,world,
          static_cast<RectangleTransform>(raw));const std::string key=view_key(
          make_position(transformed.frame,transformed.world),{Color::Black,false});
        if(!best||key<*best)best=key;}return *best;
}

void copy_digest(std::array<char,64>&destination,const std::string&source,
                 const char*label){
    require_hash(source,label);std::copy(source.begin(),source.end(),
                                        destination.begin());
}

[[nodiscard]] std::array<char,64> arbitrary_semantics(){
    std::array<char,64>result{};constexpr char value[]=
      "correlated-unordered-pair-public-view-v1";
    static_assert(sizeof(value)<=result.size());
    std::copy(std::begin(value),std::end(value),result.begin());return result;
}

template<typename Value,typename Getter>
void write_sequence(std::ofstream&output,std::uint64_t count,Getter getter,
                    const char*label){
    constexpr std::size_t Chunk=1u<<16;std::vector<Value>buffer;
    buffer.reserve(Chunk);std::uint64_t cursor=0;
    while(cursor<count){const std::size_t take=static_cast<std::size_t>(
        std::min<std::uint64_t>(count-cursor,Chunk));buffer.clear();
        for(std::size_t offset=0;offset<take;++offset)
            buffer.push_back(getter(cursor+offset));
        output.write(reinterpret_cast<const char*>(buffer.data()),
                     static_cast<std::streamsize>(take*sizeof(Value)));
        if(!output)throw std::runtime_error(std::string("cannot write ")+label);
        cursor+=take;}
}

[[nodiscard]] ArbitrarySidecarCertificate write_arbitrary_sidecar(
  const std::string&path,const SolveOptions&options,
  const TransitionHeaderDisk&transition,TransitionDatabase&database,
  PairRobdd&bdd,MmapFile<PairRobdd::Id>&owner,
  MmapFile<PairRobdd::Id>&observer){
    if(path.empty())throw std::invalid_argument(
      "exact Ghost-pair solve requires outputArbitrarySidecar");
    if(owner.size()!=database.owner_roots()||observer.size()!=database.strata())
        throw std::runtime_error("Ghost-pair all-beliefs root extent residual");
    owner.flush();observer.flush();
    ArbitraryHeaderDisk header;header.nodes=bdd.node_count();
    header.geometries=database.geometries();header.strata=database.strata();
    header.actuals=database.actuals();header.ownerRoots=database.owner_roots();
    header.nodeOffset=sizeof(header);
    header.geometryOffset=checked_add(header.nodeOffset,checked_mul(
      header.nodes,sizeof(PairRobdd::NodeRecord)));
    header.stratumOffset=checked_add(header.geometryOffset,checked_mul(
      header.geometries,sizeof(ArbitraryGeometryDisk)));
    header.actualOffset=checked_add(header.stratumOffset,checked_mul(
      header.strata,sizeof(PairMask)));
    header.ownerOffset=checked_add(header.actualOffset,checked_mul(
      header.actuals,sizeof(std::uint32_t)));
    header.observerOffset=checked_add(header.ownerOffset,checked_mul(
      header.ownerRoots,sizeof(PairRobdd::Id)));
    const std::uint64_t extent=checked_add(header.observerOffset,checked_mul(
      header.strata,sizeof(PairRobdd::Id)));
    header.payloadBytes=extent-sizeof(header);header.semantics=arbitrary_semantics();
    copy_digest(header.sourceSha,options.sourceSha256,"source SHA-256");
    copy_digest(header.modelSha,options.modelSha256,"model SHA-256");
    copy_digest(header.observationSha,options.observationSha256,
                "observation SHA-256");
    copy_digest(header.transitionPayloadSha,
      std::string(transition.payloadSha.data(),64),"transition payload SHA-256");
    copy_digest(header.transitionHeaderSha,
      sha256_file(options.transitionPrefix+".header"),
      "transition header SHA-256");
    copy_digest(header.transitionMarkerSha,
      sha256_file(options.transitionPrefix+".verified"),
      "transition marker SHA-256");
    copy_digest(header.lowerSidecarSha,options.lowerGhostSidecarSha256,
                "lower Ghost sidecar SHA-256");
    copy_digest(header.lowerSourceSha,options.lowerGhostSourceSha256,
                "lower Ghost source SHA-256");
    copy_digest(header.lowerModelSha,options.lowerGhostModelSha256,
                "lower Ghost model SHA-256");
    copy_digest(header.lowerObservationSha,
      options.lowerGhostObservationSha256,"lower Ghost observation SHA-256");
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    write_value(output,header);
    write_sequence<PairRobdd::NodeRecord>(output,header.nodes,
      [&](std::uint64_t id){return bdd.node_record(
        static_cast<PairRobdd::Id>(id));},"Ghost-pair arbitrary nodes");
    write_sequence<ArbitraryGeometryDisk>(output,header.geometries,
      [&](std::uint64_t id){const GeometryDisk&source=database.meta(id);
        return ArbitraryGeometryDisk{source.raw,source.variableCount,
          source.ownerBase,source.actualBase,source.stratumBase,
          source.stratumCount,source.liveCount};},
      "Ghost-pair arbitrary geometries");
    write_sequence<PairMask>(output,header.strata,
      [&](std::uint64_t id){return database.stratum(id);},
      "Ghost-pair arbitrary strata");
    write_sequence<std::uint32_t>(output,header.actuals,
      [&](std::uint64_t id){return database.actual(id);},
      "Ghost-pair arbitrary reverse map");
    write_sequence<PairRobdd::Id>(output,header.ownerRoots,
      [&](std::uint64_t id){return owner[id];},
      "Ghost-pair arbitrary owner roots");
    write_sequence<PairRobdd::Id>(output,header.strata,
      [&](std::uint64_t id){return observer[id];},
      "Ghost-pair arbitrary observer roots");
    output.close();if(!output)throw std::runtime_error(
      "cannot close Ghost-pair arbitrary sidecar");
    if(file_bytes(path)!=extent)throw std::runtime_error(
      "Ghost-pair arbitrary sidecar extent residual");
    const std::string payload=sha256_range(path,sizeof(header),
                                           header.payloadBytes);
    std::copy(payload.begin(),payload.end(),header.payloadSha.begin());
    std::fstream rewrite(path,std::ios::binary|std::ios::in|std::ios::out);
    rewrite.write(reinterpret_cast<const char*>(&header),sizeof(header));
    rewrite.close();if(!rewrite)throw std::runtime_error(
      "cannot finalize Ghost-pair arbitrary sidecar");
    SolveOptions standalone=options;standalone.transitionPrefix.clear();
    return verify_arbitrary_sidecar(path,standalone);
}

}  // namespace

SolveCertificate solve_exact(const SolveOptions&options){
    require_hash(options.sourceSha256,"source SHA-256");
    require_hash(options.modelSha256,"model SHA-256");
    require_hash(options.observationSha256,"observation SHA-256");
    const TransitionHeaderDisk authenticated=authenticate_transition_database(
      options.transitionPrefix,options.sourceSha256,options.modelSha256,
      options.observationSha256,true);
    const ResourceEstimate resources=full_domain_preflight(
      options.transitionPrefix,options.bdd,options.resources,
      options.scratchPrefix);
    // During dependency authentication, the independent certified probe and
    // the solver's collision-free node importer coexist briefly.  Account for
    // both exact UFGM copies plus the concrete WDL payload before opening them.
    const std::uint64_t dependencyResident=checked_add(
      checked_mul(file_bytes(options.lowerGhostSidecar),2),
      file_bytes(options.sourceTable));
    if(checked_add(resources.peakResidentBytes,dependencyResident)>
         options.resources.maxResidentBytes||
       checked_add(resources.peakResidentBytes,dependencyResident)>
         physical_memory()*7/10)
        throw std::runtime_error(
          "Ghost-pair lower dependency exceeds resident-memory gate");
    const PackedTable concrete=load_concrete(options.sourceTable);
    if(concrete.sha!=options.sourceSha256)
        throw std::runtime_error("Ghost-pair concrete SHA mismatch");
    LowerGhostSidecar lower(options);TransitionDatabase database(
      options.transitionPrefix);PairRobdd bdd(options.scratchPrefix+".bdd-a",
      options.bdd,true);
    MmapFile<PairRobdd::Id>owner(options.scratchPrefix+".owner-a",
      database.owner_roots(),true),ownerNext(options.scratchPrefix+".owner-b",
      database.owner_roots(),true),observer(options.scratchPrefix+".observer-a",
      database.strata(),true),observerNext(options.scratchPrefix+".observer-b",
      database.strata(),true),domains(options.scratchPrefix+".domains",
      database.strata(),true);
    owner.fill(PairRobdd::False);ownerNext.fill(PairRobdd::False);
    observer.fill(PairRobdd::False);observerNext.fill(PairRobdd::False);
    DomainRootCache domainCache(bdd);
    const auto domainStarted=std::chrono::steady_clock::now();
    for(std::uint32_t gid=0;gid<database.geometries();++gid){
        const GeometryDisk&meta=database.meta(gid);
        for(std::uint32_t local=0;local<meta.stratumCount;++local)
            domains[meta.stratumBase+local]=domainCache.root(
              database.stratum(meta.stratumBase+local),meta.variableCount);
        if((gid+1)%50'000==0||gid+1==database.geometries()){
            const double elapsed=std::chrono::duration<double>(
              std::chrono::steady_clock::now()-domainStarted).count();
            std::cout<<"ghost_pair_domain_roots geometry "<<gid+1<<'/'
              <<database.geometries()<<" roots "
              <<meta.stratumBase+meta.stratumCount<<'/'<<database.strata()
              <<" cache_hits "<<domainCache.hits()
              <<" cache_misses "<<domainCache.misses()
              <<" cache_entries "<<domainCache.entries()
              <<" bdd_nodes "<<bdd.node_count()<<" elapsed "<<elapsed
              <<"s\n"<<std::flush;
        }
    }
    SolveCertificate certificate;certificate.domainRoots=database.strata();
    certificate.domainCacheHits=domainCache.hits();
    certificate.domainCacheMisses=domainCache.misses();
    certificate.domainCacheEntries=domainCache.entries();
    certificate.transitionPayloadSha256.assign(
      authenticated.payloadSha.data(),64);
    certificate.transitionHeaderSha256=sha256_file(
      options.transitionPrefix+".header");
    certificate.transitionMarkerSha256=sha256_file(
      options.transitionPrefix+".verified");
    certificate.lowerGhostSidecarSha256=options.lowerGhostSidecarSha256;
    for(;;){ExactKernel kernel(database,lower,bdd,owner,observer,domains,
                               &certificate);
        kernel.sweep(ownerNext,observerNext);++certificate.iterations;
        const bool stable=same_arrays(owner,ownerNext)&&
                          same_arrays(observer,observerNext);
        if(options.measureIterations&&
           certificate.iterations>=options.measureIterations){
            certificate.bddNodes=bdd.node_count();return certificate;}
        if(stable)break;
        std::swap(owner,ownerNext);std::swap(observer,observerNext);
        if(options.compactEvery&&
           certificate.iterations%options.compactEvery==0){
            std::vector<PairRobdd::Id>roots;roots.reserve(owner.size()+
              observer.size()+domains.size());
            for(std::uint64_t index=0;index<owner.size();++index)
                roots.push_back(owner[index]);
            for(std::uint64_t index=0;index<observer.size();++index)
                roots.push_back(observer[index]);
            for(std::uint64_t index=0;index<domains.size();++index)
                roots.push_back(domains[index]);
            auto compacted=bdd.compact(options.scratchPrefix+
              (certificate.compactions%2?".bdd-a":".bdd-b"),
              options.scratchPrefix+".compact-remap",roots);
            if(compacted.second.structuralResidual||
               compacted.second.rootResidual)
                throw std::runtime_error("Ghost-pair BDD compaction residual");
            std::size_t cursor=0;
            for(std::uint64_t index=0;index<owner.size();++index)
                owner[index]=roots[cursor++];
            for(std::uint64_t index=0;index<observer.size();++index)
                observer[index]=roots[cursor++];
            for(std::uint64_t index=0;index<domains.size();++index)
                domains[index]=roots[cursor++];
            bdd=std::move(compacted.first);++certificate.compactions;
        }
    }
    ExactKernel verifier(database,lower,bdd,owner,observer,domains,&certificate);
    verifier.sweep(ownerNext,observerNext);
    for(std::uint64_t index=0;index<owner.size();++index)
        certificate.bellmanResidual+=owner[index]!=ownerNext[index];
    for(std::uint64_t index=0;index<observer.size();++index)
        certificate.bellmanResidual+=observer[index]!=observerNext[index];
    if(certificate.bellmanResidual)
        throw std::runtime_error("Ghost-pair Bellman residual");
    for(std::uint32_t gid=0;gid<database.geometries();++gid){
        const GeometryDisk&meta=database.meta(gid);const PublicFrame frame=
          decode_geometry(meta.raw);
        for(unsigned actual=0;actual<meta.variableCount;++actual){
            if(!meta.live.test(actual))continue;
            const std::uint32_t stratum=meta.stratumBase+
              database.actual_stratum(meta,actual);
            certificate.rankResidual+=!bdd.is_upward_closed(
              owner[owner_root_index(meta,actual)],database.stratum(stratum));
            PairMask singleton;singleton.set(actual);
            const PairWorld world=decode_pair_variable(frame,actual);
            const std::uint32_t concreteIndex=encode_source(product_to_source(
              frame,world));const Wdl exact=concrete.result(concreteIndex);
            certificate.singletonResidual+=bdd.evaluate(
              owner[owner_root_index(meta,actual)],singleton)!=
              wdl_forces(exact,frame.side,Color::White);
            certificate.singletonResidual+=bdd.evaluate(observer[stratum],
              singleton)!=wdl_forces(exact,frame.side,Color::Black);
        }
        for(std::uint32_t local=0;local<meta.stratumCount;++local)
            certificate.rankResidual+=!bdd.is_downward_closed(
              observer[meta.stratumBase+local]);
    }
    if(certificate.rankResidual||certificate.singletonResidual)
        throw std::runtime_error("Ghost-pair rank/singleton residual");
    std::vector<std::uint8_t>flags(StateCount,0);
    std::array<std::set<std::string>,2>rootSets;
    for(std::uint32_t index=0;index<StateCount;++index){
        const ConcreteState source=decode_source(index);
        const FramedWorld physical=source_to_product(source);
        const Position position=make_position(physical.frame,physical.world);
        const std::size_t side=static_cast<std::size_t>(source.side);
        if(fresh_world_admission(physical.frame,physical.world)!=
             AdmissionVerdict::Admit){
            ++certificate.unreachable[side][static_cast<unsigned>(
              concrete.result(index))];++certificate.unreachableRealizations[side];
            continue;
        }
        ++certificate.legalRealizations[side];
        if(position.game_over()){
            flags[index]=4|terminal_flags(position);
            rootSets[side].insert(canonical_terminal_view(physical.frame,
                                                          physical.world));
        }else{
            const PairMask root=fresh_root(physical.frame,physical.world);
            const unsigned rawActual=pair_variable(physical.frame,
                                                   physical.world);
            if(!root.test(rawActual)){++certificate.partitionResidual;
                throw std::runtime_error("Ghost-pair fresh root lost actual world");}
            const CanonicalSet canonical=canonicalize_set(physical.frame,root);
            const FramedWorld actual=transform_world(physical.frame,
              physical.world,canonical.transform);
            const std::uint32_t gid=database.ordinal(encode_geometry(
              canonical.value.frame));const GeometryDisk&meta=database.meta(gid);
            const unsigned variable=pair_variable(canonical.value.frame,
                                                  actual.world);
            const std::uint32_t stratum=meta.stratumBase+
              database.actual_stratum(meta,variable);
            std::string key(sizeof(gid)+sizeof(PairMask),'\0');
            std::memcpy(key.data(),&gid,sizeof(gid));
            std::memcpy(key.data()+sizeof(gid),&canonical.value.worlds,
                        sizeof(PairMask));rootSets[side].insert(std::move(key));
            const bool white=bdd.evaluate(owner[owner_root_index(meta,variable)],
                                          canonical.value.worlds);
            const bool black=bdd.evaluate(observer[stratum],
                                          canonical.value.worlds);
            certificate.dualWinResidual+=white&&black;
            flags[index]=4|(white?1:0)|(black?2:0);
        }
        const bool mover=flags[index]&(source.side==Color::White?1:2);
        const bool opponent=flags[index]&(source.side==Color::White?2:1);
        ++certificate.totals[side][mover?1:opponent?2:3];
    }
    for(std::size_t side=0;side<2;++side){
        certificate.informationSets[side]=rootSets[side].size();
        std::uint64_t conserved=0;for(unsigned result=1;result<4;++result)
            conserved+=certificate.totals[side][result]+
                       certificate.unreachable[side][result];
        certificate.conservationResidual+=conserved!=StateCount/2;
        certificate.conservationResidual+=certificate.legalRealizations[side]+
          certificate.unreachableRealizations[side]!=StateCount/2;
    }
    if(certificate.dualWinResidual||certificate.conservationResidual||
       certificate.partitionResidual||certificate.observationResidual)
        throw std::runtime_error("Ghost-pair root conservation residual");
    // Strip every construction-only/domain-only ROBDD node before producing
    // the permanent all-beliefs artifact.  The compaction certificate proves
    // the complete owner/observer function vector is unchanged.
    std::vector<PairRobdd::Id>probeRoots;probeRoots.reserve(owner.size()+
      observer.size());
    for(std::uint64_t index=0;index<owner.size();++index)
        probeRoots.push_back(owner[index]);
    for(std::uint64_t index=0;index<observer.size();++index)
        probeRoots.push_back(observer[index]);
    auto compacted=bdd.compact(options.scratchPrefix+
      (certificate.compactions%2?".bdd-a":".bdd-b"),
      options.scratchPrefix+".compact-remap",probeRoots);
    if(compacted.second.structuralResidual||compacted.second.rootResidual)
        throw std::runtime_error("Ghost-pair final probe compaction residual");
    std::size_t rootCursor=0;
    for(std::uint64_t index=0;index<owner.size();++index)
        owner[index]=probeRoots[rootCursor++];
    for(std::uint64_t index=0;index<observer.size();++index)
        observer[index]=probeRoots[rootCursor++];
    bdd=std::move(compacted.first);++certificate.compactions;
    std::ofstream output(options.outputOverlay,std::ios::binary|std::ios::trunc);
    output.write(OverlayMagic,8);for(std::uint32_t word:{2u,
      static_cast<std::uint32_t>(PieceType::Ghost),
      static_cast<std::uint32_t>(PieceType::Ghost),
      static_cast<std::uint32_t>(Color::White),StateCount,4u})
        write_value(output,word);
    output.write(options.sourceSha256.data(),64);
    output.write(options.modelSha256.data(),64);
    output.write(reinterpret_cast<const char*>(flags.data()),flags.size());
    if(!output)throw std::runtime_error("cannot write Ghost-pair UFIW2");
    output.close();certificate.overlaySha256=sha256_file(options.outputOverlay);
    const ArbitrarySidecarCertificate arbitrary=write_arbitrary_sidecar(
      options.outputArbitrarySidecar,options,authenticated,database,bdd,owner,
      observer);
    certificate.arbitrarySidecarSha256=arbitrary.fileSha256;
    certificate.bddNodes=bdd.node_count();return certificate;
}

ArbitrarySidecarCertificate verify_arbitrary_sidecar(
  const std::string&path,const SolveOptions&options){
    require_hash(options.sourceSha256,"source SHA-256");
    require_hash(options.modelSha256,"model SHA-256");
    require_hash(options.observationSha256,"observation SHA-256");
    require_hash(options.lowerGhostSidecarSha256,
                 "lower Ghost sidecar SHA-256");
    require_hash(options.lowerGhostSourceSha256,"lower Ghost source SHA-256");
    require_hash(options.lowerGhostModelSha256,"lower Ghost model SHA-256");
    require_hash(options.lowerGhostObservationSha256,
                 "lower Ghost observation SHA-256");
    std::ifstream headerInput(path,std::ios::binary);
    const ArbitraryHeaderDisk header=read_value<ArbitraryHeaderDisk>(headerInput);
    const auto text=[](const std::array<char,64>&value){return std::string(
      value.data(),value.size());};
    if(header.magic!=std::array<char,8>{'U','F','G','G','1',0,0,0}||
       header.version!=1||header.headerBytes!=sizeof(header)||
       header.primary!=static_cast<std::uint32_t>(PieceType::Ghost)||
       header.secondary!=static_cast<std::uint32_t>(PieceType::Ghost)||
       header.owner!=static_cast<std::uint32_t>(Color::White)||
       header.files!=Position::BoardFiles||header.ranks!=Position::BoardRanks||
       header.squares!=Squares||header.variables!=MaximumWorlds||
       header.stateCount!=StateCount||
       header.nodeBytes!=sizeof(PairRobdd::NodeRecord)||
       header.geometryBytes!=sizeof(ArbitraryGeometryDisk)||
       header.maskBytes!=sizeof(PairMask)||header.reserved||
       header.nodes<2||header.nodes>std::numeric_limits<PairRobdd::Id>::max()||
       !header.geometries||!header.strata||!header.actuals||
       !header.ownerRoots||header.semantics!=arbitrary_semantics()||
       text(header.sourceSha)!=options.sourceSha256||
       text(header.modelSha)!=options.modelSha256||
       text(header.observationSha)!=options.observationSha256||
       text(header.lowerSidecarSha)!=options.lowerGhostSidecarSha256||
       text(header.lowerSourceSha)!=options.lowerGhostSourceSha256||
       text(header.lowerModelSha)!=options.lowerGhostModelSha256||
       text(header.lowerObservationSha)!=
         options.lowerGhostObservationSha256||
       !valid_sha256(text(header.transitionPayloadSha))||
       !valid_sha256(text(header.transitionHeaderSha))||
       !valid_sha256(text(header.transitionMarkerSha))||
       !valid_sha256(text(header.payloadSha)))
        throw std::runtime_error("invalid Ghost-pair arbitrary sidecar header");
    std::uint64_t cursor=sizeof(header);
    const auto section=[&](std::uint64_t declared,std::uint64_t count,
                           std::uint64_t width){
        if(declared!=cursor)throw std::runtime_error(
          "Ghost-pair arbitrary sidecar offset residual");
        cursor=checked_add(cursor,checked_mul(count,width));};
    section(header.nodeOffset,header.nodes,sizeof(PairRobdd::NodeRecord));
    section(header.geometryOffset,header.geometries,
            sizeof(ArbitraryGeometryDisk));
    section(header.stratumOffset,header.strata,sizeof(PairMask));
    section(header.actualOffset,header.actuals,sizeof(std::uint32_t));
    section(header.ownerOffset,header.ownerRoots,sizeof(PairRobdd::Id));
    section(header.observerOffset,header.strata,sizeof(PairRobdd::Id));
    if(header.payloadBytes!=cursor-sizeof(header)||file_bytes(path)!=cursor||
       sha256_range(path,sizeof(header),header.payloadBytes)!=
         text(header.payloadSha))
        throw std::runtime_error("Ghost-pair arbitrary sidecar payload residual");
    if(!options.transitionPrefix.empty()){
        const TransitionHeaderDisk transition=authenticate_transition_database(
          options.transitionPrefix,options.sourceSha256,options.modelSha256,
          options.observationSha256,true);
        if(text(header.transitionPayloadSha)!=
             std::string(transition.payloadSha.data(),64)||
           text(header.transitionHeaderSha)!=sha256_file(
             options.transitionPrefix+".header")||
           text(header.transitionMarkerSha)!=sha256_file(
             options.transitionPrefix+".verified"))
            throw std::runtime_error(
              "Ghost-pair arbitrary transition provenance residual");
    }
    if(!options.lowerGhostSidecar.empty()&&
       sha256_file(options.lowerGhostSidecar)!=options.lowerGhostSidecarSha256)
        throw std::runtime_error(
          "Ghost-pair arbitrary lower-sidecar provenance residual");

    // Validate the complete reduced ordered arena in one forward scan.  Two
    // bytes per node retain only prior variable levels, avoiding a second
    // node-arena mapping while still certifying child order.
    std::ifstream nodes(path,std::ios::binary);nodes.seekg(
      static_cast<std::streamoff>(header.nodeOffset));
    std::vector<std::uint16_t>levels;levels.reserve(
      static_cast<std::size_t>(header.nodes));
    for(std::uint64_t id=0;id<header.nodes;++id){
        const PairRobdd::NodeRecord node=read_value<PairRobdd::NodeRecord>(nodes);
        if(id<2){if(node.variable!=MaximumWorlds||node.low!=id||node.high!=id)
                throw std::runtime_error("bad Ghost-pair arbitrary terminal");}
        else{if(node.variable>=MaximumWorlds||node.low>=id||node.high>=id||
                node.low==node.high||levels[node.low]<=node.variable||
                levels[node.high]<=node.variable)
                throw std::runtime_error("bad Ghost-pair arbitrary node");}
        levels.push_back(node.variable);
    }

    const auto mask_within=[](const PairMask&mask,unsigned count){
        const unsigned whole=count/64,tail=count%64;
        if(tail&&whole<MaskWords&&
           (mask.words[whole]&~((std::uint64_t{1}<<tail)-1)))return false;
        for(unsigned word=whole+(tail?1:0);word<MaskWords;++word)
            if(mask.words[word])return false;
        return true;};
    std::ifstream geometries(path,std::ios::binary),strata(path,std::ios::binary),
      actuals(path,std::ios::binary),ownerRoots(path,std::ios::binary),
      observerRoots(path,std::ios::binary);
    geometries.seekg(static_cast<std::streamoff>(header.geometryOffset));
    strata.seekg(static_cast<std::streamoff>(header.stratumOffset));
    actuals.seekg(static_cast<std::streamoff>(header.actualOffset));
    ownerRoots.seekg(static_cast<std::streamoff>(header.ownerOffset));
    observerRoots.seekg(static_cast<std::streamoff>(header.observerOffset));
    std::uint64_t stratumCursor=0,actualCursor=0,ownerCursor=0;
    std::uint32_t previousRaw=0;
    for(std::uint64_t gid=0;gid<header.geometries;++gid){
        const ArbitraryGeometryDisk meta=read_value<ArbitraryGeometryDisk>(
          geometries);
        const PublicFrame frame=decode_geometry(meta.raw);
        const auto canonical=canonical_geometry(frame);
        if((gid&&meta.raw<=previousRaw)||canonical.first!=meta.raw||
           meta.variableCount!=variable_count(frame)||
           meta.ownerBase!=ownerCursor||meta.actualBase!=actualCursor||
           meta.stratumBase!=stratumCursor||!meta.stratumCount||
           !meta.liveCount||meta.liveCount>meta.variableCount)
            throw std::runtime_error("bad Ghost-pair arbitrary geometry");
        previousRaw=meta.raw;
        std::vector<bool>used(meta.stratumCount,false);
        std::vector<std::uint32_t>reverse(meta.variableCount,NoIndex);
        for(unsigned variable=0;variable<meta.variableCount;++variable){
            const std::uint32_t local=read_value<std::uint32_t>(actuals);
            if(local!=NoIndex){if(local>=meta.stratumCount)
                    throw std::runtime_error(
                      "bad Ghost-pair arbitrary reverse map");
                used[local]=true;}reverse[variable]=local;
        }
        PairMask live;
        for(std::uint32_t local=0;local<meta.stratumCount;++local){
            const PairMask cell=read_value<PairMask>(strata);
            const PairRobdd::Id root=read_value<PairRobdd::Id>(observerRoots);
            if(!used[local]||!cell.count()||!mask_within(cell,
              meta.variableCount)||root>=header.nodes)
                throw std::runtime_error("bad Ghost-pair arbitrary stratum");
            for(unsigned word=0;word<MaskWords;++word){std::uint64_t bits=
                cell.words[word];while(bits){const unsigned bit=static_cast<unsigned>(
                  __builtin_ctzll(bits));const unsigned variable=word*64+bit;
                  if(variable>=meta.variableCount||reverse[variable]!=local||
                     live.test(variable))throw std::runtime_error(
                       "bad Ghost-pair arbitrary stratum partition");
                  live.set(variable);bits&=bits-1;}}
        }
        if(live.count()!=meta.liveCount)throw std::runtime_error(
          "bad Ghost-pair arbitrary live count");
        for(unsigned variable=0;variable<meta.variableCount;++variable)
            if(live.test(variable)!=(reverse[variable]!=NoIndex))
                throw std::runtime_error(
                  "bad Ghost-pair arbitrary live reverse map");
        for(std::uint32_t local=0;local<meta.liveCount;++local)
            if(read_value<PairRobdd::Id>(ownerRoots)>=header.nodes)
                throw std::runtime_error("bad Ghost-pair arbitrary owner root");
        stratumCursor+=meta.stratumCount;actualCursor+=meta.variableCount;
        ownerCursor+=meta.liveCount;
    }
    if(stratumCursor!=header.strata||actualCursor!=header.actuals||
       ownerCursor!=header.ownerRoots)
        throw std::runtime_error("Ghost-pair arbitrary catalog conservation");
    ArbitrarySidecarCertificate certificate;certificate.nodes=header.nodes;
    certificate.geometries=header.geometries;certificate.strata=header.strata;
    certificate.ownerRoots=header.ownerRoots;certificate.bytes=cursor;
    certificate.payloadSha256=text(header.payloadSha);
    certificate.fileSha256=sha256_file(path);
    certificate.transitionPayloadSha256=text(header.transitionPayloadSha);
    certificate.transitionHeaderSha256=text(header.transitionHeaderSha);
    certificate.transitionMarkerSha256=text(header.transitionMarkerSha);
    certificate.lowerGhostSidecarSha256=text(header.lowerSidecarSha);
    return certificate;
}

class ArbitrarySidecarProbe::Impl {
  public:
    Impl(const std::string&path,const SolveOptions&options)
      :certificate_(verify_arbitrary_sidecar(path,options)){
        bytes_=file_bytes(path);if(bytes_>std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("Ghost-pair arbitrary sidecar too large");
        descriptor_=::open(path.c_str(),O_RDONLY);
        if(descriptor_<0)system_error("cannot open",path);
        void*mapping=::mmap(nullptr,static_cast<std::size_t>(bytes_),PROT_READ,
                            MAP_SHARED,descriptor_,0);
        if(mapping==MAP_FAILED){::close(descriptor_);descriptor_=-1;
            system_error("cannot mmap",path);}
        data_=static_cast<const std::uint8_t*>(mapping);
        header_=read<ArbitraryHeaderDisk>(0);
    }
    ~Impl(){
        if(data_)::munmap(const_cast<std::uint8_t*>(data_),
                          static_cast<std::size_t>(bytes_));
        if(descriptor_>=0)::close(descriptor_);
    }
    Impl(const Impl&)=delete;Impl&operator=(const Impl&)=delete;

    [[nodiscard]] bool forces(const PublicFrame&frame,const PairWorld&actual,
                              const PairMask&worlds,bool owner)const{
        const unsigned rawActual=pair_variable(frame,actual);
        if(!worlds.count()||!worlds.test(rawActual))throw std::invalid_argument(
          "Ghost-pair probe belief does not contain actual world");
        const CanonicalSet canonical=canonicalize_set(frame,worlds);
        const FramedWorld mapped=transform_world(frame,actual,
                                                 canonical.transform);
        const std::uint32_t raw=encode_geometry(canonical.value.frame);
        std::uint64_t low=0,high=header_.geometries;
        while(low<high){const std::uint64_t middle=(low+high)/2;
            if(geometry(middle).raw<raw)low=middle+1;else high=middle;}
        if(low>=header_.geometries||geometry(low).raw!=raw)
            throw std::runtime_error("Ghost-pair probe geometry absent");
        const ArbitraryGeometryDisk meta=geometry(low);
        const unsigned variable=pair_variable(canonical.value.frame,
                                               mapped.world);
        if(variable>=meta.variableCount)throw std::runtime_error(
          "Ghost-pair probe actual coordinate residual");
        const std::uint32_t local=read<std::uint32_t>(header_.actualOffset+
          (meta.actualBase+variable)*sizeof(std::uint32_t));
        if(local==NoIndex||local>=meta.stratumCount)throw std::invalid_argument(
          "Ghost-pair probe requested terminal/nonlive world");
        const PairMask cell=read<PairMask>(header_.stratumOffset+
          (meta.stratumBase+local)*sizeof(PairMask));
        for(unsigned word=0;word<MaskWords;++word)
            if(canonical.value.worlds.words[word]&~cell.words[word])
                throw std::invalid_argument(
                  "Ghost-pair probe belief spans legal-dot decision cells");
        PairRobdd::Id root=PairRobdd::False;
        if(owner){std::uint64_t rank=0;
            for(unsigned candidate=0;candidate<variable;++candidate)
                rank+=read<std::uint32_t>(header_.actualOffset+
                  (meta.actualBase+candidate)*sizeof(std::uint32_t))!=NoIndex;
            if(rank>=meta.liveCount)throw std::runtime_error(
              "Ghost-pair probe owner rank residual");
            root=read<PairRobdd::Id>(header_.ownerOffset+
              (meta.ownerBase+rank)*sizeof(PairRobdd::Id));
        }else root=read<PairRobdd::Id>(header_.observerOffset+
          (meta.stratumBase+local)*sizeof(PairRobdd::Id));
        while(root>PairRobdd::True){const PairRobdd::NodeRecord node=
            read<PairRobdd::NodeRecord>(header_.nodeOffset+
              std::uint64_t(root)*sizeof(PairRobdd::NodeRecord));
            root=canonical.value.worlds.test(node.variable)?node.high:node.low;}
        return root==PairRobdd::True;
    }
    [[nodiscard]] const ArbitrarySidecarCertificate&certificate()const{
        return certificate_;}
  private:
    template<typename Value>
    [[nodiscard]] Value read(std::uint64_t offset)const{
        if(offset>bytes_||sizeof(Value)>bytes_-offset)
            throw std::runtime_error("Ghost-pair arbitrary mapped read overflow");
        Value result;std::memcpy(&result,data_+offset,sizeof(result));return result;
    }
    [[nodiscard]] ArbitraryGeometryDisk geometry(std::uint64_t id)const{
        return read<ArbitraryGeometryDisk>(header_.geometryOffset+
          id*sizeof(ArbitraryGeometryDisk));}
    int descriptor_=-1;const std::uint8_t*data_=nullptr;std::uint64_t bytes_=0;
    ArbitraryHeaderDisk header_{};ArbitrarySidecarCertificate certificate_;
};

ArbitrarySidecarProbe::ArbitrarySidecarProbe(const std::string&path,
  const SolveOptions&options):impl_(new Impl(path,options)){}
ArbitrarySidecarProbe::~ArbitrarySidecarProbe(){delete impl_;}
ArbitrarySidecarProbe::ArbitrarySidecarProbe(
  ArbitrarySidecarProbe&&other)noexcept
  :impl_(std::exchange(other.impl_,nullptr)){}
ArbitrarySidecarProbe&ArbitrarySidecarProbe::operator=(
  ArbitrarySidecarProbe&&other)noexcept{
    if(this!=&other){delete impl_;impl_=std::exchange(other.impl_,nullptr);}
    return *this;
}
bool ArbitrarySidecarProbe::owner_forces(const PublicFrame&frame,
  const PairWorld&actual,const PairMask&worlds)const{
    if(!impl_)throw std::runtime_error("moved-from Ghost-pair probe");
    return impl_->forces(frame,actual,worlds,true);
}
bool ArbitrarySidecarProbe::observer_forces(const PublicFrame&frame,
  const PairWorld&actual,const PairMask&worlds)const{
    if(!impl_)throw std::runtime_error("moved-from Ghost-pair probe");
    return impl_->forces(frame,actual,worlds,false);
}
const ArbitrarySidecarCertificate&ArbitrarySidecarProbe::certificate()const{
    if(!impl_)throw std::runtime_error("moved-from Ghost-pair probe");
    return impl_->certificate();
}

SolveCertificate verify_exact_overlay(const SolveOptions&options){
    const PackedTable concrete=load_concrete(options.sourceTable);
    if(concrete.sha!=options.sourceSha256)
        throw std::runtime_error("Ghost-pair overlay source mismatch");
    std::ifstream input(options.outputOverlay,std::ios::binary);
    std::array<char,160>header{};input.read(header.data(),header.size());
    const auto word=[&](std::size_t offset){return little_u32(
      reinterpret_cast<const std::uint8_t*>(header.data()+offset));};
    if(!input||std::memcmp(header.data(),OverlayMagic,8)||word(8)!=2||
       word(12)!=static_cast<std::uint32_t>(PieceType::Ghost)||
       word(16)!=static_cast<std::uint32_t>(PieceType::Ghost)||
       word(20)!=static_cast<std::uint32_t>(Color::White)||
       word(24)!=StateCount||word(28)!=4||
       std::string(header.data()+32,64)!=options.sourceSha256||
       std::string(header.data()+96,64)!=options.modelSha256)
        throw std::runtime_error("Ghost-pair UFIW2 header mismatch");
    std::vector<std::uint8_t>flags(StateCount);input.read(
      reinterpret_cast<char*>(flags.data()),flags.size());
    if(!input||input.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("Ghost-pair UFIW2 extent mismatch");
    SolveCertificate certificate;certificate.overlaySha256=sha256_file(
      options.outputOverlay);std::array<std::set<std::string>,2>rootSets;
    for(std::uint32_t index=0;index<StateCount;++index){
        const ConcreteState source=decode_source(index);
        const FramedWorld physical=source_to_product(source);
        const Position position=make_position(physical.frame,physical.world);
        const std::size_t side=static_cast<std::size_t>(source.side);
        if((flags[index]&~std::uint8_t{7})||
           (!(flags[index]&4)&&(flags[index]&3)))
            throw std::runtime_error("malformed Ghost-pair UFIW2 flags");
        const bool admitted=fresh_world_admission(physical.frame,
          physical.world)==AdmissionVerdict::Admit;
        if(bool(flags[index]&4)!=admitted)
            throw std::runtime_error("Ghost-pair UFIW2 admission residual");
        if(!admitted){++certificate.unreachableRealizations[side];
            ++certificate.unreachable[side][static_cast<unsigned>(
              concrete.result(index))];continue;}
        ++certificate.legalRealizations[side];
        if(position.game_over()){
            if((flags[index]&3)!=terminal_flags(position))
                throw std::runtime_error("Ghost-pair terminal flag residual");
            rootSets[side].insert(canonical_terminal_view(physical.frame,
                                                          physical.world));
        }else{
            const PairMask root=fresh_root(physical.frame,physical.world);
            if(!root.test(pair_variable(physical.frame,physical.world))){
                ++certificate.partitionResidual;continue;}
            const CanonicalSet canonical=canonicalize_set(physical.frame,root);
            const std::uint32_t geometry=encode_geometry(canonical.value.frame);
            std::string key(sizeof(geometry)+sizeof(PairMask),'\0');
            std::memcpy(key.data(),&geometry,sizeof(geometry));
            std::memcpy(key.data()+sizeof(geometry),&canonical.value.worlds,
                        sizeof(PairMask));rootSets[side].insert(std::move(key));
        }
        const bool white=flags[index]&1,black=flags[index]&2;
        certificate.dualWinResidual+=white&&black;
        const bool mover=source.side==Color::White?white:black;
        const bool opponent=source.side==Color::White?black:white;
        ++certificate.totals[side][mover?1:opponent?2:3];
    }
    for(std::size_t side=0;side<2;++side){certificate.informationSets[side]=
        rootSets[side].size();std::uint64_t conserved=0;
        for(unsigned result=1;result<4;++result)conserved+=
          certificate.totals[side][result]+certificate.unreachable[side][result];
        certificate.conservationResidual+=conserved!=StateCount/2;
        certificate.conservationResidual+=certificate.legalRealizations[side]+
          certificate.unreachableRealizations[side]!=StateCount/2;}
    if(certificate.dualWinResidual||certificate.conservationResidual||
       certificate.partitionResidual)
        throw std::runtime_error("Ghost-pair UFIW2 verification residual");
    return certificate;
}

void exact_small_domain_self_test(const std::string&scratchPrefix){
    // Complete public-frame codec certificate.  decode_geometry performs the
    // inverse encode assertion internally for every HH/HV/VV raw coordinate.
    for(std::uint32_t raw=0;raw<RawGeometryCount;++raw)
        (void)decode_geometry(raw);

    // The production concrete-plane validator is count-parametric.  Exercise
    // a complete tiny plane so malformed WDL zeroes and trailing extents fail
    // closed without constructing the 75,915,840-record production payload.
    std::vector<std::uint8_t>packed(5,0);
    packed[0]=std::uint8_t{1}|(std::uint8_t{2}<<2)|
              (std::uint8_t{3}<<4)|(std::uint8_t{1}<<6);
    validate_packed_planes(packed,0,4,1,4,0);
    std::vector<std::uint8_t>invalidWdl=packed;invalidWdl[0]&=~std::uint8_t{3};
    bool rejectedWdl=false;try{validate_packed_planes(invalidWdl,0,4,1,4,0);}
    catch(const std::runtime_error&){rejectedWdl=true;}
    std::vector<std::uint8_t>trailing=packed;trailing.push_back(0);
    bool rejectedExtent=false;try{validate_packed_planes(trailing,0,4,1,4,0);}
    catch(const std::runtime_error&){rejectedExtent=true;}
    if(!rejectedWdl||!rejectedExtent)
        throw std::runtime_error("malformed concrete-plane input was accepted");

    PairRobdd::Limits limits;limits.maxNodes=20'000;limits.uniqueSlots=1<<15;
    limits.applyCacheEntries=20'000;limits.unaryCacheEntries=10'000;
    limits.budgetBytes=1ULL<<30;PairRobdd bdd(scratchPrefix+".a",limits,true);
    const std::array<unsigned,6>variables{{0,1,2,77,78,3002}};
    std::array<PairRobdd::Id,6>items{};for(unsigned index=0;index<items.size();++index)
        items[index]=bdd.variable(variables[index]);
    const PairRobdd::Id correlated=bdd.logical_or(
      bdd.logical_and(items[0],items[1]),bdd.logical_and(items[2],items[5]));
    for(unsigned bits=0;bits<(1u<<items.size());++bits){PairMask assignment;
        for(unsigned index=0;index<items.size();++index)if(bits&(1u<<index))
            assignment.set(variables[index]);
        const bool expected=
              ((bits&1)&&(bits&2))||((bits&4)&&(bits&(1u<<5)));
        if(bdd.evaluate(correlated,assignment)!=expected)
            throw std::runtime_error("PairRobdd exhaustive truth-table residual");}
    PairMask allowed;allowed.set(variables[0]);allowed.set(variables[1]);
    DomainRootCache domainCache(bdd);
    const PairRobdd::Id domain=domainCache.root(allowed,3);
    const std::uint32_t domainNodes=bdd.node_count();
    if(domainCache.root(allowed,3)!=domain||
       bdd.node_count()!=domainNodes||domainCache.hits()!=1||
       domainCache.misses()!=1||domainCache.entries()!=1)
        throw std::runtime_error("PairRobdd domain-cache reuse residual");
    for(unsigned bits=0;bits<16;++bits){PairMask assignment;
        for(unsigned variable=0;variable<4;++variable)
            if(bits&(1u<<variable))assignment.set(variable);
        const bool expected=!(bits&(1u<<2))&&!(bits&(1u<<3));
        if(bdd.evaluate(domain,assignment)!=expected)
            throw std::runtime_error("PairRobdd direct domain residual");}
    const PairRobdd::Id upward=bdd.logical_and(items[0],items[1]);
    const PairRobdd::Id downward=bdd.logical_not(upward);
    if(!bdd.is_upward_closed(upward,allowed)||
       !bdd.is_downward_closed(downward)||
       bdd.evaluate(upward,PairMask{})||
       !bdd.evaluate(downward,PairMask{}))
        throw std::runtime_error("PairRobdd monotonic/empty-bottom residual");
    std::vector<PairRobdd::Id>image(MaximumWorlds,PairRobdd::False);
    image[variables[0]]=items[2];image[variables[1]]=items[5];
    const PairRobdd::Id composed=bdd.compose(upward,image,7);
    for(unsigned bits=0;bits<4;++bits){PairMask assignment;
        if(bits&1)assignment.set(variables[2]);if(bits&2)assignment.set(variables[5]);
        if(bdd.evaluate(composed,assignment)!=(bits==3))
            throw std::runtime_error("PairRobdd composition residual");}
    std::vector<PairRobdd::Id>roots{correlated,upward,downward,composed};
    for(unsigned pass=0;pass<3;++pass){auto compacted=bdd.compact(
        scratchPrefix+(pass%2?".a":".b"),scratchPrefix+".remap",roots);
        if(compacted.second.structuralResidual||compacted.second.rootResidual)
            throw std::runtime_error("PairRobdd repeated compaction residual");
        bdd=std::move(compacted.first);}
    const std::uint64_t arenaBytes=PairRobdd::required_bytes(limits);
    const std::uint64_t firstArena=file_bytes(scratchPrefix+".a.nodes")+
      file_bytes(scratchPrefix+".a.unique");
    const std::uint64_t secondArena=file_bytes(scratchPrefix+".b.nodes")+
      file_bytes(scratchPrefix+".b.unique");
    const std::uint64_t remapBytes=file_bytes(scratchPrefix+".remap");
    if(firstArena!=arenaBytes||secondArena!=arenaBytes||
       remapBytes<2*sizeof(PairRobdd::Id)||
       remapBytes>std::uint64_t(limits.maxNodes)*sizeof(PairRobdd::Id)||
       remapBytes%sizeof(PairRobdd::Id))
        throw std::runtime_error("PairRobdd two-arena disk bound residual");
    for(unsigned bits=0;bits<(1u<<items.size());++bits){PairMask assignment;
        for(unsigned index=0;index<items.size();++index)if(bits&(1u<<index))
            assignment.set(variables[index]);
        const bool expected=
              ((bits&1)&&(bits&2))||((bits&4)&&(bits&(1u<<5)));
        if(bdd.evaluate(roots[0],assignment)!=expected)
            throw std::runtime_error("PairRobdd compacted truth residual");}

    // Serialize a genuine 3,003-world hidden-hidden UFGG1 catalog and load it
    // only through the production standalone authenticator/probe.  This
    // exercises every permanent section and proves that transition scratch is
    // not needed to retain or query a correlated arbitrary-belief result.
    const PublicFrame sidecarFrame=decode_geometry(0);
    TransitionCertificate sidecarIgnored;
    const BuiltBlock catalog=build_block(sidecarFrame,false,sidecarIgnored);
    if(catalog.meta.variableCount!=MaximumWorlds||
       catalog.meta.liveCount!=MaximumWorlds||
       catalog.meta.stratumCount!=1)
        throw std::runtime_error(
          "hidden-hidden arbitrary sidecar test domain residual");
    SolveOptions sidecarOptions;
    sidecarOptions.sourceSha256=std::string(64,'1');
    sidecarOptions.modelSha256=std::string(64,'2');
    sidecarOptions.observationSha256=std::string(64,'3');
    sidecarOptions.lowerGhostSidecarSha256=std::string(64,'4');
    sidecarOptions.lowerGhostSourceSha256=std::string(64,'5');
    sidecarOptions.lowerGhostModelSha256=std::string(64,'6');
    sidecarOptions.lowerGhostObservationSha256=std::string(64,'7');
    ArbitraryHeaderDisk sidecarHeader;sidecarHeader.nodes=bdd.node_count();
    sidecarHeader.geometries=1;sidecarHeader.strata=catalog.strata.size();
    sidecarHeader.actuals=catalog.actualStratum.size();
    sidecarHeader.ownerRoots=catalog.meta.liveCount;
    sidecarHeader.nodeOffset=sizeof(sidecarHeader);
    sidecarHeader.geometryOffset=sidecarHeader.nodeOffset+
      sidecarHeader.nodes*sizeof(PairRobdd::NodeRecord);
    sidecarHeader.stratumOffset=sidecarHeader.geometryOffset+
      sizeof(ArbitraryGeometryDisk);
    sidecarHeader.actualOffset=sidecarHeader.stratumOffset+
      sidecarHeader.strata*sizeof(PairMask);
    sidecarHeader.ownerOffset=sidecarHeader.actualOffset+
      sidecarHeader.actuals*sizeof(std::uint32_t);
    sidecarHeader.observerOffset=sidecarHeader.ownerOffset+
      sidecarHeader.ownerRoots*sizeof(PairRobdd::Id);
    sidecarHeader.payloadBytes=sidecarHeader.observerOffset+
      sidecarHeader.strata*sizeof(PairRobdd::Id)-sizeof(sidecarHeader);
    sidecarHeader.semantics=arbitrary_semantics();
    copy_digest(sidecarHeader.sourceSha,sidecarOptions.sourceSha256,
                "test source SHA-256");
    copy_digest(sidecarHeader.modelSha,sidecarOptions.modelSha256,
                "test model SHA-256");
    copy_digest(sidecarHeader.observationSha,
      sidecarOptions.observationSha256,"test observation SHA-256");
    copy_digest(sidecarHeader.transitionPayloadSha,std::string(64,'8'),
                "test transition payload SHA-256");
    copy_digest(sidecarHeader.transitionHeaderSha,std::string(64,'9'),
                "test transition header SHA-256");
    copy_digest(sidecarHeader.transitionMarkerSha,std::string(64,'a'),
                "test transition marker SHA-256");
    copy_digest(sidecarHeader.lowerSidecarSha,
      sidecarOptions.lowerGhostSidecarSha256,
      "test lower sidecar SHA-256");
    copy_digest(sidecarHeader.lowerSourceSha,
      sidecarOptions.lowerGhostSourceSha256,"test lower source SHA-256");
    copy_digest(sidecarHeader.lowerModelSha,
      sidecarOptions.lowerGhostModelSha256,"test lower model SHA-256");
    copy_digest(sidecarHeader.lowerObservationSha,
      sidecarOptions.lowerGhostObservationSha256,
      "test lower observation SHA-256");
    const std::string sidecarPath=scratchPrefix+".ufgg";
    std::ofstream sidecar(sidecarPath,std::ios::binary|std::ios::trunc);
    write_value(sidecar,sidecarHeader);
    for(PairRobdd::Id id=0;id<bdd.node_count();++id)
        write_value(sidecar,bdd.node_record(id));
    const ArbitraryGeometryDisk sidecarGeometry{catalog.meta.raw,
      catalog.meta.variableCount,catalog.meta.ownerBase,catalog.meta.actualBase,
      catalog.meta.stratumBase,catalog.meta.stratumCount,
      catalog.meta.liveCount};
    write_value(sidecar,sidecarGeometry);
    for(const PairMask&cell:catalog.strata)write_value(sidecar,cell);
    for(std::uint32_t local:catalog.actualStratum)write_value(sidecar,local);
    for(std::uint32_t id=0;id<catalog.meta.liveCount;++id)
        write_value(sidecar,roots[0]);
    for(std::size_t id=0;id<catalog.strata.size();++id)
        write_value(sidecar,roots[2]);
    sidecar.close();const std::string sidecarPayload=sha256_range(sidecarPath,
      sizeof(sidecarHeader),sidecarHeader.payloadBytes);
    std::copy(sidecarPayload.begin(),sidecarPayload.end(),
              sidecarHeader.payloadSha.begin());
    std::fstream finalize(sidecarPath,std::ios::binary|std::ios::in|
                          std::ios::out);
    finalize.write(reinterpret_cast<const char*>(&sidecarHeader),
                   sizeof(sidecarHeader));finalize.close();
    const ArbitrarySidecarCertificate sidecarCertificate=
      verify_arbitrary_sidecar(sidecarPath,sidecarOptions);
    if(sidecarCertificate.nodes!=bdd.node_count()||
       sidecarCertificate.geometries!=1||
       sidecarCertificate.strata!=catalog.strata.size()||
       sidecarCertificate.ownerRoots!=catalog.meta.liveCount||
       sidecarCertificate.payloadSha256!=sidecarPayload)
        throw std::runtime_error("Ghost-pair arbitrary sidecar reload residual");
    {
        ArbitrarySidecarProbe probe(sidecarPath,sidecarOptions);
        const PublicFrame probeFrame=decode_geometry(catalog.meta.raw);
        const PairWorld probeActual=decode_pair_variable(probeFrame,0);
        PairMask probeWorlds;probeWorlds.set(0);probeWorlds.set(1);
        if(!probe.owner_forces(probeFrame,probeActual,probeWorlds)||
           probe.observer_forces(probeFrame,probeActual,probeWorlds)||
           probe.certificate().fileSha256!=sidecarCertificate.fileSha256)
            throw std::runtime_error(
              "Ghost-pair multi-world arbitrary sidecar query residual");
        const PairSet transformed=transform_set(probeFrame,probeWorlds,
          RectangleTransform::Both);
        const FramedWorld transformedActual=transform_world(probeFrame,
          probeActual,RectangleTransform::Both);
        if(!probe.owner_forces(transformed.frame,transformedActual.world,
                               transformed.worlds)||
           probe.observer_forces(transformed.frame,transformedActual.world,
                                  transformed.worlds))
            throw std::runtime_error(
              "Ghost-pair D2 arbitrary sidecar query residual");
        ArbitrarySidecarProbe moved(std::move(probe));
        bool rejectedMovedFrom=false;try{(void)probe.certificate();}
        catch(const std::runtime_error&){rejectedMovedFrom=true;}
        if(!rejectedMovedFrom||!moved.owner_forces(probeFrame,probeActual,
                                                   probeWorlds))
            throw std::runtime_error(
              "Ghost-pair moved probe ownership residual");
    }
    std::fstream corrupt(sidecarPath,std::ios::binary|std::ios::in|
                         std::ios::out);corrupt.seekg(
      static_cast<std::streamoff>(sidecarHeader.nodeOffset));char original=0;
    corrupt.read(&original,1);const char changed=original^1;corrupt.seekp(
      static_cast<std::streamoff>(sidecarHeader.nodeOffset));
    corrupt.write(&changed,1);corrupt.close();bool rejectedSidecar=false;
    try{(void)verify_arbitrary_sidecar(sidecarPath,sidecarOptions);}
    catch(const std::runtime_error&){rejectedSidecar=true;}
    std::fstream restore(sidecarPath,std::ios::binary|std::ios::in|
                         std::ios::out);restore.seekp(
      static_cast<std::streamoff>(sidecarHeader.nodeOffset));
    restore.write(&original,1);restore.close();
    if(!rejectedSidecar)throw std::runtime_error(
      "corrupt Ghost-pair arbitrary sidecar was accepted");

    const auto square=[](const char*name){const int value=
        Position::square_from_name(name);if(value==Position::NoSquare)
            throw std::runtime_error("bad Ghost-pair self-test square");
        return static_cast<std::uint8_t>(value);};
    const auto move_between=[](const Position&position,std::uint8_t from,
                               std::uint8_t to){
        std::optional<Move>result;for(const Move&move:position.legal_moves())
            if(move.from==from&&move.to==to){if(result)
                throw std::runtime_error("ambiguous Ghost-pair test move");
                result=move;}
        if(!result)throw std::runtime_error("missing Ghost-pair test move");
        return *result;};

    // Two distinct owner-private Ghost actions in two actual worlds produce
    // exactly the same Black observation.  The very helper used by the
    // production compiler must place both sources and both physical children
    // in one relation; adding ActionKey to that relation key breaks this test.
    const PublicFrame ownerFrame{Color::White,square("a1"),square("h10"),{},0};
    const std::array<PairWorld,2>witnessWorlds{{
      {square("a5"),square("c7")},{square("h5"),square("c7")}}};
    const std::array<std::pair<std::uint8_t,std::uint8_t>,2>endpoints{{
      {square("a5"),square("a6")},{square("h5"),square("h6")}}};
    std::map<std::string,std::map<std::uint16_t,PairMask>>relations;
    std::array<ActionKey,2>witnessActions{};
    for(unsigned index=0;index<witnessWorlds.size();++index){
        Position before=make_position(ownerFrame,witnessWorlds[index]);
        const Move move=move_between(before,endpoints[index].first,
                                     endpoints[index].second);
        witnessActions[index]=action_key(move);Position child=before;Undo undo;
        if(!child.make_move(move,undo))
            throw std::runtime_error("hidden Ghost witness move failed");
        const std::string observation=black_transition_observation(
          before,move,child);const FramedWorld physical=
          *same_class_product(child);const auto[raw,transform]=
          canonical_geometry(physical.frame);(void)raw;
        const FramedWorld mapped=transform_world(physical.frame,
          physical.world,transform);relations[observation][static_cast<std::uint16_t>(
            pair_variable(mapped.frame,mapped.world))].set(
              pair_variable(ownerFrame,witnessWorlds[index]));
    }
    if(witnessActions[0]==witnessActions[1]||relations.size()!=1||
       relations.begin()->second.size()!=2)
        throw std::runtime_error("owner-private action observation union residual");
    PairMask witnessSources;for(const auto&[target,sources]:
        relations.begin()->second){(void)target;for(unsigned variable=0;
          variable<variable_count(ownerFrame);++variable)
            if(sources.test(variable))witnessSources.set(variable);}
    for(const PairWorld&world:witnessWorlds)
        if(!witnessSources.test(pair_variable(ownerFrame,world)))
            throw std::runtime_error("compiled observation lost private source");

    // Inspect the actual production block—not a parallel test encoder.  Both
    // distinct ActionKeys must carry the same relation ID, and that relation's
    // stored edge image must contain both correlated sources and children.
    TransitionCertificate productionCertificate;
    const BuiltBlock production=build_block(ownerFrame,false,
                                             productionCertificate);
    std::array<std::uint32_t,2>witnessActionIds{};
    for(unsigned witness=0;witness<witnessActions.size();++witness){
        const auto found=std::find_if(production.actions.begin(),
          production.actions.end(),[&](const ActionDisk&item){
              return item.key==witnessActions[witness];});
        if(found==production.actions.end())
            throw std::runtime_error("production block lost witness action");
        witnessActionIds[witness]=static_cast<std::uint32_t>(
          found-production.actions.begin());
    }
    std::array<CompiledEdge,2>witnessEdges{};
    for(unsigned witness=0;witness<witnessWorlds.size();++witness){
        const unsigned source=pair_variable(ownerFrame,
                                             witnessWorlds[witness]);
        bool found=false;for(std::uint32_t cursor=production.edgeOffsets[source];
          cursor<production.edgeOffsets[source+1];++cursor)
            if(production.edges[cursor].edge.action==
                 witnessActionIds[witness]){
                if(found)throw std::runtime_error(
                  "production block duplicated witness action");
                witnessEdges[witness]=production.edges[cursor].edge;
                found=true;}
        if(!found)throw std::runtime_error(
          "production block lost witness source/action edge");
    }
    if(witnessEdges[0].relation!=witnessEdges[1].relation||
       witnessEdges[0].childActual==witnessEdges[1].childActual)
        throw std::runtime_error(
          "production relation conditioned on private ActionKey");
    PairMask productionSources;std::set<std::uint16_t>productionChildren;
    for(unsigned source=0;source<production.meta.variableCount;++source)
        for(std::uint32_t cursor=production.edgeOffsets[source];
            cursor<production.edgeOffsets[source+1];++cursor){
            const CompiledEdge&edge=production.edges[cursor].edge;
            if(edge.relation==witnessEdges[0].relation){
                productionSources.set(source);
                productionChildren.insert(edge.childActual);}}
    if(productionChildren.size()<2)
        throw std::runtime_error("production relation lost private children");
    for(const PairWorld&world:witnessWorlds)
        if(!productionSources.test(pair_variable(ownerFrame,world)))
            throw std::runtime_error("production relation lost private source");

    // Capturing the public member of a mixed pair projects the correlated
    // companion positions directly into the arbitrary-mask KGhost oracle.
    PublicFrame lowerFrame{Color::Black,square("a1"),square("e5"),{},1};
    lowerFrame.visibleGhosts[0]=square("e4");
    RuntimeRelation lowerRelation;const std::array<const char*,8>companions{{
      "a8","b8","c8","d8","e8","f8","g8","h8"}};
    for(const char*name:companions){const PairWorld world{
        std::min(square("e4"),square(name)),
        std::max(square("e4"),square(name))};
        Position before=make_position(lowerFrame,world);const Move capture=
          move_between(before,square("e5"),square("e4"));Position child=before;
        Undo undo;if(!child.make_move(capture,undo))
            throw std::runtime_error("lower Ghost capture witness failed");
        const ClassifiedChild classified=classify_child(child);
        if(classified.domain!=ChildDomain::LowerGhost)
            throw std::runtime_error("capture did not lower to KGhost");
        add_lower_image(lowerRelation,classified.index,
                        pair_variable(lowerFrame,world));}
    if(lowerRelation.lowerImage.size()!=companions.size())
        throw std::runtime_error("arbitrary lower Ghost mask was marginalized");

    // The dense concrete probe folds horizontally by the White King.  The
    // epistemic successor must instead retain the physical public frame until
    // the whole belief is transformed.  Exercise a native child where those
    // two representations differ and certify the compiled coordinate.
    const PublicFrame foldFrame{Color::Black,square("g2"),square("d10"),{},0};
    const PairWorld foldWorld{square("a5"),square("h5")};
    Position foldBefore=make_position(foldFrame,foldWorld);
    const Move foldMove=move_between(foldBefore,square("d10"),square("d9"));
    Position foldChild=foldBefore;Undo foldUndo;
    if(!foldChild.make_move(foldMove,foldUndo))
        throw std::runtime_error("physical-fold witness failed");
    const FramedWorld physical=*same_class_product(foldChild);
    const ClassifiedChild classified=classify_child(foldChild);
    const PublicFrame folded=source_to_product(decode_source(
      classified.index)).frame;
    if(physical.frame==folded)
        throw std::runtime_error("physical-fold witness did not separate frames");
    const CompiledEdge compiled=encode_child(foldChild,physical);
    const auto[physicalRaw,physicalTransform]=canonical_geometry(physical.frame);
    const FramedWorld physicalMapped=transform_world(physical.frame,
      physical.world,physicalTransform);
    if(compiled.domain!=CompiledChildDomain::SameClass||
       compiled.childGeometry!=physicalRaw||compiled.childActual!=
         pair_variable(physicalMapped.frame,physicalMapped.world))
        throw std::runtime_error("same-class child used folded probe coordinate");
}

}  // namespace Stockfish::Ultimate::GhostPairInformation
