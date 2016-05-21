#include <cmath>

#include <iostream>
#include <array>
#include <vector>
#include <regex>

#include <rapidjson/filewritestream.h>
#include <rapidjson/writer.h>

#include <yaml.h>

#include "./yaml_scalar_parse.hpp"

namespace yaml {

struct event : yaml_event_t {
  ~event() { yaml_event_delete(this); }
};

struct parser_exception : std::exception {
  const char* what() const noexcept {
    return "The libyaml parser had some error";
  }
};

struct unknown_parser_event : parser_exception {
  const char* what() const noexcept {
    return "The libyaml parser emitted an even of unknown type";
  }
};

struct parser : yaml_parser_t {
  parser()  {
    yaml_parser_initialize(this);
    yaml_parser_set_encoding(this, YAML_UTF8_ENCODING);
  }

  ~parser() { yaml_parser_delete(this); }

  parser(std::istream &s) : parser{} {
    yaml_parser_set_input(this,
      [](void *s_, yaml_char_t *buf, size_t buf_s, size_t *red) -> int {
        std::istream &s = *((std::istream*)s_);
        s.read((char*)buf, buf_s);
        *red = s.gcount();
        return true;
      }, &s);
  }

  event next_event() {
    event ev;
    if (!yaml_parser_parse(this, &ev)) throw parser_exception{};
    return ev;
  }

  template<typename Visitor>
  void parse(Visitor &v) {
    while (true) {
      auto ev = next_event();
      switch (ev.type) {
        case YAML_NO_EVENT:           break; // pass
        case YAML_STREAM_START_EVENT: break;

        case YAML_DOCUMENT_START_EVENT: v.document_start(); break;
        case YAML_DOCUMENT_END_EVENT:   v.document_end();   break;
        case YAML_ALIAS_EVENT:          v.alias();          break;
        case YAML_SEQUENCE_START_EVENT: v.sequence_start(); break;
        case YAML_SEQUENCE_END_EVENT:   v.sequence_end();   break;
        case YAML_MAPPING_START_EVENT:  v.mapping_start();  break;
        case YAML_MAPPING_END_EVENT:    v.mapping_end();    break;

        case YAML_SCALAR_EVENT:
          v.scalar((char*)ev.data.scalar.value, ev.data.scalar.length,
                   ev.data.scalar.style);
          break;

        case YAML_STREAM_END_EVENT:  return; // quit

        default: throw unknown_parser_event{};
      }
    }
  }
};

}

void error(const std::string &s) {
   std::cerr << "Error: " << s << "\n";
   exit(1);
}

template<typename Writer>
struct adapter {
  Writer &w;

  int docno=0;

  // Count the scalars in maps; 0, 2, 4 being keys and odd
  // numbers being values; -1 is array (not a map)
  struct stack_t : std::vector<ssize_t> {
    ssize_t &top() { return (*this)[size() - 1]; }
    void push(ssize_t v) { push_back(v); }
    void pop() { pop_back(); }
  } stack{};

  adapter(Writer &w) : w{w} {
    stack.reserve(1024);
    stack.push(-1);
  }

  void document_start() {
    if (docno > 0) error("Multiple documents in a stream are unsupported");
    docno++;
  }
  void document_end()   { /* pass */ }
  void alias()          { error("Aliases are unsupported"); }
  void sequence_start() { w.StartArray();  stack.push(-1); }
  void sequence_end()   { w.EndArray();    stack.pop(); }
  void mapping_start()  { w.StartObject(); stack.push(0); }
  void mapping_end()    { w.EndObject();   stack.pop(); }

  void scalar(const char *s, size_t len, yaml_scalar_style_t style) {
    // We keep track of whether we are inserting a key or
    // a value, so we can skip the check below for values
    bool in_obj = stack.top() != -1;
    bool next_key = in_obj && (stack.top() % 2) == 0;
    if (in_obj) stack.top()++;

    bool b; int64_t i; double d;

    if(next_key || style != YAML_PLAIN_SCALAR_STYLE) {
      w.String(s, len); // Explicit strings and keys are just strings
    } else if (yaml_scalar_is_null(s, len)) {
      w.Null();
    } else if (yaml_scalar_parse_bool(s, len, b)) {
      w.Bool(b);
    } else if (yaml_scalar_parse_int(s, len, i)) {
      w.Int64(i);
    } else if (yaml_scalar_parse_double(s, len, d)) {
      if (std::isinf(d)) error("Infinity floats are not supported");
      if (std::isnan(d)) error("NaN floats are not supported");
      w.Double(d);
    } else {
      w.String(s, len); // Fall back to string, when the literal is nothing else
    }
  }
};

int main(int argc, char**){
  if (argc != 1) {
    std::cerr << "USAGE: yaml2json <YAML >JSON\n";
    exit(1);
  }

  std::array<char, 102400> buf{};
  rapidjson::FileWriteStream os(stdout, &buf[0], buf.size());
  rapidjson::Writer<rapidjson::FileWriteStream> w{os};

  adapter<decltype(w)> adat{w};
  yaml::parser p{std::cin};
  p.parse(adat);

  return 0;
}
