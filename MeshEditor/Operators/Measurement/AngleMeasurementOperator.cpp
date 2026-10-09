#include "AngleMeasurementOperator.h"
#include "View.h"
#include "Contact.h"
#include "Graph/Model.h"
#include "Application.h"
#include <iostream>
#include <optional>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vector_angle.hpp>

glm::vec3 AngleMeasurementOperator::computeWorldNormal(Mesh* mesh, FaceHandle fh, Node* node)
{
    std::vector<glm::vec3> poly = mesh->collectFacePolygon(fh);
    if (poly.size() < 3) return glm::vec3(0, 1, 0);

    // Newell's method for robust polygon normal computation
    glm::vec3 localNormal(0.0f);
    for (size_t i = 0; i < poly.size(); ++i) {
        const glm::vec3 &cur = poly[i];
        const glm::vec3 &next = poly[(i + 1) % poly.size()];
        localNormal.x += (cur.y - next.y) * (cur.z + next.z);
        localNormal.y += (cur.z - next.z) * (cur.x + next.x);
        localNormal.z += (cur.x - next.x) * (cur.y + next.y);
    }
    if (glm::dot(localNormal, localNormal) > 1e-12f)
        localNormal = glm::normalize(localNormal);
    else
        localNormal = glm::vec3(0, 1, 0);
    
    glm::mat4 absTrf = node->calcAbsoluteTransform();
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(absTrf)));
    return glm::normalize(normalMatrix * localNormal);
}

void AngleMeasurementOperator::onEnter(View &view)
{
    m_hasFirst = false;
    if (Model *model = view.getModel()) {
        model->forEachMeshRecursive([](Mesh *m) { m->clearSelectedFace(); });
    }

    MeasurementData md;
    md.type = MeasurementType::Angle;
    md.isComplete = false;
    md.value = 0.0f;
    if (Application *app = Application::getInstance())
        app->setMeasurementData(md);

    std::cout << "[Angle] Tool active. Click first face." << std::endl;
}

void AngleMeasurementOperator::onExit(View &view)
{
    m_hasFirst = false;
    if (Model *model = view.getModel()) {
        model->forEachMeshRecursive([](Mesh *m) { m->clearSelectedFace(); });
    }
    if (Application *app = Application::getInstance())
        app->clearMeasurementData();
}

void AngleMeasurementOperator::onMouseInput(View &view, ButtonCode button,
                                           Action action, Modifier mods,
                                           double x, double y)
{
    if (button != ButtonCode::MouseButtonLeft || action != Action::Press)
        return;

    std::optional<Contact> hit = view.pickFace(x, y);
    if (!hit || !hit->node) return;

    Mesh *mesh = hit->node->getMesh();
    if (!mesh || hit->face.index < 0) return;

    glm::vec3 normal = computeWorldNormal(mesh, hit->face, hit->node);

    if (!m_hasFirst) {
        // Clear previous face selections across the scene
        if (Model *model = view.getModel()) {
            model->forEachMeshRecursive([](Mesh *m) { m->clearSelectedFace(); });
        }
        m_firstFace = {mesh, hit->node, hit->face, normal};
        m_hasFirst = true;
        mesh->addSelectedFace(hit->face);

        MeasurementData md;
        md.type = MeasurementType::Angle;
        md.isComplete = false;
        md.value = 0.0f;
        md.n1 = normal;
        md.n2 = glm::vec3(0.0f);
        if (Application *app = Application::getInstance())
            app->setMeasurementData(md);

        std::cout << "[Angle] First face selected (Face #" << hit->face.index << "). Click second face." << std::endl;
    } else {
        float angleRad = glm::angle(m_firstFace.normal, normal);
        float angleDeg = glm::degrees(angleRad);
        
        mesh->addSelectedFace(hit->face);

        MeasurementData md;
        md.type = MeasurementType::Angle;
        md.isComplete = true;
        md.value = angleDeg;
        md.n1 = m_firstFace.normal;
        md.n2 = normal;
        if (Application *app = Application::getInstance())
            app->setMeasurementData(md);

        std::cout << "[Angle] Second face selected (Face #" << hit->face.index << "). Angle between faces: " << angleDeg << " degrees." << std::endl;
        m_hasFirst = false; // Reset for next measurement
    }
}
