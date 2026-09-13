// Full MaleCNS fixed circuit runtime; independent implementation, GPLv3+.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <stdexcept>
static std::vector<uint32_t> offsets,targets,inputs,channels,outputs;
static std::vector<float> weights,activity,drive,next,readout;
static uint32_t count=0;
static std::vector<std::vector<int>> frames;
extern "C" {
int brain_load(const char* path) {
 try {
  std::ifstream file(path,std::ios::binary);uint32_t header[5];file.read((char*)header,sizeof(header));
  if(!file || header[0]!=0x55464c59 || header[1]<160000 || header[1]>220000 || header[2]>40000000)return -1;
  count=header[1];offsets.resize(count+1);targets.resize(header[2]);weights.resize(header[2]);inputs.resize(header[3]);channels.resize(header[3]);outputs.resize(header[4]);
  auto read=[&](auto& v){file.read((char*)v.data(),v.size()*sizeof(v[0]));};
  read(offsets);read(targets);read(weights);read(inputs);read(channels);read(outputs);
  if(!file || offsets.back()!=targets.size())return -1;
  activity.resize(count);drive.resize(count);next.resize(count);readout.resize(outputs.size());
  return int(outputs.size());
 }catch(...){return -1;}
}
// All retained cells and edges participate on each of eight synchronous iterations.
// Smooth saturating rate nonlinearity; dimensionless, not measured spikes.
const float* brain_encode(const float* features) {
 frames.clear();
 std::fill(activity.begin(),activity.end(),0);std::fill(drive.begin(),drive.end(),0);
 for(size_t i=0;i<inputs.size();i++)drive[inputs[i]]=channels[i]==0?features[0]:2*features[channels[i]]-1;
 for(int t=0;t<8;t++) {
  std::copy(drive.begin(),drive.end(),next.begin());
  for(uint32_t a=0;a<count;a++) {
   const float signal=1.4f*activity[a];
   if(signal==0)continue;
   for(uint32_t e=offsets[a];e<offsets[a+1];e++)next[targets[e]]+=weights[e]*signal;
  }
  for(uint32_t i=0;i<count;i++)activity[i]=0.3f*activity[i]+0.7f*next[i]/(1.0f+std::abs(next[i]));
  std::vector<int> frame;for(uint32_t i=0;i<count;i+=8)frame.push_back(int(std::abs(activity[i])*255));frames.push_back(std::move(frame));
 }
 for(size_t i=0;i<outputs.size();i++)readout[i]=activity[outputs[i]];
 return readout.data();
}
const float* brain_activity(){return activity.data();}
int brain_count(){return int(count);}
}
#ifdef FLY_BRAIN_CLI
int main(int argc,char** argv) {
 if(argc!=2 || brain_load(argv[1])<0)return 1;
 std::cout<<std::setprecision(9);
 std::cout<<"ready "<<outputs.size()<<std::endl;
 int n;
 while(std::cin>>n) {
  if(n==0) {
   std::cout<<"{\"frames\":[";
   for(size_t t=0;t<frames.size();t++){if(t)std::cout<<',';std::cout<<'[';for(size_t i=0;i<frames[t].size();i++){if(i)std::cout<<',';std::cout<<frames[t][i];}std::cout<<']';}
   int active=0;for(float v:activity)if(std::abs(v)>0.02f)active++;
   std::cout<<"],\"active\":"<<active<<"}"<<std::endl;continue;
  }
  if(n<1 || n>256)return 2;
  std::vector<std::vector<float>> rows(n,std::vector<float>(8));
  for(auto& f:rows)for(auto& v:f)if(!(std::cin>>v)||!std::isfinite(v)||v< -1||v>1)return 3;
  std::cout<<'[';
  for(int j=0;j<n;j++) {
   brain_encode(rows[j].data());if(j)std::cout<<',';std::cout<<'[';
   for(size_t k=0;k<outputs.size();k++){if(k)std::cout<<',';std::cout<<readout[k];}std::cout<<']';
  }
  std::cout<<']'<<std::endl;
 }
}
#endif
