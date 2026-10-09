#include "EdgeMeasurementOperator.h"
#include "View.h"
#include "Contact.h"
#include "Application.h"
#include <iostream>
#include <cmath>

void EdgeMeasurementOperator::onEnter(View &view)
{
    m_hasEdge = false;
    if (m_edge.mesh) {
        m_edge.mesh->setPolylineSegments({});
        m_edge.mesh = nullptr;
    }

    MeasurementData md;
    md.type = MeasurementType::Edge;
    md.isComplete = false;
    md.value = 0.0f;
    if (Application *app = Application::getInstance())
        app->setMeasurementData(md);

    std::cout << "[Edge] Tool active. Click near an edge to measure its length." << std::endl;
}

void EdgeMeasurementOperator::onExit(View &view)
{
    if (m_edge.mesh) {
        m_edge.mesh->setPolylineSegments({});
        m_edge.mesh = nullptr;
    }
    m_hasEdge = false;
    if (Application *app = Application::getInstance())
        app->clearMeasurementData();
}

void EdgeMeasurementOperator::onUpdate(View &view)
{
    if (m_hasEdge && m_edge.mesh && m_edge.node) {
        updatePolyline(view);
    }
}

void EdgeMeasurementOperator::updatePolyline(View &view)
{
    if (!m_edge.mesh || !m_edge.node) return;

    glm::mat4 absTrf = m_edge.node->calcAbsoluteTransform();
    glm::mat4 invAbsTrf = glm::inverse(absTrf);

    glm::vec3 localStart = glm::vec3(invAbsTrf * glm::vec4(m_edge.worldStart, 1.0f));
    glm::vec3 localEnd = glm::vec3(invAbsTrf * glm::vec4(m_edge.worldEnd, 1.0f));

    std::vector<glm::vec3> lines;
    lines.push_back(localStart);
    lines.push_back(localEnd);

    const Viewport &vp = view.getViewport();
    const float fovRad = glm::radians(static_cast<float>(vp.getFov()) * 0.5f);
    const float vpHeight = static_cast<float>(vp.getHeight());
    const glm::vec3 eye = vp.getCamera().getEye();
    const glm::vec3 camRight = vp.getCamera().calcRight();
    const glm::vec3 camUp = vp.getCamera().getUp();
    const glm::vec3 camForward = vp.getCamera().calcForward();

    auto addCross = [&](const glm::vec3 &worldP)
    {
        const float dist = glm::length(worldP - eye);
        const float worldHeight = vp.isParallelProjection()
            ? static_cast<float>(vp.calcTargetPlaneHeight())
            : (2.0f * dist * std::tan(fovRad));
        const float pixelInWorld = (vpHeight > 0.0f) ? (worldHeight / vpHeight) : 0.002f;
        const float arm = 5.5f * pixelInWorld; // ~11 pixels total span (5.5px per arm)

        glm::vec3 rWorld = camRight * arm;
        glm::vec3 uWorld = camUp * arm;
        glm::vec3 fWorld = camForward * arm;

        glm::vec3 rLocal = glm::vec3(invAbsTrf * glm::vec4(worldP + rWorld, 1.0f)) - glm::vec3(invAbsTrf * glm::vec4(worldP, 1.0f));
        glm::vec3 uLocal = glm::vec3(invAbsTrf * glm::vec4(worldP + uWorld, 1.0f)) - glm::vec3(invAbsTrf * glm::vec4(worldP, 1.0f));
        glm::vec3 fLocal = glm::vec3(invAbsTrf * glm::vec4(worldP + fWorld, 1.0f)) - glm::vec3(invAbsTrf * glm::vec4(worldP, 1.0f));

        glm::vec3 localCenter = glm::vec3(invAbsTrf * glm::vec4(worldP, 1.0f));

        lines.push_back(localCenter - rLocal);
        lines.push_back(localCenter + rLocal);
        lines.push_back(localCenter - uLocal);
        lines.push_back(localCenter + uLocal);
        lines.push_back(localCenter - fLocal);
        lines.push_back(localCenter + fLocal);
    };

    addCross(m_edge.worldStart);
    addCross(m_edge.worldEnd);

    m_edge.mesh->setPolylineSegments(lines);
}

void EdgeMeasurementOperator::onMouseInput(View &view, ButtonCode button,
                                           Action action, Modifier mods,
                                           double x, double y)
{
    if (button != ButtonCode::MouseButtonLeft || action != Action::Press)
        return;

    std::vector<Contact> meshContacts = view.raycast(x, y, FilterValue::Node);
    if (meshContacts.empty()) return;

    Contact hit = meshContacts.front();
    Mesh *mesh = hit.node ? hit.node->getMesh() : nullptr;
    if (!mesh || hit.face.index < 0) return;

    const auto& het = mesh->getHalfEdgeTable();
    Face face = het.deref(hit.face);
    if (face.heh.index == -1) return;

    glm::mat4 absTrf = hit.node->calcAbsoluteTransform();
    glm::mat4 invAbsTrf = glm::inverse(absTrf);
    glm::vec3 localHit = glm::vec3(invAbsTrf * glm::vec4(hit.position, 1.0f));

    float minDist = std::numeric_limits<float>::max();
    glm::vec3 bestStart(0.0f), bestEnd(0.0f);

    HalfEdgeHandle curr = face.heh;
    do
    {
        VertexHandle v1 = het.deref(het.prev(curr)).dst;
        VertexHandle v2 = het.deref(curr).dst;
        
        glm::vec3 p1 = het.getPoint(v1);
        glm::vec3 p2 = het.getPoint(v2);

        glm::vec3 dir = p2 - p1;
        float len2 = glm::dot(dir, dir);
        float dist = 0.0f;
        if (len2 == 0.0f) {
            dist = glm::distance(localHit, p1);
        } else {
            float t = glm::max(0.0f, glm::min(1.0f, glm::dot(localHit - p1, dir) / len2));
            glm::vec3 proj = p1 + t * dir;
            dist = glm::distance(localHit, proj);
        }

        if (dist < minDist)
        {
            minDist = dist;
            bestStart = p1;
            bestEnd = p2;
        }

        curr = het.next(curr);
    } while (curr != face.heh && curr.index != -1);

    if (m_edge.mesh && m_edge.mesh != mesh)
        m_edge.mesh->setPolylineSegments({});

    m_edge.mesh = mesh;
    m_edge.node = hit.node;
    m_edge.worldStart = glm::vec3(absTrf * glm::vec4(bestStart, 1.0f));
    m_edge.worldEnd = glm::vec3(absTrf * glm::vec4(bestEnd, 1.0f));
    m_hasEdge = true;

    updatePolyline(view);

    float length = glm::length(m_edge.worldEnd - m_edge.worldStart);

    MeasurementData md;
    md.type = MeasurementType::Edge;
    md.isComplete = true;
    md.p1 = m_edge.worldStart;
    md.p2 = m_edge.worldEnd;
    md.value = length;
    md.delta = m_edge.worldEnd - m_edge.worldStart;
    if (Application *app = Application::getInstance())
        app->setMeasurementData(md);

    std::cout << "[Edge] Edge Length: " << length << " units." << std::endl;
}
