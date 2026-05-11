#include "Road.h"
#include <glm/gtc/type_ptr.hpp>

Road::Road() {
    controlPoints = {
        glm::vec3(-45.0f, 0.0f, 10.0f),
        glm::vec3(-20.0f, 0.0f, 10.0f),
        glm::vec3(-20.0f, 0.0f, -15.0f),
        glm::vec3(20.0f, 0.0f, -15.0f),
        glm::vec3(20.0f, 0.0f, 30.0f),
        glm::vec3(45.0f, 0.0f, 30.0f)
    };
    GenerateSpline();
    GenerateMesh();
}

void Road::GenerateSpline() {
    splinePoints.clear();
    for (const auto& pt : controlPoints) {
        splinePoints.push_back(pt);
    }
}

void Road::GenerateMesh() {
    vertices.clear();
    indices.clear();
    if (splinePoints.size() < 2) return;

    float tex_v_coord = 0.0f;

    for (size_t i = 0; i < splinePoints.size(); i++) {
        glm::vec3 current = splinePoints[i];
        glm::vec3 dir;

        if (i == 0) {
            dir = glm::normalize(splinePoints[i + 1] - current);
        }
        else if (i == splinePoints.size() - 1) {
            dir = glm::normalize(current - splinePoints[i - 1]);
        }
        else {
            glm::vec3 dirPrev = glm::normalize(current - splinePoints[i - 1]);
            glm::vec3 dirNext = glm::normalize(splinePoints[i + 1] - current);
            dir = glm::normalize(dirPrev + dirNext);
        }

        glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));

        Vertex leftVert, rightVert;
        leftVert.Position = current - right * roadWidth;
        leftVert.TexCoords = glm::vec2(0.0f, tex_v_coord);

        rightVert.Position = current + right * roadWidth;
        rightVert.TexCoords = glm::vec2(1.0f, tex_v_coord);

        vertices.push_back(leftVert);
        vertices.push_back(rightVert);

        tex_v_coord += 1.0f;
    }

    for (size_t i = 0; i < (vertices.size() / 2) - 1; i++) {
        int base = i * 2;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);

        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }
}

void Road::Draw() {
    glDisable(GL_LIGHTING);
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_TRIANGLES);
    for (size_t iter = 0; iter < indices.size(); iter++) {
        unsigned int current_idx = indices[iter];
        Vertex current_vert = vertices[current_idx];
        glTexCoord2f(current_vert.TexCoords.x, current_vert.TexCoords.y);
        glVertex3f(current_vert.Position.x, current_vert.Position.y + 0.05f, current_vert.Position.z);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}