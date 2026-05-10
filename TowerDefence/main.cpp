
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <iostream>
#include "Game.h"
#include "Camera.h"

static Game   g_game;
static Camera g_camera;
static double g_lastMouseX = 0;
static double g_lastMouseY = 0;
static bool   g_mouseDown = false;
static bool g_rightMouseDown = false;
static bool g_mouseMoved = false;
static float  g_lastTime = 0.0f;

const float MAP_LIMIT = 45.0f;
const float MOVE_SPEED = 30.0f;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (height == 0) height = 1;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)width / height, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        g_mouseDown = (action == GLFW_PRESS);
        if (action == GLFW_PRESS) g_mouseMoved = false;
        if (action == GLFW_RELEASE && !g_mouseMoved) {
            double x_coord, y_coord;
            glfwGetCursorPos(window, &x_coord, &y_coord);
            g_game.tryPlaceTower((int)x_coord, (int)y_coord, g_camera);
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        g_rightMouseDown = (action == GLFW_PRESS);
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    float dx = (float)(xpos - g_lastMouseX);
    float dy = (float)(ypos - g_lastMouseY);

    if (g_mouseDown) {
        g_mouseMoved = true;
        float rad = g_camera.angleY * 3.14159f / 180.0f;
        float fwdX = sinf(rad), fwdZ = -cosf(rad);
        float rgtX = cosf(rad), rgtZ = sinf(rad);
        float speed = g_camera.zoom * 0.002f;
        g_camera.cx -= rgtX * dx * speed;
        g_camera.cz -= rgtZ * dx * speed;
        g_camera.cx += fwdX * dy * speed;
        g_camera.cz += fwdZ * dy * speed;
    }
    if (g_rightMouseDown) {
        g_camera.handleMouse((int)dx, (int)dy);
    }

    g_lastMouseX = xpos;
    g_lastMouseY = ypos;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (yoffset > 0) g_camera.zoom = fmaxf(5.0f, g_camera.zoom - 1.0f);
    else if (yoffset < 0) g_camera.zoom = fminf(50.0f, g_camera.zoom + 1.0f);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
        if (key == GLFW_KEY_R) g_game.init();

        if (key == GLFW_KEY_R) {
            g_game.init();
            g_camera.cx = 0.0f;
            g_camera.cz = 0.0f;
        }

        if (key == GLFW_KEY_1) g_game.selectTowerType(0);
        if (key == GLFW_KEY_2) g_game.selectTowerType(1);
        if (key == GLFW_KEY_3) g_game.selectTowerType(2);
    }
}

int main(void) {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(800, 600, "Tower Defense 3D", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    if (glewInit() != GLEW_OK) return -1;

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    framebuffer_size_callback(window, w, h);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);

    g_game.init();
    g_lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float current_frame_time = (float)glfwGetTime();
        float delta_step = current_frame_time - g_lastTime;
        g_lastTime = current_frame_time;

        double mouse_x, mouse_y;
        int screen_w, screen_h;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);
        glfwGetWindowSize(window, &screen_w, &screen_h);

        const int margin = 15;

        float rad = g_camera.angleY * 3.14159f / 180.0f;
        float fwdX = sinf(rad);
        float fwdZ = -cosf(rad);
        float rgtX = cosf(rad);
        float rgtZ = sinf(rad);

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            g_camera.cx += fwdX * MOVE_SPEED * delta_step;
            g_camera.cz += fwdZ * MOVE_SPEED * delta_step;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            g_camera.cx -= fwdX * MOVE_SPEED * delta_step;
            g_camera.cz -= fwdZ * MOVE_SPEED * delta_step;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            g_camera.cx -= rgtX * MOVE_SPEED * delta_step;
            g_camera.cz -= rgtZ * MOVE_SPEED * delta_step;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            g_camera.cx += rgtX * MOVE_SPEED * delta_step;
            g_camera.cz += rgtZ * MOVE_SPEED * delta_step;
        }

        if (mouse_x < margin) { g_camera.cx -= rgtX * MOVE_SPEED * delta_step; g_camera.cz -= rgtZ * MOVE_SPEED * delta_step; }
        if (mouse_x > screen_w - margin) { g_camera.cx += rgtX * MOVE_SPEED * delta_step; g_camera.cz += rgtZ * MOVE_SPEED * delta_step; }
        if (mouse_y < margin) { g_camera.cx += fwdX * MOVE_SPEED * delta_step; g_camera.cz += fwdZ * MOVE_SPEED * delta_step; }
        if (mouse_y > screen_h - margin) { g_camera.cx -= fwdX * MOVE_SPEED * delta_step; g_camera.cz -= fwdZ * MOVE_SPEED * delta_step; }

        if (g_camera.cx < -MAP_LIMIT) g_camera.cx = -MAP_LIMIT;
        if (g_camera.cx > MAP_LIMIT) g_camera.cx = MAP_LIMIT;
        if (g_camera.cz < -MAP_LIMIT) g_camera.cz = -MAP_LIMIT;
        if (g_camera.cz > MAP_LIMIT) g_camera.cz = MAP_LIMIT;

        if (delta_step > 0.05f) delta_step = 0.05f;

        if (!g_game.gameOver) g_game.update(delta_step);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float light_pos[] = { 0.0f, 50.0f, 20.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

        g_camera.apply();
        g_game.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}