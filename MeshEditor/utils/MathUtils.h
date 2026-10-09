#pragma once

#include <glm/glm.hpp>

namespace Utils
{
glm::vec3 normalizedOr(const glm::vec3 &value, const glm::vec3 &fallback);
glm::mat4 rotationBetweenVectors(const glm::vec3 &from,
								 const glm::vec3 &to);
bool getRayPlaneIntersection(glm::vec3 rayOrig, glm::vec3 rayDir,
							 glm::vec3 planeOrig, glm::vec3 planeNormal,
							 glm::vec3 &intersection);
float wrapAngle(float angle);
float calcPlaneAngle(const glm::vec3 &point_W, const glm::vec3 &origin_W,
					 const glm::vec3 &basisU_W, const glm::vec3 &basisV_W);
void pickLocalRingBasis(const glm::vec3 &axis_L, glm::vec3 &basisU_L,
						glm::vec3 &basisV_L);
}
