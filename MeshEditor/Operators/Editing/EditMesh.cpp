#include "EditMesh.h"
#include "View.h"
#include "Contact.h"
#include "TranslationManipulator.h"
#include <glm/gtc/matrix_transform.hpp>

void EditMeshOperator::onEnter(View &view)
{
    m_state = State::Idle;
}

void EditMeshOperator::onExit(View &view)
{
    cleanup(view);
}

void EditMeshOperator::cleanup(View &view)
{
    detachManipulator();
    m_targetMesh = nullptr;
    m_targetFace = FaceHandle{-1};
    m_state = State::Idle;
}

void EditMeshOperator::onUpdate(View &view)
{
    updateGizmoTransform(view);
}

void EditMeshOperator::onMouseMove(View &view, double x, double y)
{
    updateManipulatorDrag(view, x, y);
}

void EditMeshOperator::onMouseInput(View &view, ButtonCode button,
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
                Mesh *hitMesh = hit.node ? hit.node->getMesh() : nullptr;
                if (!hitMesh)
                    return;

                const auto &faces = hitMesh->getHalfEdgeTable().getFaces();
                if (hit.face.index < 0 ||
                    hit.face.index >= static_cast<int64_t>(faces.size()) ||
                    faces[hit.face.index].heh.index == -1)
                {
                    cleanup(view);
                    return;
                }

                if (m_targetMesh != hitMesh ||
                    m_targetFace.index != hit.face.index)
                {
                    cleanup(view);

                    m_targetMesh = hitMesh;
                    m_targetFace = hit.face;

                    if (m_targetMesh && m_targetFace.index != -1)
                    {
                        std::vector<glm::vec3> poly =
                            m_targetMesh->collectFacePolygon(m_targetFace);
                        if (poly.size() >= 3)
                        {
                            glm::vec3 center(0.0f);
                            glm::vec3 faceMin = poly[0];
                            glm::vec3 faceMax = poly[0];
                            for (const auto &p : poly)
                            {
                                center += p;
                                faceMin = glm::min(faceMin, p);
                                faceMax = glm::max(faceMax, p);
                            }
                            center /= static_cast<float>(poly.size());

                            glm::vec3 normal = glm::cross(poly[1] - poly[0],
                                                          poly[2] - poly[0]);
                            if (glm::dot(normal, normal) > 0.0f)
                            {
                                normal = glm::normalize(normal);
                            }
                            else
                            {
                                normal = glm::vec3(0.0f, 0.0f, 1.0f);
                            }

                            auto manipulator =
                                std::make_unique<TranslationManipulator>(
                                    normal, 1.5f, 0.8f);

                            manipulator->setCallback(
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

                                        glm::mat4 T_manip =
                                            m_manipulatorNode
                                                ->getRelativeTransform();
                                        glm::mat4 deltaMeshSpace =
                                            T_manip * deltaMatrix *
                                            glm::inverse(T_manip);

                                        m_targetMesh->applyTransformation(
                                            m_targetFace, deltaMeshSpace);
                                    }
                                });

                            attachGizmo(
                                hit.node, std::move(manipulator),
                                glm::translate(glm::mat4(1.0f), center));
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