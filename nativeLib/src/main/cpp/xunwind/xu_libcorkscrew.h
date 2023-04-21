// Copyright (c) 2020-2023 HexHacking Team
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Created by caikelun on 2020-10-21.

#ifndef IO_GITHUB_HEXHACKING_XUNWIND_LIBCORKSCREW
#define IO_GITHUB_HEXHACKING_XUNWIND_LIBCORKSCREW

#include "xu_config.h"
#ifdef XU_CONFIG_CFI_WITH_LIBCORKSCREW

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int xu_libcorkscrew_init(void);

void *xu_libcorkscrew_create(pid_t pid, pid_t tid);
void xu_libcorkscrew_destroy(void *self);
size_t xu_libcorkscrew_unwind(void *self, size_t num_ignore_frames, void *context);
void xu_libcorkscrew_format(void *self, size_t frame_num, char *buf, size_t buf_cap);

#ifdef __cplusplus
}
#endif

#endif

#endif
