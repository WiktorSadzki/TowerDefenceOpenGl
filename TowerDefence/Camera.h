#pragma once
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    float angleX = 50.0f;   // pitch  (10 – 85 degrees)
    float angleY = 0.0f;    // yaw    (free)
    float zoom = 22.0f;   // orbit arm length
    float cx = 0.0f;    // look-at X
    float cz = 0.0f;    // look-at Z

	// Updates camera angles based on mouse movement, with limits on pitch to prevent flipping
    void handleMouse(int dx, int dy) {
        angleY += dx * 0.4f;
        angleX += dy * 0.4f;

		// Clamp the pitch angle to prevent the camera from flipping upside down or going too low. This ensures a better user experience by keeping the camera within a reasonable range of angles for viewing the game scene.
        if (angleX < 10.0f) angleX = 10.0f;
        if (angleX > 85.0f) angleX = 85.0f;
    }
};
