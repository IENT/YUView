
#pragma once

#include <string>

namespace parser
{

class SubByteReaderDummy
{
public:
  SubByteReaderDummy() = default;

  bool byte_aligned() const { return false; }

  bool        readFlag(const std::string &name) { return false; }
  int         readBits(const std::string &name, const int numBits) { return 0; }
  int         readBitsSigned(const std::string &name, const int numBits) { return 0; }
  unsigned    readUEV(const std::string &name) { return 0; }
  int         readSEV(const std::string &name) { return 0; }
  std::string readString(const std::string &name) { return ""; }
};

} // namespace parser
