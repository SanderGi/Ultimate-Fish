/* Ultimate Fish exact crossed Jester/Ghost arbitrary-belief sidecar. GPLv3+. */

#include "crossed_jester_ghost_information_sidecar.h"
#include "crossed_jester_ghost_information_lower_oracle.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace Stockfish::Ultimate::CrossedJesterGhostSolver {
namespace {

namespace Model = CrossedJesterGhostInformation;
constexpr std::uint32_t EmptyRoot = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint32_t HeaderBytes = 640;
constexpr std::uint32_t IndexBytes = 16;
constexpr std::array<char,8> Magic{{'U','F','C','R','O','S','S','1'}};
constexpr std::string_view Semantics = "dual-perfect-recall-forces-v1";

void require_hash(const std::string& value, const char* label) {
    if (value.size()!=64||!std::all_of(value.begin(),value.end(),[](unsigned char c){
          return(c>='0'&&c<='9')||(c>='a'&&c<='f');}))
        throw std::invalid_argument(std::string(label)+" is not SHA-256");
}

void validate_bindings(const SidecarBindings& bindings) {
    require_hash(bindings.sourceSha256,"source SHA-256");
    require_hash(bindings.modelSha256,"model SHA-256");
    require_hash(bindings.observationSha256,"observation SHA-256");
    require_hash(bindings.graphCheckpointSha256,"graph checkpoint SHA-256");
    require_hash(bindings.lowerJesterTableSha256,"lower Jester table SHA-256");
    require_hash(bindings.lowerJesterOverlaySha256,"lower Jester overlay SHA-256");
    require_hash(bindings.lowerGhostSidecarSha256,"lower Ghost sidecar SHA-256");
}

void put_u32(std::uint8_t* p,std::uint32_t value){
    for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(value>>(8*i));
}
void put_u64(std::uint8_t* p,std::uint64_t value){
    put_u32(p,static_cast<std::uint32_t>(value));
    put_u32(p+4,static_cast<std::uint32_t>(value>>32));
}
std::uint32_t get_u32(const std::uint8_t* p){
    return p[0]|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|
      (std::uint32_t(p[3])<<24);
}
std::uint64_t get_u64(const std::uint8_t* p){
    return get_u32(p)|(std::uint64_t(get_u32(p+4))<<32);
}
void write_u32(std::ostream& output,std::uint32_t value){
    std::array<std::uint8_t,4>b{};put_u32(b.data(),value);
    output.write(reinterpret_cast<const char*>(b.data()),b.size());
}
void write_u64(std::ostream& output,std::uint64_t value){
    std::array<std::uint8_t,8>b{};put_u64(b.data(),value);
    output.write(reinterpret_cast<const char*>(b.data()),b.size());
}
std::uint64_t checked_add(std::uint64_t a,std::uint64_t b){
    if(a>std::numeric_limits<std::uint64_t>::max()-b)
        throw std::overflow_error("crossed sidecar extent overflow");
    return a+b;
}
std::uint64_t checked_mul(std::uint64_t a,std::uint64_t b){
    if(a&&b>std::numeric_limits<std::uint64_t>::max()/a)
        throw std::overflow_error("crossed sidecar extent overflow");
    return a*b;
}
void sync_file(const std::string& path){
    const int fd=::open(path.c_str(),O_RDONLY);
    if(fd<0||::fsync(fd)!=0){if(fd>=0)::close(fd);throw std::runtime_error("cannot fsync crossed sidecar");}
    ::close(fd);
}

struct IndexRecord{std::uint64_t keyOffset=0;std::uint32_t keyLength=0;std::uint32_t atomBase=0;};

std::array<std::uint8_t,HeaderBytes> make_header(
  std::uint32_t nodes,std::uint64_t atoms,std::uint64_t keyBytes,
  std::uint64_t rootsOffset,std::uint64_t indexOffset,std::uint64_t keyOffset,
  std::uint64_t whiteOffset,std::uint64_t blackOffset,std::uint64_t fileBytes,
  const SidecarBindings& bindings,const std::string& payloadSha){
    std::array<std::uint8_t,HeaderBytes> h{};
    std::copy(Magic.begin(),Magic.end(),reinterpret_cast<char*>(h.data()));
    put_u32(h.data()+8,1);put_u32(h.data()+12,HeaderBytes);
    put_u32(h.data()+16,Model::RawPublicFrameCount);put_u32(h.data()+20,nodes);
    put_u32(h.data()+24,Model::RawPublicFrameCount);put_u32(h.data()+28,IndexBytes);
    put_u64(h.data()+32,atoms);put_u64(h.data()+40,keyBytes);
    put_u64(h.data()+48,rootsOffset);put_u64(h.data()+56,indexOffset);
    put_u64(h.data()+64,keyOffset);put_u64(h.data()+72,whiteOffset);
    put_u64(h.data()+80,blackOffset);put_u64(h.data()+88,fileBytes);
    const std::array<std::string,8> text{{bindings.sourceSha256,bindings.modelSha256,
      bindings.observationSha256,bindings.graphCheckpointSha256,
      bindings.lowerJesterTableSha256,bindings.lowerJesterOverlaySha256,
      bindings.lowerGhostSidecarSha256,payloadSha}};
    for(std::size_t i=0;i<text.size();++i)
        std::copy(text[i].begin(),text[i].end(),h.begin()+96+i*64);
    std::copy(Semantics.begin(),Semantics.end(),h.begin()+608);
    return h;
}

}  // namespace

