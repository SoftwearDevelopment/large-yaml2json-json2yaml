#pragma once

#include <cstring>
#include <cstdlib>

#include <sstream>
#include <ios>

inline bool scmp(const char *a, size_t a_len, const std::string &b) {
  return b.size() == a_len && strncmp(a, b.c_str(), a_len) == 0;
}

// Check if the given memory segment is equal to any of the
// given strings.
template<typename B=const std::string&>
bool smulticmp(const char *a, size_t a_len, B b) {
  return scmp(a, a_len, b);
}
template<typename B=const std::string&, typename... T>
bool smulticmp(const char *a, size_t a_len, B b, T... r) {
  return scmp(a, a_len, b) || smulticmp<T...>(a, a_len, r...);
}

// Check if one character is any of the rest of the
// characters specified
template<typename B, typename... T>
bool cmulticmp(char a, B b, T... r) {
  return a == b || cmulticmp<T...>(a, r...);
}
template<> bool cmulticmp<char>(char a, char b) { return a == b; }

// Check if a memory segment starts with a string
inline bool startswith(const char *a, size_t a_len, const std::string &b) {
  if (a_len < b.size()) return false;
  else                  return strncmp(a, b.c_str(), b.size()) == 0;
}

// Check if any of the given characters is contained in the
// memory segment
// Note: works for ascii characters only
template<typename B, typename... T>
bool containsc(const char *s, size_t len, B b, T... r) {
  return memchr(s, b, len) || containsc<T...>(s, len, r...);
}
template<> bool containsc<char>(const char *s, size_t len, char b) {
  return memchr(s, b, len);
}

#define YAML_NULL_LITS  "", "~", "Null", "null", "NULL"
#define YAML_TRUE_LITS  "True", "true", "TRUE", "Yes", "yes", \
                        "YES", "On", "on", "ON", "y", "Y"
#define YAML_FALSE_LITS "False", "false", "FALSE", "No", "no", \
                        "NO", "Off", "off", "OFF", "n", "N"
#define YAML_BOOL_LITS  YAML_TRUE_LITS, YAML_FALSE_LITS
#define YAML_NAN_LITS   ".nan", ".NaN", ".NAN"
#define YAML_INF_LITS   ".inf", ".Inf", ".INF", "+.inf", "+.Inf", "+.INF", \
                        "-.inf", "-.Inf", "-.INF"
#define YAML_FLOAT_LITS YAML_NAN_LITS, YAML_INF_LITS
#define YAML_ALL_LITS   YAML_NULL_LITS, YAML_BOOL_LITS, YAML_FLOAT_LITS

inline bool yaml_scalar_is_null(const char *s, size_t len) {
  return smulticmp(s, len, YAML_NULL_LITS);
}

inline bool yaml_scalar_is_true(const char *s, size_t len) {
  return smulticmp(s, len, YAML_TRUE_LITS);
}

inline bool yaml_scalar_is_false(const char *s, size_t len) {
  return smulticmp(s, len, YAML_FALSE_LITS);
}

inline bool yaml_scalar_parse_bool(const char *s, size_t len, bool &b) {
  if      (yaml_scalar_is_true(s, len))  { b = true;  return true;  }
  else if (yaml_scalar_is_false(s, len)) { b = false; return true;  }
  else                                   {            return false; }
}

inline bool yaml_scalar_parse_int(const char *s, size_t len, int64_t &i) {
  std::stringstream ss{};
  ss.rdbuf()->pubsetbuf(const_cast<char*>(s), len);

  uint64_t k=0;
  if (startswith(s, len, "0x")) {
    ss >> std::hex >> k;
    i=k;
  } else if (startswith(s, len, "0o")) {
    ss >> std::oct >> k;
    i=k;
  } else {
    ss >> std::dec >> i;
  }

  return k <= (uint64_t)std::numeric_limits<int64_t>::max() // Fail on overflow
      && !ss.fail() && ss.tellg() == -1;
}

inline bool yaml_scalar_parse_double(const char *s, size_t len, double &d) {
  if (smulticmp(s, len, YAML_NAN_LITS)) {
    d = NAN;
    return true;
  } else if (smulticmp(s, len, YAML_INF_LITS)) {
    d = INFINITY;
    return true;
  }

  std::istringstream ss;
  ss.rdbuf()->pubsetbuf(const_cast<char*>(s), len);
  ss >> d;
  return !ss.fail() && ss.tellg() == -1;
}

// Check if the string can be pasted as a string literal
// without escaping. Texts that would be interpreted as
// booleans, numbers or would brake the syntax need to be
// escaped.
inline bool is_yaml_literal_string(const char *s, size_t len) {
  double d; int64_t i;
  return smulticmp(s, len, YAML_ALL_LITS)
      || (len>0 && cmulticmp(s[0], ' ', '|', '*', '&', '!', '\'', '"',
                                    '{', '}', '>', '-', '@', '`', '%'))
      || startswith(s, len, "---")
      || s[len-1] == ' ' // Can not end on a space
      || containsc(s, len, '\n', '\t', '#', ',')
      || yaml_scalar_parse_int(s, len, i)
      || yaml_scalar_parse_double(s, len, d);
}
