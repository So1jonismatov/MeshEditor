#include "EditVertex.h"
#include "View.h"
#include "Contact.h"
#include "TranslationManipulator.h"
#include <glm/gtc/matrix_transform.hpp>

void EditVertexOperator::onEnter(View &view)
{
    m_state = State::Idle;
}
void EditVertexOperator::onExit(View &view)
{
    cleanup(view);
}

void EditVertexOperator::cleanup(View &view)
{
    detachManipulator();
    m_targetMesh = nullptr;
    m_targetVertex = VertexHandle{-1};
    m_state = State::Idle;
}

void EditVertexOperator::onUpdate(View &view)
{
    updateGizmoTransform(view);
}

void EditVertexOperator::onMouseMove(View &view, double x, double y)
{
    updateManipulatorDrag(view, x, y);
}

void EditVertexOperator::onMouseInput(View &view, ButtonCode button,
                                      Action action, Modifier mods, double x,
                                      double y)
{
    if (button != ButtonCode::MouseButtonLeft)
        return;

    if (action == Action::Press)
    {
        if (m_state == State::Idle)
        {
            // if we select the manipulator we return
            if (beginManipulatorDrag(view, x, y))
                return;

            // we get the face hits
            std::vector<Contact> meshContacts =
                view.raycast(x, y, FilterValue::Node);
            if (!meshContacts.empty())
            {
                Contact hit = meshContacts.front();

                Mesh *mesh = hit.node->getMesh();
                if (!mesh)
                    return;

                const auto &het = mesh->getHalfEdgeTable();
                const auto &faces = het.getFaces();
                if (hit.face.index < 0 ||
                    hit.face.index >= static_cast<int64_t>(faces.size()) ||
                    faces[hit.face.index].heh.index == -1)
                {
                    return;
                }

                Face face = het.deref(hit.face);
                if (face.heh.index == -1)
                    return;

                glm::mat4 absTrf = hit.node->calcAbsoluteTransform();
                glm::mat4 invAbsTrf = glm::inverse(absTrf);
                glm::vec3 localHit = glm::vec3(invAbsTrf * glm::vec4(hit.position, 1.0f));

                float minDist = std::numeric_limits<float>::max();
                VertexHandle closestVertex{-1};
                // get the vertex with minimum distance
                HalfEdgeHandle curr = face.heh;
                do
                {
                    VertexHandle vh = het.deref(curr).dst;
                    glm::vec3 pos = het.getPoint(vh);
                    float dist = glm::length(pos - localHit);
                    if (dist < minDist)
                    {
                        minDist = dist;
                        closestVertex = vh;
                    }
                    curr = het.next(curr);
                } while (curr != face.heh);

                if (closestVertex.index != -1)
                {
                    if (m_targetMesh != mesh || m_targetVertex != closestVertex)
                    {
                        cleanup(view);
                        m_targetMesh = mesh;
                        m_targetVertex = closestVertex;

                        glm::vec3 avgNormal(0.0f);
                        HalfEdgeHandle startHeh = het.deref(closestVertex).heh;
                        if (startHeh.index != -1)
                        {
                            // calculate the average of the normals of the incident faces
                            HalfEdgeHandle vCurr = startHeh;
                            do
                            {
                                if (het.deref(vCurr).fh.index != -1)
                                {
                                    std::vector<glm::vec3> fPoly =
                                        mesh->collectFacePolygon(
                                            het.deref(vCurr).fh);
                                    if (fPoly.size() >= 3)
                                    {
                                        glm::vec3 n = glm::normalize(
                                            glm::cross(fPoly[1] - fPoly[0],
                                                       fPoly[2] - fPoly[0]));
                                        avgNormal += n;
                                    }
                                }
                                HalfEdgeHandle twin = het.deref(vCurr).twin;
                                if (twin.index == -1)
                                    break;
                                vCurr = het.next(twin);
                            } while (vCurr != startHeh && vCurr.index != -1);
                        }
                        if (glm::length(avgNormal) > 1e-6f)
                            avgNormal = glm::normalize(avgNormal);
                        else
                            avgNormal = glm::vec3(0, 1, 0);
                        // create the manipulator
                        glm::vec3 center = het.getPoint(closestVertex);
                        auto manipulator =
                            std::make_unique<TranslationManipulator>(avgNormal,
                                                                     1.5f);

                        manipulator->setCallback(
                            [this](const glm::mat4 &deltaMatrix)
                            {
                                if (m_targetMesh && m_targetVertex.index != -1)
                                {
                                    glm::mat4 T_manip =
                                        m_manipulatorNode
                                            ->getRelativeTransform();
                                    glm::mat4 deltaMeshSpace =
                                        T_manip * deltaMatrix *
                                        glm::inverse(T_manip);

                                    glm::vec3 startPos =
                                        m_targetMesh->getHalfEdgeTable()
                                            .getPoint(m_targetVertex);
                                    glm::vec3 newPos =
                                        glm::vec3(deltaMeshSpace *
                                                  glm::vec4(startPos, 1.0f));
                                    m_targetMesh->getHalfEdgeTable().setPoint(
                                        m_targetVertex, newPos);
                                    m_targetMesh->markVertexDirty(
                                        m_targetVertex);
                                }
                            });

                        attachGizmo(hit.node, std::move(manipulator),
                                    glm::translate(glm::mat4(1.0f), center));
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
