#include "DistanceMeasurementOperator.h"
#include "View.h"
#include "Contact.h"
#include "Application.h"
#include "QtUI/Widgets/MeasurementCardWidget.h"
#include <iostream>
#include <cmath>

void DistanceMeasurementOperator::onEnter(View &view)
{
    m_active = false;
    m_hasMeasurement = false;
    updatePolyline(view, false);
    
    MeasurementData md;
    md.type = MeasurementType::Distance;
    md.isComplete = false;
    md.value = 0.0f;
    if (Application *app = Application::getInstance())
        app->setMeasurementData(md);

    std::cout << "[Distance] Tool active. Click to set start point." << std::endl;
}

void DistanceMeasurementOperator::onExit(View &view)
{
    m_active = false;
    m_hasMeasurement = false;
    updatePolyline(view, false);
    if (Application *app = Application::getInstance())
        app->clearMeasurementData();
}

void DistanceMeasurementOperator::onUpdate(View &view)
{
    if (m_startPoint.mesh && (m_active || m_hasMeasurement)) {
        updatePolyline(view, true);
    }
}

void DistanceMeasurementOperator::updatePolyline(View &view, bool show)
{
    if (m_startPoint.mesh) {
        if (show && m_endPoint.mesh && m_startPoint.node) {
            glm::mat4 invTrf1 = glm::inverse(m_startPoint.node->calcAbsoluteTransform());
            glm::vec3 localP1 = glm::vec3(invTrf1 * glm::vec4(m_startPoint.worldPos, 1.0f));
            glm::vec3 localP2 = glm::vec3(invTrf1 * glm::vec4(m_endPoint.worldPos, 1.0f));
            
            std::vector<glm::vec3> lines;
            lines.push_back(localP1);
            lines.push_back(localP2);
            
            const Viewport &vp = view.getViewport();
            const float fovRad = glm::radians(static_cast<float>(vp.getFov()) * 0.5f);
            const float vpHeight = static_cast<float>(vp.getHeight());
            const glm::vec3 eye = vp.getCamera().getEye();
            const glm::vec3 camRight = vp.getCamera().calcRight();
            const glm::vec3 camUp = vp.getCamera().getUp();
            const glm::vec3 camForward = vp.getCamera().calcForward();
            
            // Helper to draw a screen-constant 3D cross (5-7 pixels, 3px per arm)
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
                
                glm::vec3 rLocal = glm::vec3(invTrf1 * glm::vec4(worldP + rWorld, 1.0f)) - glm::vec3(invTrf1 * glm::vec4(worldP, 1.0f));
                glm::vec3 uLocal = glm::vec3(invTrf1 * glm::vec4(worldP + uWorld, 1.0f)) - glm::vec3(invTrf1 * glm::vec4(worldP, 1.0f));
                glm::vec3 fLocal = glm::vec3(invTrf1 * glm::vec4(worldP + fWorld, 1.0f)) - glm::vec3(invTrf1 * glm::vec4(worldP, 1.0f));
                
                glm::vec3 localCenter = glm::vec3(invTrf1 * glm::vec4(worldP, 1.0f));
                
                // Horizontal arm
                lines.push_back(localCenter - rLocal);
                lines.push_back(localCenter + rLocal);
                // Vertical arm
                lines.push_back(localCenter - uLocal);
                lines.push_back(localCenter + uLocal);
                // Depth arm
                lines.push_back(localCenter - fLocal);
                lines.push_back(localCenter + fLocal);
            };
            
            addCross(m_startPoint.worldPos);
            addCross(m_endPoint.worldPos);
            
            m_startPoint.mesh->setPolylineSegments(lines);
        } else {
            m_startPoint.mesh->setPolylineSegments({});
        }
    }
}

bool DistanceMeasurementOperator::raycastPoint(View &view, double x, double y, Point &outPoint)
{
    std::vector<Contact> meshContacts = view.raycast(x, y, FilterValue::Node);
    if (meshContacts.empty()) return false;

    Contact hit = meshContacts.front();
    Mesh *mesh = hit.node ? hit.node->getMesh() : nullptr;
    if (!mesh) return false;

    outPoint.mesh = mesh;
    outPoint.node = hit.node;
    outPoint.worldPos = hit.position;
    return true;
}

void DistanceMeasurementOperator::onMouseInput(View &view, ButtonCode button,
                                               Action action, Modifier mods,
                                               double x, double y)
{
    if (button != ButtonCode::MouseButtonLeft || action != Action::Press)
        return;

    Point p;
    if (raycastPoint(view, x, y, p)) {
        if (!m_active) {
            // First click
            if (m_startPoint.mesh && m_startPoint.mesh != p.mesh)
                m_startPoint.mesh->setPolylineSegments({});
            m_startPoint = p;
            m_endPoint = p;
            m_active = true;
            m_hasMeasurement = false;
            updatePolyline(view, true);

            MeasurementData md;
            md.type = MeasurementType::Distance;
            md.isComplete = false;
            md.p1 = m_startPoint.worldPos;
            md.p2 = m_endPoint.worldPos;
            md.value = 0.0f;
            md.delta = glm::vec3(0.0f);
            if (Application *app = Application::getInstance())
                app->setMeasurementData(md);

            std::cout << "[Distance] Start point set. Move mouse to measure." << std::endl;
        } else {
            // Second click
            m_endPoint = p;
            float dist = glm::length(m_endPoint.worldPos - m_startPoint.worldPos);
            m_hasMeasurement = true;
            m_active = false;
            updatePolyline(view, true);

            MeasurementData md;
            md.type = MeasurementType::Distance;
            md.isComplete = true;
            md.p1 = m_startPoint.worldPos;
            md.p2 = m_endPoint.worldPos;
            md.value = dist;
            md.delta = m_endPoint.worldPos - m_startPoint.worldPos;
            if (Application *app = Application::getInstance())
                app->setMeasurementData(md);

            std::cout << "[Distance] End point set. Distance: " << dist << " units." << std::endl;
        }
    }
}

void DistanceMeasurementOperator::onMouseMove(View &view, double x, double y)
{
    if (m_active) {
        Point p;
        if (raycastPoint(view, x, y, p)) {
            m_endPoint = p;
            updatePolyline(view, true);

            MeasurementData md;
            md.type = MeasurementType::Distance;
            md.isComplete = false;
            md.p1 = m_startPoint.worldPos;
            md.p2 = m_endPoint.worldPos;
            md.value = glm::length(m_endPoint.worldPos - m_startPoint.worldPos);
            md.delta = m_endPoint.worldPos - m_startPoint.worldPos;
            if (Application *app = Application::getInstance())
                app->setMeasurementData(md);
        }
    }
}
