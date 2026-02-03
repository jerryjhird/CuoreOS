/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef HELPERS_H
#define HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#define INFO_LOG_STR  "\x1b[38;2;72;185;215m[ INFO ]\x1b[0m"
#define TIME_LOG_STR  "\x1b[38;2;72;185;215m[ TIME ]\x1b[0m"
#define PASS_LOG_STR  "\x1b[38;2;150;217;192m[ PASS ]\x1b[0m"
#define FAIL_LOG_STR  "\x1b[38;2;217;150;150m[ FAIL ]\x1b[0m"
#define WARN_LOG_STR  "\x1b[38;2;211;90;17m[ WARN ]\x1b[0m"

#ifdef __cplusplus
}
#endif

#endif // HELPERS_H