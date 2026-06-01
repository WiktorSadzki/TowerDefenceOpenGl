#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <GL/glew.h>

// Represents a single vertex with position and texture coordinates
struct Vertex
{
    glm::vec3 Position;
    glm::vec2 TexCoords;
};

class Road
{
public:
	// Control points define the path of the road, which are used to generate a smooth spline. The control points can be set in the game editor or defined in code to create different road layouts.
    std::vector<glm::vec3> controlPoints;
	// Spline points are generated from the control points using a Catmull-Rom spline interpolation. These points define the actual path of the road and are used to create the mesh vertices. The spline points are calculated in the GenerateSpline() method based on the control points.
    std::vector<glm::vec3> splinePoints;
	// 3D vertices of the road mesh, which include position and texture coordinates. These vertices are generated in the GenerateMesh() method based on the spline points and the defined road width. The vertices are stored in an indexed format, where the indices define how the vertices are connected to form triangles for rendering the road mesh.
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float roadWidth = 3.0f;

    Road();
       
    void GenerateSpline();
    void GenerateMesh();

private:
    glm::vec3 CatmullRom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t);
};