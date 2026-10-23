#pragma once
#include <bits/c++config.h>
#undef _GLIBCXX_HAS_GTHREADS
#define _GLIBCXX_GCC_GTHR_H 1
#include <bits/gthr-single.h>
