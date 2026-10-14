#pragma once
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
class String {
  std::string value_;
public:
  String(const char* s):value_(s?s:""){}
  String(const std::string& s):value_(s){}
  size_t length() const {return value_.length();}
  bool isEmpty() const {return value_.empty();}
  bool operator==(const char* s)const{return value_==s;}
  char operator[](size_t i)const{return value_[i];}
  const char* c_str()const{return value_.c_str();}
};
