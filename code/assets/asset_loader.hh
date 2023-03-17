#pragma once
#include "base/base.hh"
#include "base/string.hh"

// Initialises the deferred asset loader. Expects an OpenGL context to be set up.
void InitAssetLoader();

// Initialisation functions for asset loading subsystems. Called by InitAssetLoader.
void InitTextureLoader();
void InitShaderLoader();
void InitModelLoader();

// Processes one pending asset load operation. Returns the number of remaining operations. This is
// intended to be called in a loop, either until it returns 0 or until the total time spent on asset
// processing exceeds a per-frame maximum. The caller is responsible for tracking time.
uint32_t ProcessAssetLoadOperation();
