#include <iostream>
#include <array>
#include <vector>
#include <unordered_set>
#include <regex>
#include <limits>

#include <rapidjson/filewritestream.h>
#include <rapidjson/writer.h>

#include <yaml-cpp/parser.h>
#include <yaml-cpp/eventhandler.h>

std::unordered_set<std::string> literal_true{"True", "true",
  "TRUE", "Yes", "yes", "YES", "On", "on", "ON", "y", "Y"};
std::unordered_set<std::string> literal_false{"False", "false",
  "FALSE", "No", "no", "NO", "Off", "off", "OFF", "n", "N"};

std::regex pattern_int{
  "([-+]?[0-9]+)"
  "|(0o[0-7]+)"
  "|(0x[0-9a-fA-F]+)",
  std::regex::optimize | std::regex::extended};

std::regex pattern_float{
  "([-+]?(\\.[0-9]+|[0-9]+(\\.[0-9]*)?)([eE][-+]?[0-9]+)?)",
  std::regex::optimize | std::regex::extended};

std::unordered_set<std::string> literal_NaN{".nan", ".NaN", ".NAN"};

std::unordered_set<std::string> literal_inf{".inf", ".Inf", ".INF",
  "+.inf", "+.Inf", "+.INF", "-.inf", "-.Inf", "-.INF"};

void error(const std::string &s) {
   std::cerr << "Error: " << s << "\n";
   exit(1);
}

template<typename Writer>
class adapter : public YAML::EventHandler {
public:
  Writer &w;
  size_t doc_no = 0;

  // Count the scalars in maps; 0, 2, 4 being keys and odd
  // numbers being values; -1 is array (not a map)
  struct stack_t : std::vector<ssize_t> {
    ssize_t &top() { return (*this)[size() - 1]; }
    void push(ssize_t v) { push_back(v); }
    void pop() { pop_back(); }
  } stack{};

  adapter(Writer &w) : w{w} {
    stack.reserve(1024);
    stack.push_back(-1);
  }

  void OnDocumentStart(const YAML::Mark&) {
    if (doc_no != 0) error("Multiple documents are not supported!");
    doc_no++;
  }
  void OnDocumentEnd() { /* pass */ }

  void OnNull(const YAML::Mark&, YAML::anchor_t) { w.Null(); }

  void OnAlias(const YAML::Mark&, YAML::anchor_t) {
    error("Aliases are not supported in json.");
  }
  void OnScalar(const YAML::Mark&, const std::string&,
                YAML::anchor_t, const std::string& value, bool quoted) {

    if ((stack.top() % 2) == 0)
      w.Key(value.c_str(), value.size(), true);
    else if (quoted)
      w.String(value.c_str(), value.size(), true);
    else if (literal_true.count(value))
      w.Bool(true);
    else if (literal_false.count(value))
      w.Bool(false);
    else if (literal_NaN.count(value))
      error("NaN is not supported in JSON.");
    else if (literal_inf.count(value))
      error("Infinity is not supported in JSON.");
    else if (std::regex_match(value, pattern_int))
      w.Int64(std::stoll(value));
    else if (std::regex_match(value, pattern_float))
      w.Double(std::stod(value));
    else
      w.String(value.c_str(), value.size(), true); // Duplicate/pasted

    stack.top()++;
  };

  void OnSequenceStart(const YAML::Mark&, const std::string&,
                       YAML::anchor_t, YAML::EmitterStyle::value) {
    stack.push(-1);
    w.StartArray();
  }
  void OnSequenceEnd() {
    stack.pop();
    w.EndArray();
  };

  void OnMapStart(const YAML::Mark&, const std::string&,
                  YAML::anchor_t, YAML::EmitterStyle::value) {
    stack.push(0);
    w.StartObject();
  }
  void OnMapEnd() {
    stack.pop();
    w.EndObject();
  }
};

int main(int argc, char**){
  if (argc != 1) {
    std::cerr << "USAGE: yaml2json <YAML >JSON\n";
    exit(1);
  }

  std::array<char, 102400> buf{};
  rapidjson::FileWriteStream os(stdout, &buf[0], buf.size());
  rapidjson::Writer<rapidjson::FileWriteStream> writer{os};

  adapter<decltype(writer)> a{writer};

  YAML::Parser p(std::cin);
  p.HandleNextDocument(a);

  return 0;
}