SidecarCertificate write_arbitrary_sidecar(
  const std::string& path,const GraphDiscovery& graph,
  const PackedForcePlane& white,const PackedForcePlane& black,
  const SidecarBindings& bindings){
    validate_bindings(bindings);
    if(!graph.closed())throw std::invalid_argument("crossed sidecar requires a closed graph");
    if(graph.raw_begin()!=0||graph.roots().size()!=Model::RawPublicFrameCount)
        throw std::invalid_argument("crossed sidecar requires the full fresh-root domain");
    const Arena& arena=graph.arena();
    if(arena.size()!=white.certificate.nodes||arena.size()!=black.certificate.nodes||
       white.certificate.atomVariables!=black.certificate.atomVariables)
        throw std::runtime_error("crossed sidecar fixed-point dimensions disagree");
    if(arena.size()>std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("crossed sidecar node count exceeds uint32");
    const std::uint32_t nodes=static_cast<std::uint32_t>(arena.size());
    std::vector<NodeId> order(nodes);std::iota(order.begin(),order.end(),0);
    std::sort(order.begin(),order.end(),[&](NodeId a,NodeId b){
        return arena.encoded_key(a)<arena.encoded_key(b);});
    std::vector<std::uint32_t> remap(nodes),atomBase(nodes);
    std::uint64_t keyBytes=0,atoms=0;
    SidecarCertificate certificate;
    certificate.roots=graph.roots().size();certificate.nodes=nodes;
    for(std::uint32_t sorted=0;sorted<nodes;++sorted){
        const NodeId old=order[sorted];remap[old]=sorted;
        const auto&key=arena.encoded_key(old);
        if(sorted&&!(arena.encoded_key(order[sorted-1])<key))++certificate.keyOrderResidual;
        if(key.size()>std::numeric_limits<std::uint32_t>::max())throw std::overflow_error("crossed sidecar key exceeds uint32");
        if(atoms>=InformationTrue)throw std::overflow_error("crossed sidecar atom base exceeds token domain");
        atomBase[sorted]=static_cast<std::uint32_t>(atoms);
        atoms=checked_add(atoms,arena.node(old).atoms.size());
        keyBytes=checked_add(keyBytes,key.size());
    }
    if(atoms!=white.certificate.atomVariables||certificate.keyOrderResidual)
        throw std::runtime_error("crossed sidecar sorted graph residual");
    certificate.atoms=atoms;certificate.keyBytes=keyBytes;
    const std::uint64_t bitBytes=(atoms+7)/8;
    const std::uint64_t rootsOffset=HeaderBytes;
    const std::uint64_t indexOffset=checked_add(rootsOffset,checked_mul(graph.roots().size(),4));
    const std::uint64_t keyOffset=checked_add(indexOffset,checked_mul(nodes,IndexBytes));
    const std::uint64_t whiteOffset=checked_add(keyOffset,keyBytes);
    const std::uint64_t blackOffset=checked_add(whiteOffset,bitBytes);
    const std::uint64_t fileBytes=checked_add(blackOffset,bitBytes);
    const std::string temporary=path+".tmp";
    {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        std::array<char,HeaderBytes> blank{};output.write(blank.data(),blank.size());
        for(const NodeId root:graph.roots()){
            if(root==EmptyRoot)write_u32(output,EmptyRoot);
            else if(root>=nodes){++certificate.rootBoundsResidual;write_u32(output,EmptyRoot);}
            else write_u32(output,remap[root]);
        }
        std::uint64_t cursor=0;
        for(std::uint32_t sorted=0;sorted<nodes;++sorted){
            const auto&key=arena.encoded_key(order[sorted]);
            write_u64(output,cursor);write_u32(output,static_cast<std::uint32_t>(key.size()));
            write_u32(output,atomBase[sorted]);cursor+=key.size();
        }
        for(const NodeId old:order){const auto&key=arena.encoded_key(old);
            output.write(reinterpret_cast<const char*>(key.data()),key.size());}
        const auto bits=[&](const PackedForcePlane& solution,bool checkDual){
            std::array<std::uint8_t,1<<20>buffer{};std::size_t used=0;std::uint8_t byte=0;unsigned bit=0;
            const auto emit=[&](std::uint8_t value){buffer[used++]=value;if(used==buffer.size()){output.write(reinterpret_cast<const char*>(buffer.data()),used);used=0;}};
            for(const NodeId old:order){const auto state=arena.node(old);
                for(std::uint32_t atom=0;atom<state.atoms.size();++atom){
                    const bool value=solution.value(old,atom);
                    if(checkDual&&value&&black.value(old,atom))++certificate.dualForceResidual;
                    if(value)byte|=std::uint8_t(1u<<bit);
                    if(++bit==8){emit(byte);byte=0;bit=0;}
                }}
            if(bit)emit(byte);if(used)output.write(reinterpret_cast<const char*>(buffer.data()),used);
        };
        bits(white,true);bits(black,false);
        output.flush();if(!output)throw std::runtime_error("failed writing crossed sidecar payload");
    }
    if(certificate.rootBoundsResidual||certificate.dualForceResidual)
        throw std::runtime_error("crossed sidecar force/root certificate residual");
    const std::string payload=authenticated_file_range_sha256(
      temporary,HeaderBytes,fileBytes-HeaderBytes);
    const auto header=make_header(nodes,atoms,keyBytes,rootsOffset,indexOffset,keyOffset,
      whiteOffset,blackOffset,fileBytes,bindings,payload);
    {
        std::fstream output(temporary,std::ios::binary|std::ios::in|std::ios::out);
        output.write(reinterpret_cast<const char*>(header.data()),header.size());
        output.flush();if(!output)throw std::runtime_error("failed binding crossed sidecar header");
    }
    sync_file(temporary);
    if(std::rename(temporary.c_str(),path.c_str())!=0)
        throw std::runtime_error("cannot install crossed sidecar: "+std::string(std::strerror(errno)));
    sync_file(path);
    certificate.payloadBytes=fileBytes-HeaderBytes;certificate.payloadSha256=payload;
    certificate.fileSha256=authenticated_file_sha256(path);
    return certificate;
}

class CrossedSidecarProbe::Impl {
   public:
    Impl(const std::string& path,const std::string& expectedFile,
         const SidecarBindings& expected){
        validate_bindings(expected);require_hash(expectedFile,"sidecar full SHA-256");
        if(authenticated_file_sha256(path)!=expectedFile)throw std::runtime_error("crossed sidecar full SHA mismatch");
        fd_=::open(path.c_str(),O_RDONLY);if(fd_<0)throw std::runtime_error("cannot open crossed sidecar");
        struct stat status{};if(::fstat(fd_,&status)!=0||status.st_size<HeaderBytes)throw std::runtime_error("cannot stat crossed sidecar");
        bytes_=static_cast<std::uint64_t>(status.st_size);mapping_=static_cast<const std::uint8_t*>(::mmap(nullptr,bytes_,PROT_READ,MAP_SHARED,fd_,0));
        if(mapping_==MAP_FAILED){mapping_=nullptr;throw std::runtime_error("cannot mmap crossed sidecar");}
        if(!std::equal(Magic.begin(),Magic.end(),reinterpret_cast<const char*>(mapping_))||
           get_u32(mapping_+8)!=1||get_u32(mapping_+12)!=HeaderBytes||
           get_u32(mapping_+16)!=Model::RawPublicFrameCount||get_u32(mapping_+24)!=Model::RawPublicFrameCount||
           get_u32(mapping_+28)!=IndexBytes||get_u64(mapping_+88)!=bytes_)
            throw std::runtime_error("crossed sidecar header mismatch");
        nodes_=get_u32(mapping_+20);atoms_=get_u64(mapping_+32);keyBytes_=get_u64(mapping_+40);
        rootsOffset_=get_u64(mapping_+48);indexOffset_=get_u64(mapping_+56);keyOffset_=get_u64(mapping_+64);
        whiteOffset_=get_u64(mapping_+72);blackOffset_=get_u64(mapping_+80);
        const std::array<std::string,8> text{{expected.sourceSha256,expected.modelSha256,
          expected.observationSha256,expected.graphCheckpointSha256,
          expected.lowerJesterTableSha256,expected.lowerJesterOverlaySha256,
          expected.lowerGhostSidecarSha256,std::string(reinterpret_cast<const char*>(mapping_+544),64)}};
        for(std::size_t i=0;i<7;++i)if(std::string(reinterpret_cast<const char*>(mapping_+96+i*64),64)!=text[i])throw std::runtime_error("crossed sidecar provenance mismatch");
        if(std::string(reinterpret_cast<const char*>(mapping_+608),Semantics.size())!=Semantics)
            throw std::runtime_error("crossed sidecar semantics mismatch");
        const std::uint64_t bitBytes=(atoms_+7)/8;
        if(rootsOffset_!=HeaderBytes||indexOffset_!=rootsOffset_+std::uint64_t(Model::RawPublicFrameCount)*4||
           keyOffset_!=indexOffset_+std::uint64_t(nodes_)*IndexBytes||whiteOffset_!=keyOffset_+keyBytes_||
           blackOffset_!=whiteOffset_+bitBytes||bytes_!=blackOffset_+bitBytes)
            throw std::runtime_error("crossed sidecar extent mismatch");
        const std::string payload(reinterpret_cast<const char*>(mapping_+544),64);
        if(authenticated_file_range_sha256(path,HeaderBytes,bytes_-HeaderBytes)!=payload)
            throw std::runtime_error("crossed sidecar payload SHA mismatch");
        std::uint64_t keyCursor=0,atomCursor=0;std::vector<std::uint8_t> previous;
        for(std::uint32_t id=0;id<nodes_;++id){const IndexRecord record=index(id);
            if(record.keyOffset!=keyCursor||record.atomBase!=atomCursor||record.keyLength>keyBytes_-keyCursor)
                throw std::runtime_error("crossed sidecar index residual");
            std::vector<std::uint8_t> key(mapping_+keyOffset_+keyCursor,mapping_+keyOffset_+keyCursor+record.keyLength);
            if(id&&!(previous<key))throw std::runtime_error("crossed sidecar key order residual");
            const Model::KnowledgeState state=Model::deserialize_state(key);
            if(Model::serialize_state(Model::canonicalize_state(state).value)!=key)
                throw std::runtime_error("crossed sidecar key canonical residual");
            atomCursor+=state.atoms.size();keyCursor+=record.keyLength;previous=std::move(key);
        }
        if(keyCursor!=keyBytes_||atomCursor!=atoms_)throw std::runtime_error("crossed sidecar conservation residual");
        for(std::uint32_t raw=0;raw<Model::RawPublicFrameCount;++raw){const auto root=get_u32(mapping_+rootsOffset_+std::uint64_t(raw)*4);if(root!=EmptyRoot&&root>=nodes_)++certificate_.rootBoundsResidual;}
        for(std::uint64_t atom=0;atom<atoms_;++atom)if(bit(whiteOffset_,atom)&&bit(blackOffset_,atom))++certificate_.dualForceResidual;
        if(certificate_.rootBoundsResidual||certificate_.dualForceResidual)throw std::runtime_error("crossed sidecar root/dual residual");
        certificate_.roots=Model::RawPublicFrameCount;certificate_.nodes=nodes_;certificate_.atoms=atoms_;
        certificate_.keyBytes=keyBytes_;certificate_.payloadBytes=bytes_-HeaderBytes;
        certificate_.payloadSha256=payload;certificate_.fileSha256=expectedFile;
    }
    ~Impl(){if(mapping_)::munmap(const_cast<std::uint8_t*>(mapping_),bytes_);if(fd_>=0)::close(fd_);}
    IndexRecord index(std::uint32_t id)const{if(id>=nodes_)throw std::out_of_range("crossed sidecar node");const auto*p=mapping_+indexOffset_+std::uint64_t(id)*IndexBytes;return{get_u64(p),get_u32(p+8),get_u32(p+12)};}
    bool bit(std::uint64_t offset,std::uint64_t atom)const{return(mapping_[offset+atom/8]>>(atom%8))&1u;}
    std::uint32_t find(const std::vector<std::uint8_t>& key)const{
        std::uint32_t low=0,high=nodes_;
        while(low<high){const std::uint32_t mid=low+(high-low)/2;const IndexRecord r=index(mid);const auto*begin=mapping_+keyOffset_+r.keyOffset;const int cmp=std::lexicographical_compare(begin,begin+r.keyLength,key.begin(),key.end())?-1:std::lexicographical_compare(key.begin(),key.end(),begin,begin+r.keyLength)?1:0;if(cmp<0)low=mid+1;else high=mid;}
        if(low>=nodes_)throw std::out_of_range("crossed arbitrary belief absent from solved graph");const IndexRecord r=index(low);if(r.keyLength!=key.size()||!std::equal(key.begin(),key.end(),mapping_+keyOffset_+r.keyOffset))throw std::out_of_range("crossed arbitrary belief absent from solved graph");return low;
    }
    bool force(const Model::KnowledgeState& state,std::uint32_t actual,Color target)const{
        if(target!=Color::White&&target!=Color::Black)throw std::invalid_argument("crossed force target");
        Model::validate_knowledge_state(state);if(actual>=state.atoms.size())throw std::out_of_range("crossed actual atom");
        const Model::KnowledgeState canonical=Model::canonicalize_state(state).value;const auto key=Model::serialize_state(canonical);const std::uint32_t id=find(key);const IndexRecord r=index(id);if(actual>=canonical.atoms.size())throw std::runtime_error("crossed canonical atom residual");return bit(target==Color::White?whiteOffset_:blackOffset_,std::uint64_t(r.atomBase)+actual);
    }
    std::uint32_t root(std::uint32_t raw)const{if(raw>=Model::RawPublicFrameCount)throw std::out_of_range("crossed raw frame");return get_u32(mapping_+rootsOffset_+std::uint64_t(raw)*4);}
    int fd_=-1;const std::uint8_t*mapping_=nullptr;std::uint64_t bytes_=0;
    std::uint32_t nodes_=0;std::uint64_t atoms_=0,keyBytes_=0,rootsOffset_=0,indexOffset_=0,keyOffset_=0,whiteOffset_=0,blackOffset_=0;
    SidecarCertificate certificate_;
};

CrossedSidecarProbe::CrossedSidecarProbe(const std::string&path,const std::string&sha,const SidecarBindings&bindings):impl_(std::make_unique<Impl>(path,sha,bindings)){}
CrossedSidecarProbe::~CrossedSidecarProbe()=default;
CrossedSidecarProbe::CrossedSidecarProbe(CrossedSidecarProbe&&)noexcept=default;
CrossedSidecarProbe&CrossedSidecarProbe::operator=(CrossedSidecarProbe&&)noexcept=default;
bool CrossedSidecarProbe::force(const Model::KnowledgeState&state,std::uint32_t atom,Color target)const{return impl_->force(state,atom,target);}
std::uint32_t CrossedSidecarProbe::fresh_root(std::uint32_t raw)const{return impl_->root(raw);}
const SidecarCertificate&CrossedSidecarProbe::certificate()const{return impl_->certificate_;}

void arbitrary_sidecar_format_self_test(const std::string& path){
    SidecarBindings bindings{std::string(64,'1'),std::string(64,'2'),
      std::string(64,'3'),std::string(64,'4'),std::string(64,'5'),
      std::string(64,'6'),std::string(64,'7')};
    std::uint32_t raw=0;std::optional<Model::KnowledgeState> state;
    for(;raw<Model::RawPublicFrameCount&&!state;++raw)
        state=Model::admitted_fresh_state(Model::decode_public_frame(raw));
    if(!state)throw std::runtime_error("crossed sidecar self-test found no admitted root");
    --raw;*state=Model::canonicalize_state(*state).value;
    const auto key=Model::serialize_state(*state);const std::uint64_t atoms=state->atoms.size();
    const std::uint64_t rootsOffset=HeaderBytes;
    const std::uint64_t indexOffset=rootsOffset+std::uint64_t(Model::RawPublicFrameCount)*4;
    const std::uint64_t keyOffset=indexOffset+IndexBytes;
    const std::uint64_t whiteOffset=keyOffset+key.size();const std::uint64_t bitBytes=(atoms+7)/8;
    const std::uint64_t blackOffset=whiteOffset+bitBytes,fileBytes=blackOffset+bitBytes;
    {
        std::ofstream output(path,std::ios::binary|std::ios::trunc);
        std::array<char,HeaderBytes>blank{};output.write(blank.data(),blank.size());
        std::array<std::uint8_t,1<<20> roots{};roots.fill(0xff);
        std::uint64_t remaining=std::uint64_t(Model::RawPublicFrameCount)*4;
        while(remaining){const std::size_t take=static_cast<std::size_t>(std::min<std::uint64_t>(remaining,roots.size()));output.write(reinterpret_cast<const char*>(roots.data()),take);remaining-=take;}
        output.seekp(static_cast<std::streamoff>(rootsOffset+std::uint64_t(raw)*4));write_u32(output,0);
        output.seekp(static_cast<std::streamoff>(indexOffset));write_u64(output,0);write_u32(output,key.size());write_u32(output,0);
        output.write(reinterpret_cast<const char*>(key.data()),key.size());
        std::vector<std::uint8_t> white(bitBytes),black(bitBytes);white[0]=1;
        output.write(reinterpret_cast<const char*>(white.data()),white.size());output.write(reinterpret_cast<const char*>(black.data()),black.size());
        output.flush();if(!output)throw std::runtime_error("crossed sidecar self-test write failed");
    }
    const std::string payload=authenticated_file_range_sha256(path,HeaderBytes,fileBytes-HeaderBytes);
    const auto header=make_header(1,atoms,key.size(),rootsOffset,indexOffset,keyOffset,whiteOffset,blackOffset,fileBytes,bindings,payload);
    {std::fstream output(path,std::ios::binary|std::ios::in|std::ios::out);output.write(reinterpret_cast<const char*>(header.data()),header.size());}
    const std::string full=authenticated_file_sha256(path);
    {
        CrossedSidecarProbe probe(path,full,bindings);
        if(!probe.force(*state,0,Color::White)||probe.force(*state,0,Color::Black)||
           probe.fresh_root(raw)!=0||probe.certificate().dualForceResidual)
            throw std::runtime_error("crossed sidecar self-test query residual");
    }
    {std::fstream output(path,std::ios::binary|std::ios::in|std::ios::out);
        output.seekg(static_cast<std::streamoff>(whiteOffset));const int value=output.get();
        output.seekp(static_cast<std::streamoff>(whiteOffset));output.put(static_cast<char>(value^1));}
    bool rejected=false;
    try{CrossedSidecarProbe corrupt(path,full,bindings);(void)corrupt;}
    catch(const std::runtime_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("crossed sidecar self-test accepted corruption");
    std::remove(path.c_str());
}

}  // namespace Stockfish::Ultimate::CrossedJesterGhostSolver
