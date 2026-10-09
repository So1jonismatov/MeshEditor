#include "MathUtils.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr float kEpsilon = 1e-6f;
constexpr float kTwoPi = 6.28318530717958647692f;

glm::vec3 normalizedOrFallback(const glm::vec3 &value,
							   const glm::vec3 &fallback)
{
	const float lenSq = glm::dot(value, value);
	if (lenSq <= kEpsilon || !std::isfinite(lenSq))
		return fallback;
	return value / std::sqrt(lenSq);
}
} // namespace

namespace Utils
{

glm::vec3 normalizedOr(const glm::vec3 &value, const glm::vec3 &fallback)
{
	return normalizedOrFallback(value, fallback);
}

glm::mat4 rotationBetweenVectors(const glm::vec3 &from, const glm::vec3 &to)
{
	const glm::vec3 fromDir = normalizedOrFallback(from, glm::vec3(0.0f, 0.0f, 1.0f));
	const glm::vec3 toDir = normalizedOrFallback(to, fromDir);

	const glm::vec3 axis = glm::cross(fromDir, toDir);
	const float axisLenSq = glm::dot(axis, axis);
	if (axisLenSq > kEpsilon && std::isfinite(axisLenSq))
	{
		const float angle = std::acos(glm::clamp(glm::dot(fromDir, toDir), -1.0f,
												 1.0f));
		return glm::rotate(glm::mat4(1.0f), angle,
						   normalizedOrFallback(axis, glm::vec3(1.0f, 0.0f, 0.0f)));
	}

	if (glm::dot(fromDir, toDir) < -0.99f)
	{
		const glm::vec3 fallbackAxis = std::abs(fromDir.x) > 0.5f
										   ? glm::vec3(0.0f, 1.0f, 0.0f)
										   : glm::vec3(1.0f, 0.0f, 0.0f);
		return glm::rotate(glm::mat4(1.0f), glm::pi<float>(), fallbackAxis);
	}

	return glm::mat4(1.0f);
}

bool getRayPlaneIntersection(glm::vec3 rayOrig, glm::vec3 rayDir,
							 glm::vec3 planeOrig, glm::vec3 planeNormal,
							 glm::vec3 &intersection)
{
	const float denom = glm::dot(planeNormal, rayDir);
	if (std::abs(denom) > kEpsilon)
	{
		const float t = glm::dot(planeOrig - rayOrig, planeNormal) / denom;
		if (t >= 0.0f)
		{
			intersection = rayOrig + t * rayDir;
			return true;
		}
	}
	return false;
}

float wrapAngle(float angle)
{
	while (angle > glm::pi<float>())
		angle -= kTwoPi;
	while (angle < -glm::pi<float>())
		angle += kTwoPi;
	return angle;
}

float calcPlaneAngle(const glm::vec3 &point_W, const glm::vec3 &origin_W,
					 const glm::vec3 &basisU_W, const glm::vec3 &basisV_W)
{
	const glm::vec3 offset = point_W - origin_W;
	const float x = glm::dot(offset, basisU_W);
	const float y = glm::dot(offset, basisV_W);
	if ((x * x + y * y) <= kEpsilon)
		return 0.0f;
	return std::atan2(y, x);
}

void pickLocalRingBasis(const glm::vec3 &axis_L, glm::vec3 &basisU_L,
						glm::vec3 &basisV_L)
{
	if (std::abs(axis_L.x) > 0.5f)
	{
		basisU_L = glm::vec3(0.0f, 1.0f, 0.0f);
		basisV_L = glm::vec3(0.0f, 0.0f, 1.0f);
	}
	else if (std::abs(axis_L.y) > 0.5f)
	{
		basisU_L = glm::vec3(1.0f, 0.0f, 0.0f);
		basisV_L = glm::vec3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		basisU_L = glm::vec3(1.0f, 0.0f, 0.0f);
		basisV_L = glm::vec3(0.0f, 1.0f, 0.0f);
	}
}

}
