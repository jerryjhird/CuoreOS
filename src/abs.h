#pragma once

#ifndef BRANCH_PREDICTION_MANIPULATION_DISABLED // if BRANCH_PREDICTION_MANIPULATION_DISABLED isnt defined
	#define LIKELY(x)	__builtin_expect(!!(x), 1)
	#define UNLIKELY(x)  __builtin_expect(!!(x), 0)
#else // if BRANCH_PREDICTION_MANIPULATION_DISABLED is defined
	#define LIKELY(x) (x)
	#define UNLIKELY(x) (x)
#endif

#define UNUSED(x) ((void)(x))
#define ALIGN(alignment, size) (((size) + ((alignment) - 1)) & ~((alignment) - 1))
#define IP_ADDR(a, b, c, d) ((a) | (b << 8) | (c << 16) | (d << 24))
