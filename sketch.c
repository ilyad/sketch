#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

void die(char *str) {
  fprintf(stderr, "dying: %s\n", str);
  exit(1);
}

#define MAX_CELLS 1000000
uint64_t cells[MAX_CELLS];
/* start from 1, as index 0 is reserved for errors or invalid values */
uint32_t next_cell = 1;

#define CHECK_CELLS(i) do { if (next_cell + i >= MAX_CELLS) \
  die("out of cells"); } while(0)

// 4 lowest-order bits for the type
#define TYPE_MASK 15
#define TYPE(i) (cells[i] & TYPE_MASK)
#define T_NONE   0  /* this cell is unused */
#define T_INT32  1  /* immediate 32-bit value */
#define T_PAIR   2  /* pair, uses next cell */
#define T_STR    3  /* string */
#define T_SYM    4  /* symbol */
#define T_EMPTY  5  /* empty list, a special value */
#define T_BOOL   6  /* boolean */

#define SKIP_WS(str) do { while(isspace(*str)) ++str; } while(0)

/* helper rules for identifying symbols */
#define INITIAL(c) ((c>='a' && c<='z') || c=='!' || c=='$' || c=='%' || \
                   c=='&' || c=='*' || c=='/' || c==':' || c=='<' || \
                   c=='=' || c=='>' || c=='?' || c=='^' || c=='_' || c=='~')

#define SUBSEQUENT(c) (INITIAL(c) || (c>='0' && c<='9') || c=='+' || \
                       c=='-' || c=='.' || c=='@')

#define SYMBOL_NAME(i) (char *)(cells+i+1)
#define SYMBOL_LEN(i) (cells[i] >> 32)

/* helper func to store a string into cells */
uint32_t pack_string(char *str, char *end, int type) {
  uint32_t len = (end-str+7)/8;
  CHECK_CELLS(len);
  uint32_t index = next_cell;
  uint64_t value = type | (uint64_t)len << 16 | (uint64_t)(end-str) << 32;
  cells[next_cell++] = value;
  strncpy(SYMBOL_NAME(index), str, end-str);  
  next_cell+=len;
  return index;
}

int read_value(char **pstr, uint32_t *pindex, int implicit_paren) {
  char *str = *pstr, *p;
  int num, count;
  uint64_t value;
  uint32_t index = next_cell;

  SKIP_WS(str);
  if (implicit_paren || *str == '(') {
    if (!implicit_paren) ++str;
    SKIP_WS(str);
    if (*str == ')') {
      value = T_EMPTY;
      CHECK_CELLS(1);
      cells[next_cell++] = value;
      *pindex = index;
      *pstr = str;
      return 1;
    }
    uint32_t index1;
    int res = read_value(&str, &index1, 0);
    if (!res) return 0;
    
    SKIP_WS(str);
    if (*str == ')' && !implicit_paren) { /* just one value in the list */
      *pstr = str+1;
      *pindex = index1;
      return 1; 
    }

    int dot_pair = 0;
    if (*str == '.' && isspace(*(str+1))) {
      dot_pair = 1;
      str++;
    }
    uint32_t index2;
    res = read_value(&str, &index2, !dot_pair);
    if (!res) return 0;

    if (dot_pair) { /* consume the final )  */
      SKIP_WS(str);
      if (*str != ')') return 0;
      str++;
    }
    
    /* create and store the pair */
    value = T_PAIR;
    CHECK_CELLS(2);
    index = next_cell;
    cells[next_cell++] = value;
    cells[next_cell++] = ((uint64_t)index1 << 32) | index2;
    *pindex = index; *pstr = str;
    return 1;
  }
    
  if (sscanf(str, "%d%n", &num, &count) >= 1) {
    str += count;
    value = T_INT32 | ((uint64_t)num << 32);
    CHECK_CELLS(1);
    cells[next_cell++] = value;
    *pindex = index;
    *pstr = str;
    return 1;
  }
 
  if (*str == '"') {
    char *end = ++str;
    while(*end && *end != '"') ++end;
    if (*end == '\0') return 0;
    *pindex = pack_string(str, end, T_STR);
    *pstr = end+1;
    return 1;
  }


  /* is it a symbol? */
  int symbol = 0;
  char *end; /* points one past the end of the symbol */

  /* special identifiers */
  if (*str == '+' || *str == '-') {
    end = str+2; symbol = 1;
  }
  if (!symbol && strncmp(str, "...", 3) == 0) {
    end = str+4; symbol = 1;
  }

  /* regular identifiers */
  if (!symbol && INITIAL(*str)) {
    symbol = 1;
    end = str+1;
    while(SUBSEQUENT(*end)) end++;
  }

  if (symbol) {
    *pindex = pack_string(str, end, T_SYM);
    *pstr = end;
    return 1;
  }

  return 0;
}

