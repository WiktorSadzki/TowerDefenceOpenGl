#pragma once
#define GLEW_STATIC
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>

class Camera {
public:
    float angleX = 50.0f;
    float angleY = 0.0f;
    float zoom = 22.0f;
    float cx = 10.0f;
    float cz = 10.0f;

    void apply() {
        glLoadIdentity();
        glTranslatef(0, 0, -zoom);
        glRotatef(angleX, 1, 0, 0);
        glRotatef(angleY, 0, 1, 0);
        glTranslatef(-cx, 0, -cz);
    }

    void handleMouse(int dx, int dy) {
        angleY += dx * 0.4f;
        angleX += dy * 0.4f;
        if (angleX < 10.0f) angleX = 10.0f;
        if (angleX > 85.0f) angleX = 85.0f;
    }

    bool screenToTile(int sx, int sy, int w, int h, float& outX, float& outZ) {
        float ndcX = (2.0f * sx / w) - 1.0f;
        float ndcY = 1.0f - (2.0f * sy / h);

        float pitch = angleX * 3.14159f / 180.0f;
        float yaw = angleY * 3.14159f / 180.0f;

        float eyeX = cx - sinf(yaw) * cosf(pitch) * zoom;
        float eyeY = sinf(pitch) * zoom;
        float eyeZ = cz - cosf(yaw) * cosf(pitch) * zoom;

        float dirX = ndcX * cosf(yaw) - ndcY * sinf(pitch) * sinf(yaw);
        float dirZ = ndcX * sinf(yaw) + ndcY * sinf(pitch) * cosf(yaw);
        float dirY = -ndcY * cosf(pitch);

        if (fabsf(dirY) < 0.001f) return false;
        float t = -eyeY / dirY;
        if (t < 0) return false;

        outX = eyeX + dirX * t;
        outZ = eyeZ + dirZ * t;
        return true;
    }
};