#include <cmath>

#include <iostream>
#include <iomanip>
#include <string>
#include <array>

#include <rapidjson/reader.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/error/en.h>

#include <yaml-cpp/yaml.h>

class adaptor {
public:
  YAML::Emitter &em;

  struct out_t {
    YAML::Emitter &em;
    
    template<typename T>
    inline out_t& operator<<(const T &v) {
      em << v;
      if (!em.good()) {
        std::cerr << "Error emitting YAML: " << em.GetLastError() << "\n";
        exit(20);
      }
      return *this;
    }

  } out{em};

  adaptor(YAML::Emitter &em) : em{em} {}
  
  bool Null() { em << YAML::Null; return true; }

  // TODO: Copy without decoding? (to get arbitrary precision)
  // TODO: Is there a toutplate version of this?
  bool Bool(bool x)       { out << x; return true; }
  bool Int(int x)         { out << x; return true; }
  bool Uint(unsigned x)   { out << x; return true; }
  bool Int64(int64_t x)   { out << x; return true; }
  bool Uint64(uint64_t x) { out << x; return true; }
  bool Double(double x)   {
    // TODO: yaml-cpp should preserve at least one decimal
    // place in round floats, to preserve the type
    if (ceil(x) == x) {
      // TODO: This is terribly inefficient; so many copies!
      std::stringstream sb;
      sb << x << ".0";
      auto s = sb.str();
      out << s;
    } else {
      out << x;
    }
    return true;
  }

  // TODO: Pretty multi line strings?
  bool String(const char* str, size_t len, bool) {
    // TODO: Get rid of the extra copy? How can we directly
    // pass a cstring with size?
    // TODO: At the moment yaml-cpp will not quote strings
    // that contain integers, hence we quote all strings. We
    // should patch yaml-cpp to quote those strings.
    out << YAML::SingleQuoted << std::string{str, len};
    return true;
  }

  bool Key(const char* str, size_t len, bool) {
    out << YAML::Key << std::string{str, len} << YAML::Value;
    return true;
  }

  bool StartObject()     { out << YAML::BeginMap; return true; }
  bool EndObject(size_t) { out << YAML::EndMap;   return true; }

  bool StartArray()     { out << YAML::BeginSeq; return true; }
  bool EndArray(size_t) { out << YAML::EndSeq;   return true; }
};

int main(int argc, char **) {
  if (argc != 1) {
    std::cerr << "USAGE: json2yaml <JSON >YAML\n";
    exit(1);
  }

  YAML::Emitter emitter{std::cout};
  adaptor adat{emitter};

  std::array<char, 102400> buf{};
  rapidjson::FileReadStream is{stdin, &buf[0], buf.size()};

  rapidjson::Reader r{};
  r.Parse(is, adat);

  if (r.HasParseError()) {
    std::cerr << "JSON Parser error at " << r.GetErrorOffset()
      << ": " << rapidjson::GetParseError_En(r.GetParseErrorCode())
      << "\n";
    exit(21);
  }

  std::cout << '\n';

  return 0;
}
