#ifndef __STDBOOL_H
#define __STDBOOL_H

// ppchibicc provides these as keywords in its default language mode. Keep
// the traditional macros for foreign C compilers using C17 or earlier.
#if !defined(__chibicc__) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ <= 201710L)
#define bool _Bool
#define true 1
#define false 0
#endif
#define __bool_true_false_are_defined 1

#endif
