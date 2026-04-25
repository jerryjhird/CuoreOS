#pragma once

#ifndef BRANCH_PREDICTION_MANIPULATION_DISABLED // if BRANCH_PREDICTION_MANIPULATION_DISABLED isnt defined
	#define LIKELY(x)	__builtin_expect(!!(x), 1)
	#define UNLIKELY(x)  __builtin_expect(!!(x), 0)
#else // if BRANCH_PREDICTION_MANIPULATION_DISABLED is defined
	#define LIKELY(x) (x)
	#define UNLIKELY(x) (x)
#endif
