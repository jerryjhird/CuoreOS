/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef LIMINEREQ_H
#define LIMINEREQ_H

#ifdef __cplusplus
extern "C" {
#endif

extern volatile struct limine_framebuffer_request fb_req;
extern volatile struct limine_module_request      module_request;
extern volatile struct limine_memmap_request      memmap_req;
extern volatile struct limine_hhdm_request        hhdm_req;

#ifdef __cplusplus
}
#endif

#endif // LIMINEREQ_H