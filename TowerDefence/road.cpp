#include "Road.h"
#include <glm/gtc/type_ptr.hpp>
#include <vector>

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
    if (controlPoints.size() < 2) return;

    const int subdivisions = 15;
    for (size_t i = 0; i < controlPoints.size() - 1; i++) {
        glm::vec3 p0 = (i == 0) ? controlPoints[i] : controlPoints[i - 1];
        glm::vec3 p1 = controlPoints[i];
        glm::vec3 p2 = controlPoints[i + 1];
        glm::vec3 p3 = (i == controlPoints.size() - 2) ? controlPoints[i + 1] : controlPoints[i + 2];

        for (int step = 0; step < subdivisions; step++) {
            float t = (float)step / (float)subdivisions;
            splinePoints.push_back(0.5f * (
                (2.0f * p1) +
                (-p0 + p2) * t +
                (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t +
                (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t * t * t
                ));
        }
    }
    splinePoints.push_back(controlPoints.back());
}
void Road::GenerateMesh() {
    vertices.clear();
    indices.clear();
    if (splinePoints.size() < 2) return;

    const glm::vec3 groundOffset = glm::vec3(0.0f, 0.01f, 0.0f); // Prevents Z-fighting
    float accumulatedDistance = 0.0f;

    // STEP 1: Generate a continuous strip of vertex pairs
    for (size_t i = 0; i < splinePoints.size(); i++) {
        glm::vec3 p = splinePoints[i];

        // Calculate a smooth, continuous forward direction at this specific point
        glm::vec3 dir;
        if (i == 0) {
            dir = glm::normalize(splinePoints[1] - splinePoints[0]);
        }
        else if (i == splinePoints.size() - 1) {
            dir = glm::normalize(splinePoints[i] - splinePoints[i - 1]);
        }
        else {
            // Average direction between the previous and next point
            dir = glm::normalize(splinePoints[i + 1] - splinePoints[i - 1]);
        }

        glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0.0f, 1.0f, 0.0f)));

        // Track the total physical distance traveled along the center of the road
        if (i > 0) {
            accumulatedDistance += glm::length(splinePoints[i] - splinePoints[i - 1]);
        }

        // 0.15f is your texture tiling scale factor. Adjust this to tile tighter or wider.
        float vCoord = accumulatedDistance * 0.15f;

        Vertex leftVert, rightVert;

        // Left Vertex (Index: 2 * i)
        leftVert.Position = p - right * roadWidth + groundOffset;
        leftVert.TexCoords = glm::vec2(0.0f, vCoord);
        leftVert.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        leftVert.Tangent = right;
        leftVert.Color = glm::vec3(1.0f, 1.0f, 1.0f);

        // Right Vertex (Index: 2 * i + 1)
        rightVert.Position = p + right * roadWidth + groundOffset;
        rightVert.TexCoords = glm::vec2(1.0f, vCoord);
        rightVert.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        rightVert.Tangent = right;
        rightVert.Color = glm::vec3(1.0f, 1.0f, 1.0f);

        vertices.push_back(leftVert);
        vertices.push_back(rightVert);
    }

    // STEP 2: Sew the vertex pairs together with triangles
    for (size_t i = 0; i < splinePoints.size() - 1; i++) {
        unsigned int topLeft = 2 * static_cast<unsigned int>(i);
        unsigned int topRight = 2 * static_cast<unsigned int>(i) + 1;
        unsigned int bottomLeft = 2 * static_cast<unsigned int>(i + 1);
        unsigned int bottomRight = 2 * static_cast<unsigned int>(i + 1) + 1;

        // Triangle 1 (Matches your original winding order convention)
        indices.push_back(topLeft);
        indices.push_back(topRight);
        indices.push_back(bottomLeft);

        // Triangle 2
        indices.push_back(topRight);
        indices.push_back(bottomRight);
        indices.push_back(bottomLeft);
    }
}