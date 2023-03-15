#pragma once
#include <assert.h>
#include <loguru.hpp>
#include "base/base.hh"

#define Panic(...) do { LOG_F(FATAL, __VA_ARGS__); Unreachable(); } while(0)

void InitDebugSystem(int argc, char* argv[]);
