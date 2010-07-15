#ifndef SYMBOL_H
#define SYMBOL_H

#include <stdint.h>

#ifdef __cplusplus
# define extern_C extern "C"
#else
# define extern_C
#endif

extern_C uint32_t get_symbol(const char *name, int len);
extern_C void set_symbol(const char *name, int len, uint32_t val);

#endif
