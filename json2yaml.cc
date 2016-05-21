#include <cmath>

#include <iostream>
#include <iomanip>
#include <string>
#include <array>
#include <regex>
#include <vector>
#include <exception>
#include <limits>

#include <rapidjson/reader.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/error/en.h>

#include <yaml.h>

#include "./yaml_scalar_parse.hpp"

namespace yaml {

namespace event {

struct event {
  yaml_event_t inner;
  // TODO: Why does this give me a double free?
  // ~event() { yaml_event_delete(&inner); }
};

struct stream_start : event {
  stream_start(yaml_encoding_t encoding=YAML_UTF8_ENCODING) {
    yaml_stream_start_event_initialize(&inner, encoding);
  }
};

struct stream_end : event {
  stream_end() { yaml_stream_end_event_initialize(&inner); }
};

struct document_start : event {
  document_start() {
    yaml_document_start_event_initialize(&inner, nullptr, nullptr,
                                         nullptr, true);
  };
};

struct document_end : event {
  document_end() {
    yaml_document_end_event_initialize(&inner, true);
  };
};

struct scalar : event {
  scalar(const char *s, size_t size,
         yaml_scalar_style_t style=YAML_ANY_SCALAR_STYLE) {
    yaml_scalar_event_initialize(&inner, nullptr, nullptr,
        (yaml_char_t*)s, size, true, true, style);
  }
};

struct sequence_start : event {
  sequence_start(yaml_sequence_style_t style=YAML_ANY_SEQUENCE_STYLE) {
    yaml_sequence_start_event_initialize(&inner, nullptr, nullptr, true, style);
  }
};

struct sequence_end : event {
  sequence_end() { yaml_sequence_end_event_initialize(&inner); }
};

struct mapping_start : event {
  mapping_start(yaml_mapping_style_t style=YAML_ANY_MAPPING_STYLE) {
    yaml_mapping_start_event_initialize(&inner, nullptr, nullptr, true, style);
  }
};

struct mapping_end : event {
  mapping_end() { yaml_mapping_end_event_initialize(&inner); }
};

}

struct emitter_exception : std::exception {
  const char* what() const noexcept { return "Yaml emitter threw exception"; }
};

struct emitter : yaml_emitter_t {
  emitter(yaml_encoding_t encoding=YAML_UTF8_ENCODING) {
    yaml_emitter_initialize(this);
    yaml_emitter_set_width(this, std::numeric_limits<int>::max()-1);
    yaml_emitter_set_unicode(this, true);
    yaml_emitter_set_break(this, YAML_LN_BREAK);
    //yaml_emitter_set_canonical(this, true);

    emit(yaml::event::stream_start{encoding});
  }

  emitter(std::ostream &s, yaml_encoding_t encoding=YAML_UTF8_ENCODING)
      : emitter{encoding} {
    yaml_emitter_set_output(this,
      [](void *s_, yaml_char_t *buf, size_t size) -> int {
        ((std::ostream*)s_)->write((const char*)buf, size);
        return true;
      }, &s);
  }

  ~emitter() {
    emit(yaml::event::stream_end{});
    yaml_emitter_delete(this);
  }

  bool emit(yaml_event_t &ev) {
    if (!yaml_emitter_emit(this, &ev)) throw emitter_exception{};
    return true;
  }

  bool emit(event::event &ev) {
    return emit(ev.inner);
  }

  bool emit(event::event &&ev_) {
    event::event ev{std::move(ev_)};
    return emit(ev);
  }
};

}

class adapter {
public:
  yaml::emitter &em;

  std::array<char, 256> strbuf;
  std::ostringstream ss;

  adapter(yaml::emitter &em) : em{em} {
    em.emit(yaml::event::document_start{});
    ss.rdbuf()->pubsetbuf(&strbuf[0], strbuf.size());
  }

  ~adapter() {
    em.emit(yaml::event::document_end{});
  }

  bool plain(const char *s, size_t size) {
    return em.emit(yaml::event::scalar{s, size, YAML_PLAIN_SCALAR_STYLE});
  }

  bool plain(const std::string &s) {
    return plain(s.c_str(), s.size());
  }

  template<typename T>
  bool cast(T v) {
    ss.seekp(0);
    ss << v;
    return plain(&strbuf[0], ss.tellp());
  }

  bool Null() { return plain("~"); }

  // TODO: Copy without decoding? (to get arbitrary precision)
  // TODO: Is there a template version of this?
  bool Bool(bool x)       { return plain(x ? "true" : "false"); }
  bool Int(int x)         { return cast(x); }
  bool Uint(unsigned x)   { return cast(x); }
  bool Int64(int64_t x)   { return cast(x); }
  bool Uint64(uint64_t x) { return cast(x); }
  bool Double(double x)   {
    ss.seekp(0);
    ss << x;
    if (ceil(x) == x) ss << ".0";
    return plain(&strbuf[0], ss.tellp());
  }

  // TODO: Pretty multi line strings?
  bool String(const char* str, size_t len, bool=false) {
    auto style = is_yaml_literal_string(str, len) \
        ? YAML_DOUBLE_QUOTED_SCALAR_STYLE
        : YAML_PLAIN_SCALAR_STYLE;
    return em.emit(yaml::event::scalar{str, len, style});
  }

  bool Key(const char* str, size_t len, bool) { return String(str, len); }

  bool StartObject()     { return em.emit(yaml::event::mapping_start{}); }
  bool EndObject(size_t) { return em.emit(yaml::event::mapping_end{}); }

  bool StartArray()     { return em.emit(yaml::event::sequence_start{}); }
  bool EndArray(size_t) { return em.emit(yaml::event::sequence_end{}); }
};

int main(int argc, char **) {
  if (argc != 1) {
    std::cerr << "USAGE: json2yaml <JSON >YAML\n";
    exit(1);
  }

  yaml::emitter em{std::cout};
  adapter adat{em};

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
