#pragma once
#include <map>
#include <vector>
#include <string>
#include <cstring>
class Preferences {
  static inline std::map<std::string,std::vector<unsigned char>> memory_;
  static inline size_t writes_=0;
  std::string ns_;
public:
  static void resetWriteCount(){writes_=0;}
  static size_t writeCount(){return writes_;}
  bool begin(const char* ns,bool =false){ns_=ns;return true;}
  void end(){}
  size_t getBytesLength(const char* key){return memory_[ns_+":"+key].size();}
  size_t getBytes(const char* key,void* data,size_t n){
    const auto& bytes=memory_[ns_+":"+key];
    if(bytes.size()!=n)return 0;
    std::memcpy(data,bytes.data(),n);return n;
  }
  size_t putBytes(const char* key,const void* ptr,size_t n){
    const auto* p=reinterpret_cast<const unsigned char*>(ptr);
    memory_[ns_+":"+key]=std::vector<unsigned char>(p,p+n);++writes_;return n;
  }
  bool getBool(const char* key,bool fallback=false){
    auto it=memory_.find(ns_+":"+key);
    return it!=memory_.end()&&!it->second.empty()?(it->second[0]!=0):fallback;
  }
  bool putBool(const char* key,bool value){
    memory_[ns_+":"+key]={static_cast<unsigned char>(value)};++writes_;return true;
  }
};
