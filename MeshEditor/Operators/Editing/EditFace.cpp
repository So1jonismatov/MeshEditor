#include "EditFace.h"
#include "View.h"
#include "Contact.h"
#include "Triad.h"
#include "utils/MathUtils.h"
#include <glm/gtc/matrix_transform.hpp>

void EditFaceOperator::onEnter(View &view)
{
    m_state = State::Idle;
}
void EditFaceOperator::onExit(View &view)
{
    cleanup(view);
}

void EditFaceOperator::cleanup(View &view)
{
    detachManipulator();
    m_targetMesh = nullptr;
    m_targetFace = FaceHandle{-1};
    m_state = State::Idle;
}

void EditFaceOperator::onUpdate(View &view)
{
    updateGizmoTransform(view);
}

void EditFaceOperator::onMouseMove(View &view, double x, double y)
{
    updateManipulatorDrag(view, x, y);
}

void EditFaceOperator::onMouseInput(View &view, ButtonCode button,
                                    Action action, Modifier mods, double x,
                                    double y)
{
    if (button != ButtonCode::MouseButtonLeft)
        return;

    if (action == Action::Press)
    {
        if (m_state == State::Idle)
        {
            if (beginManipulatorDrag(view, x, y))
                return;

            std::vector<Contact> meshContacts =
                view.raycast(x, y, FilterValue::Node);
            if (!meshContacts.empty())
            {
                Contact hit = meshContacts.front();

                if (m_targetMesh != hit.node->getMesh() ||
                    m_targetFace.index != hit.face.index)
                {
                    cleanup(view);

                    m_targetMesh = hit.node->getMesh();
                    m_targetFace = hit.face;

                    if (m_targetMesh && m_targetFace.index != -1)
                    {
                        std::vector<glm::vec3> poly =
                            m_targetMesh->collectFacePolygon(m_targetFace);
                        if (poly.size() >= 3)
                        {
                            // Calculate the center of the face polygon
                            glm::vec3 center(0.0f);
                            for (const auto &p : poly)
                                center += p;
                            center /= static_cast<float>(poly.size());

                            glm::vec3 normal = glm::cross(poly[1] - poly[0],
                                                          poly[2] - poly[0]);
                            if (glm::dot(normal, normal) > 0.0f)
                                normal = glm::normalize(normal);
                            else
                                normal = glm::vec3(0.0f, 0.0f, 1.0f);

                            auto triad = std::make_unique<Triad>(1.0f, 1.5f);

                            triad->setCallback(
                                [this](const glm::mat4 &deltaMatrix)
                                {
                                    if (m_targetMesh)
                                    {
                                        const auto &faces =
                                            m_targetMesh->getHalfEdgeTable()
                                                .getFaces();
                                        if (m_targetFace.index < 0 ||
                                            m_targetFace.index >=
                                                static_cast<int64_t>(
                                                    faces.size()))
                                        {
                                            return;
                                        }

                                        glm::mat4 T_center =
                                            m_manipulatorNode
                                                ->getRelativeTransform();
                                        glm::mat4 T_center_inv =
                                            glm::inverse(T_center);
                                        glm::mat4 targetDelta = T_center *
                                                                deltaMatrix *
                                                                T_center_inv;
                                        m_targetMesh->applyTransformation(
                                            m_targetFace, targetDelta);
                                    }
                                });

                            const glm::mat4 rot = Utils::rotationBetweenVectors(
                                glm::vec3(0.0f, 0.0f, 1.0f), normal);
                            const glm::mat4 centerTrf =
                                glm::translate(glm::mat4(1.0f), center) * rot;

                            attachGizmo(hit.node, std::move(triad), centerTrf);
                        }
                    }
                }
            }
            else
            {
                cleanup(view);
            }
        }
    }
    else if (action == Action::Release)
    {
        if (endManipulatorDrag(view, x, y) && m_targetMesh)
            m_targetMesh->flushOctreeLooseFaces();
    }
}
