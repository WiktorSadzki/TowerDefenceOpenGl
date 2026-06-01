#include "Road.h"
#include <glm/gtc/type_ptr.hpp>

Road::Road() {
	// Define control points for the road path. These points can be adjusted to create different road shapes and layouts in the game. The control points are used to generate a smooth spline that defines the actual path of the road, which is then used to create the mesh for rendering.
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

    int n = controlPoints.size();
	int subdivisions = 15; // Number of points to generate between each pair of control points for a smoother curve

    for (int i = 0; i < n - 1; i++) {
		// For each segment defined by control points p1 and p2, we need to determine the surrounding points p0 and p3 for the Catmull-Rom spline calculation. If we are at the start or end of the control points, we can duplicate the first or last point to ensure we have enough points for the spline calculation.
        glm::vec3 p0 = (i == 0) ? controlPoints[i] : controlPoints[i - 1];
        glm::vec3 p1 = controlPoints[i];
        glm::vec3 p2 = controlPoints[i + 1];
        glm::vec3 p3 = (i == n - 2) ? controlPoints[i + 1] : controlPoints[i + 2];

		// Generate points along the Catmull-Rom spline for the current segment. We loop through a number of subdivisions to create a smooth curve between the control points. The parameter t goes from 0 to 1, where 0 corresponds to p1 and 1 corresponds to p2. The Catmull-Rom formula is used to calculate the position of each point on the spline based on the four control points.
        for (int step = 0; step < subdivisions; step++) {
			// Calculate the parameter t for the current step, which determines how far along the spline we are between p1 and p2. This is done by dividing the current step by the total number of subdivisions, giving us a value between 0 and 1 that we can use in the Catmull-Rom formula to calculate the position of the point on the spline.
            float t = (float)step / (float)subdivisions;

            float t2 = t * t;
            float t3 = t2 * t;

			// formula: 0.5 * ((2 * p1) + (-p0 + p2) * t + (2*p0 - 5*p1 + 4*p2 - p3) * t^2 + (-p0 + 3*p1 - 3*p2 + p3) * t^3) for a Centripetal Catmull-Rom spline
            glm::vec3 splinePt = 0.5f * (
                (2.0f * p1) +
                (-p0 + p2) * t +
                (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
                );

            splinePoints.push_back(splinePt);
        }
    }

    splinePoints.push_back(controlPoints.back());
}

void Road::GenerateMesh() {
    vertices.clear();
    indices.clear();
    if (splinePoints.size() < 2) return;

    float tex_v_coord = 0.0f;

    for (size_t i = 0; i < splinePoints.size(); i++) {
        glm::vec3 current = splinePoints[i];
        glm::vec3 dir;

		// Calculate the direction of the road at the current point. This is done by looking at the neighboring points on the spline. If we are at the start of the spline, we use the direction to the next point. If we are at the end, we use the direction from the previous point. For points in the middle, we average the directions to the previous and next points to get a smoother result.
        if (i == 0) {
            dir = glm::normalize(splinePoints[i + 1] - current);
        }
        else if (i == splinePoints.size() - 1) {
            dir = glm::normalize(current - splinePoints[i - 1]);
        }
        else {
			// Average the direction from the previous point and the next point to get a smoother direction vector for points in the middle of the spline. This helps to create a more natural curve for the road, especially around bends and turns, by considering both the incoming and outgoing directions at each point.
            glm::vec3 dirPrev = glm::normalize(current - splinePoints[i - 1]);
            glm::vec3 dirNext = glm::normalize(splinePoints[i + 1] - current);
            dir = glm::normalize(dirPrev + dirNext);
        }

		// Calculate the right vector perpendicular to the direction of the road. We use the cross product with the up vector (0, 1, 0) to get a vector that points to the right of the road. This right vector is then normalized to ensure it has a length of 1, which allows us to easily calculate the positions of the left and right vertices of the road by multiplying this right vector by the road width.
        glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));

        Vertex leftVert, rightVert;
           
        leftVert.Position = current - right * roadWidth;
        rightVert.Position = current + right * roadWidth;

		// Texture coordinates are assigned based on the distance along the spline. The V coordinate is incremented based on the length of the segment between the current point and the previous point, multiplied by a scaling factor (0.15f) to control how stretched the texture appears along the road. The U coordinate is set to 0 for the left vertex and 1 for the right vertex, which allows us to use a simple texture that stretches across the width of the road.
        leftVert.TexCoords = glm::vec2(tex_v_coord, 0.0f);
        rightVert.TexCoords = glm::vec2(tex_v_coord, 1.0f);

        vertices.push_back(leftVert);
        vertices.push_back(rightVert);

        if (i > 0) {
			// Increment the V coordinate for the texture based on the length of the segment between the current point and the previous point. This creates a more natural texture mapping along the road, where the texture stretches according to the actual distance traveled along the spline rather than just being based on the number of points. The scaling factor (0.15f) can be adjusted to control how tightly or loosely the texture is stretched along the road.
            float segLen = glm::length(splinePoints[i] - splinePoints[i - 1]);
            tex_v_coord += segLen * 0.15f;
        }
    }

	// Generate indices for the road mesh. We create two triangles for each segment of the road between consecutive points on the spline. Since we have two vertices (left and right) for each point, we can use the indices to define how these vertices are connected to form triangles. The pattern of indices creates a strip of triangles that forms the road surface, connecting the left and right vertices of each point to the corresponding vertices of the next point.
    for (size_t i = 0; i < (vertices.size() / 2) - 1; i++) {
        int base = i * 2;
		// Triangle 1: left vertex of current point, right vertex of current point, left vertex of next point
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
		// Triangle 2: right vertex of current point, right vertex of next point, left vertex of next point
        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }
}