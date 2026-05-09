#define GLEW_STATIC
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
static float  g_lastTime = 0.0f;

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
        if (action == GLFW_RELEASE) {
            double x_coord, y_coord;
            glfwGetCursorPos(window, &x_coord, &y_coord);
            g_game.tryPlaceTower((int)x_coord, (int)y_coord, g_camera);
        }
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (g_mouseDown) {
        float x_offset = (float)(xpos - g_lastMouseX);
        float y_offset = (float)(ypos - g_lastMouseY);
        g_camera.handleMouse((int)x_offset, (int)y_offset);
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
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;

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

    float light_pos[] = { 10.0f, 20.0f, 10.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    g_game.init();
    g_lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float current_frame_time = (float)glfwGetTime();
        float delta_step = current_frame_time - g_lastTime;
        g_lastTime = current_frame_time;

        if (delta_step > 0.05f) delta_step = 0.05f;

        if (!g_game.gameOver) g_game.update(delta_step);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        g_camera.apply();
        g_game.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}