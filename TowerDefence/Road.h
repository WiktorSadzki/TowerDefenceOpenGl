#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <GL/glew.h>

struct Vertex
{
    glm::vec3 Position;
    glm::vec2 TexCoords;
};

class Road
{
public:
    std::vector<glm::vec3> controlPoints;
    std::vector<glm::vec3> splinePoints;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float roadWidth = 3.0f;

    Road();

    void GenerateSpline();
    void GenerateMesh();
    void Draw();

private:
    glm::vec3 CatmullRom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t);
};