void dump_value(uint32_t index, int implicit_paren) {
  uint32_t num, length, i;
  uint32_t index1, index2;
  char *p;
  switch(TYPE(index)) {
    case T_EMPTY: 
      printf("()"); break;
    case T_INT32:
      num = cells[index] >> 32;
      printf("%d", num);
      break;
    case T_STR:
      length = SYMBOL_LEN(index);
      p = SYMBOL_NAME(index);
      putchar('"');
      for (i = 0; i < length; i++) putchar(*p++);
      putchar('"');
      break;
    case T_SYM:
      length = SYMBOL_LEN(index);
      p = SYMBOL_NAME(index);
      for (i = 0; i < length; i++) putchar(*p++);
      break;
    case T_PAIR:
      index1 = cells[index+1] >> 32;
      index2 = cells[index+1] & 0xFFFFFFFF;
      if (!implicit_paren) putchar('(');
      dump_value(index1, 0);
      if (TYPE(index2) == T_PAIR) {
        putchar(' ');
        dump_value(index2, 1); 
      } else {
        if (TYPE(index2) != T_EMPTY) {
          printf(" . ");
          dump_value(index2, 0);
        }
        putchar(')');
      }
      break;
    default:
      break;
  }
}

#define CAR(i) (cells[i+1] >> 32)
#define CDR(i) (cells[i+1] & 0xFFFFFFFF)

uint32_t eval(uint32_t index) {
  uint32_t sym, val, car, cdr;
  switch(TYPE(index)) {
    case T_EMPTY:
    case T_INT32: 
    case T_BOOL:
    case T_STR:
      return index;
    case T_SYM:
      val = get_symbol(SYMBOL_NAME(index), SYMBOL_LEN(index));
      if (val == 0) die("undefined symbol");
      return val;
    case T_PAIR:
      car = CAR(index);
      cdr = CDR(index);
      if (TYPE(car) != T_SYM) die("car of eval'd pair is not a symbol");
      if (TYPE(cdr) != T_PAIR) die("cdr of eval'd pair is not a pair");
      
      int is_define = strncmp(SYMBOL_NAME(car), "define", 6) == 0;
      int is_set = strncmp(SYMBOL_NAME(car), "set!", 4) == 0;
      if (is_define || is_set) {
        /* the only allowed syntax here is ([define/set!] symbol value) */
        sym = CAR(cdr);
        if (TYPE(sym) != T_SYM) die ("define/set! followed by non-symbol");
        if (TYPE(CDR(cdr)) != T_PAIR) die ("define/set! symbol . smth");
        val = CAR(CDR(cdr));
        if (TYPE(CDR(CDR(cdr))) != T_EMPTY) die ("define/set! too many args");
        if (is_set && get_symbol(SYMBOL_NAME(sym), SYMBOL_LEN(sym))==0) 
          die("set! on an undefined symbol");
        set_symbol(SYMBOL_NAME(sym), SYMBOL_LEN(sym), val);
        return val; /* TODO: actually undefined value */
      }

      if (strncmp(SYMBOL_NAME(car), "quote", 5) == 0) {
        /* syntax is: (quote value) */
        val = CAR(cdr);
        if (TYPE(CDR(cdr)) != T_EMPTY) die ("bad quote syntax");
        return val;
      }

      break;
    default:
      return 0;
  }
}
      
int main(int argc, char **argv) {
  char buf[512];
  while(1) {
    printf("%d cells> ", next_cell);
    if (fgets(buf, 512, stdin) == 0) die("fgets() failed");
    char *str = buf;
    uint32_t index;
    int can_read = read_value(&str, &index, 0);
    if (can_read) { 
      uint32_t res = eval(index);
      if (res) {
        dump_value(res, 0); printf("\n");
      } else printf("eval failed.");
    }
    else printf("failed reading at: %s\n", str);
  }
  return 0;
}

