#pragma once
#include "base/base.hh"
#include "base/string.hh"

// Initialises the deferred asset loader. Expects an OpenGL context to be set up.
void InitAssetLoader();

// Initialisation functions for asset loading subsystems. Called by InitAssetLoader.
void InitTextureLoader();
void InitShaderLoader();
void InitModelLoader();
