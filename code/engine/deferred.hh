#pragma once
#include "base/base.hh"

struct Engine;
typedef void (*DeferredCallback)(Engine& engine, void* data);

// Defer an action to be run at the end of the frame.
void Defer(DeferredCallback callback, void* data);

// Run an action queued up using Defer. Returns the number of remaining actions. This is intended
// to be called in a loop, either until it returns 0 or until the total time spent exceeds a
// per-frame maximum. The caller is responsible for tracking time.
uint32_t RunDeferredAction(Engine& engine);
