#include <stdint.h>
#include <string>
#include <tr1/unordered_map>

#include "symbols.h"

using namespace std;

typedef tr1::unordered_map<string,uint32_t> table_t;

static table_t table;

static string sym_name(const char* name, int len) {
  string str;
  str.assign(name, len);
  return str;
}

uint32_t get_symbol(const char *name, int len) {
  string str = sym_name(name, len);
  table_t::const_iterator it = table.find(str);
  return it == table.end() ? 0 : it->second;
}

void set_symbol(const char *name, int len, uint32_t val) {
  table[sym_name(name, len)] = val;
}
