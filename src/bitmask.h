#pragma once

#define BIT_SET(var, mask)   ((var) |=  (mask))
#define BIT_CLEAR(var, mask) ((var) &= ~(mask))
#define BIT_CHECK(var, mask) (((var) & (mask)) == (mask))
#define BIT_TOGGLE(var, mask)((var) ^=  (mask))
