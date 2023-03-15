#pragma once

/***************************************************************************************************
 * Includes and helper functions for maths
 **************************************************************************************************/

#define _USE_MATH_DEFINES
#undef M_PI // SDL and MSVCRT definitions conflict
#include <math.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "base/base.hh"

using glm::vec2; using glm::vec3; using glm::vec4;
using glm::mat2; using glm::mat3; using glm::mat4;
using glm::quat;

template <typename T> FORCEINLINE constexpr T ToRadians (T degrees) { return degrees * T(M_PI) / T(180); }
template <typename T> FORCEINLINE constexpr T ToDegrees (T radians) { return radians * T(180) / T(M_PI); }

constexpr vec3 UPVECTOR = {0, 1, 0};
