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
    float cx = 0.0f;
    float cz = 0.0f;

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
        GLdouble modelview[16], projection[16];
        GLint viewport[4] = { 0, 0, w, h };
        glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
        glGetDoublev(GL_PROJECTION_MATRIX, projection);

        double winY = (double)h - (double)sy - 1.0;

        double nearX, nearY, nearZ;
        double farX, farY, farZ;
        gluUnProject(sx, winY, 0.0, modelview, projection, viewport, &nearX, &nearY, &nearZ);
        gluUnProject(sx, winY, 1.0, modelview, projection, viewport, &farX, &farY, &farZ);

        double dirX = farX - nearX;
        double dirY = farY - nearY;
        double dirZ = farZ - nearZ;

        if (fabs(dirY) < 0.001) return false;
        double t = -nearY / dirY;
        if (t < 0) return false;

        outX = (float)(nearX + dirX * t);
        outZ = (float)(nearZ + dirZ * t);
        return true;
    }
};