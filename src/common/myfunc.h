#pragma once

#ifndef _COMMON_MYFUNC_H_
#define _COMMON_MYFUNC_H_
#include<common/types.h>
#include<core/console.h>
#define DEBUG
#ifdef DEBUG
uint64_t cnt;
#endif
void _assert(int e,char* m);
#endif