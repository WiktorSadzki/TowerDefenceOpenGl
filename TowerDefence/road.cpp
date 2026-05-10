#include "Road.h"

#include <glm/gtc/type_ptr.hpp>

Road::Road()
{
    controlPoints =
    {
        {-20, 0, -20},
        {-10, 0, -5},
        {0, 0, 0},
        {10, 0, 10},
        {20, 0, 5},
        {30, 0, 20},
        {40, 0, 0}
    };

    GenerateSpline();
    GenerateMesh();
    SetupMesh();
}

glm::vec3 Road::CatmullRom(
    glm::vec3 p0,
    glm::vec3 p1,
    glm::vec3 p2,
    glm::vec3 p3,
    float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
        );
}

void Road::GenerateSpline()
{
    splinePoints.clear();

    for (int i = 0; i < controlPoints.size() - 3; i++)
    {
        for (float t = 0; t < 1.0f; t += 0.02f)
        {
            glm::vec3 point = CatmullRom(
                controlPoints[i],
                controlPoints[i + 1],
                controlPoints[i + 2],
                controlPoints[i + 3],
                t
            );

            splinePoints.push_back(point);
        }
    }
}

void Road::GenerateMesh()
{
    vertices.clear();
    indices.clear();

    float texV = 0.0f;

    for (int i = 0; i < splinePoints.size() - 1; i++)
    {
        glm::vec3 current = splinePoints[i];
        glm::vec3 next = splinePoints[i + 1];

        glm::vec3 forward =
            glm::normalize(next - current);

        glm::vec3 right =
            glm::normalize(
                glm::cross(forward,
                    glm::vec3(0, 1, 0)));

        glm::vec3 leftPos =
            current - right * roadWidth;

        glm::vec3 rightPos =
            current + right * roadWidth;

        Vertex leftVertex;
        leftVertex.Position = leftPos;
        leftVertex.TexCoords = glm::vec2(0.0f, texV);

        Vertex rightVertex;
        rightVertex.Position = rightPos;
        rightVertex.TexCoords = glm::vec2(1.0f, texV);

        vertices.push_back(leftVertex);
        vertices.push_back(rightVertex);

        texV += 0.2f;
    }

    for (int i = 0; i < vertices.size() - 2; i += 2)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
        indices.push_back(i + 2);

        indices.push_back(i + 1);
        indices.push_back(i + 3);
        indices.push_back(i + 2);
    }
}

void Road::SetupMesh()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        &vertices[0],
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        &indices[0],
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)0
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, TexCoords)
    );

    glBindVertexArray(0);
}

void Road::Draw()
{
    glBindVertexArray(VAO);

    glDrawElements(
        GL_TRIANGLES,
        indices.size(),
        GL_UNSIGNED_INT,
        0
    );

    glBindVertexArray(0);
}