#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include "Game.h"

void Game::init() {
    lives = 20; gold = 100; wave = 1; gameOver = false;
    towers.clear();
}

void Game::update(float dt) {
    if (gameOver) return;
    if (lives <= 0) { lives = 0; gameOver = true; }
}

void Game::render() {
    glBegin(GL_QUADS);
    glColor3f(0.4f, 0.8f, 0.4f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-50.0f, 0.0f, -50.0f); glVertex3f(50.0f, 0.0f, -50.0f);
    glVertex3f(50.0f, 0.0f, 50.0f); glVertex3f(-50.0f, 0.0f, 50.0f);
    glEnd();

    for (const auto& t : towers) {
        glPushMatrix();
        glTranslatef(t.x, 0.5f, t.z);
        glColor3f(0.6f, 0.6f, 0.7f);
        glScalef(0.8f, 1.2f, 0.8f);
        glBegin(GL_QUADS);
        glVertex3f(0.5f, 0.5f, -0.5f); glVertex3f(-0.5f, 0.5f, -0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f); glVertex3f(0.5f, 0.5f, 0.5f);
        glVertex3f(0.5f, -0.5f, 0.5f); glVertex3f(-0.5f, -0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f); glVertex3f(0.5f, 0.5f, 0.5f);
        glEnd();
        glPopMatrix();
    }
}

void Game::tryPlaceTower(int mx, int my, Camera& cam) {
    if (gold < 20) return;
    GLint viewport[4]; GLdouble mv[16], proj[16]; GLfloat wz; GLdouble wx, wy, wz_world;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glReadPixels(mx, viewport[3] - my, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &wz);
    gluUnProject(mx, viewport[3] - my, wz, mv, proj, viewport, &wx, &wy, &wz_world);
    towers.push_back({ (float)wx, (float)wz_world });
    gold -= 20;
}