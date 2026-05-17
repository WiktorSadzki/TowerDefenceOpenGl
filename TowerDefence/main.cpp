#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <iostream>
#include <cmath>
#include "Game.h"
#include "Camera.h"

static Game   g_game;
static Camera g_camera;
static double g_last_mouse_x = 0;
static double g_last_mouse_y = 0;
static bool   g_mouse_left_down = false;
static bool   g_mouse_right_down = false;
static bool   g_mouse_has_moved = false;
static float  g_previous_time = 0.0f;
const float   MAP_BOUNDARY = 45.0f;
const float   CAMERA_MOVE_SPEED = 30.0f;

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
        g_mouse_left_down = (action == GLFW_PRESS);
        if (action == GLFW_PRESS) {
            g_mouse_has_moved = false;
        }
        if (action == GLFW_RELEASE && !g_mouse_has_moved) {
            double current_x, current_y;
            glfwGetCursorPos(window, &current_x, &current_y);
            g_game.tryPlaceTower((int)current_x, (int)current_y, g_camera);
        }
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE && !g_mouse_has_moved) {
        if (g_game.isBuilding) {
            double x_coord, y_coord;
            glfwGetCursorPos(window, &x_coord, &y_coord);
            g_game.tryPlaceTower((int)x_coord, (int)y_coord, g_camera);
            g_game.isBuilding = false;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        g_mouse_right_down = (action == GLFW_PRESS);
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    float delta_x = (float)(xpos - g_last_mouse_x);
    float delta_y = (float)(ypos - g_last_mouse_y);

    if (g_mouse_left_down) {
        g_mouse_has_moved = true;
        float radian_yaw = g_camera.angleY * 3.14159f / 180.0f;
        float forward_x = sinf(radian_yaw);
        float forward_z = -cosf(radian_yaw);
        float right_x = cosf(radian_yaw);
        float right_z = sinf(radian_yaw);
        float drag_sensitivity = g_camera.zoom * 0.002f;

        g_camera.cx -= right_x * delta_x * drag_sensitivity;
        g_camera.cz -= right_z * delta_x * drag_sensitivity;
        g_camera.cx += forward_x * delta_y * drag_sensitivity;
        g_camera.cz += forward_z * delta_y * drag_sensitivity;
    }
    if (g_mouse_right_down) {
        g_camera.handleMouse((int)delta_x, (int)delta_y);
    }

    g_last_mouse_x = xpos;
    g_last_mouse_y = ypos;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (yoffset > 0) g_camera.zoom = fmaxf(5.0f, g_camera.zoom - 1.0f);
    else if (yoffset < 0) g_camera.zoom = fminf(50.0f, g_camera.zoom + 1.0f);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
        if (key == GLFW_KEY_R) {
            g_game.init();
            g_camera.cx = 0.0f;
            g_camera.cz = 0.0f;
        }
        if (key == GLFW_KEY_SPACE) g_game.startNextWave();

        if (key == GLFW_KEY_1) g_game.selectTowerType(0);
        if (key == GLFW_KEY_2) g_game.selectTowerType(1);
        if (key == GLFW_KEY_3) g_game.selectTowerType(2);

        if (key == GLFW_KEY_1) g_game.toggleBuildMode(0);
        if (key == GLFW_KEY_2) g_game.toggleBuildMode(1);
        if (key == GLFW_KEY_3) g_game.toggleBuildMode(2);
    }
}

int main(int argc, char** argv) {
    if (!glfwInit()) return -1;

    int glut_argc = 1;
    char* glut_argv[] = { (char*)"TowerDefense" };
    glutInit(&glut_argc, glut_argv);

    GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Tower Defense 3D", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }

    glfwSetWindowPos(window, 0, 0);

    glfwMakeContextCurrent(window);
    glfwGetCursorPos(window, &g_last_mouse_x, &g_last_mouse_y);

    glViewport(0, 0, mode->width, mode->height);

    if (glewInit() != GLEW_OK) return -1;

    int screen_width, screen_height;
    glfwGetFramebufferSize(window, &screen_width, &screen_height);
    framebuffer_size_callback(window, screen_width, screen_height);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);

    g_game.init();
    g_previous_time = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float current_time_stamp = (float)glfwGetTime();
        float frame_delta = current_time_stamp - g_previous_time;
        g_previous_time = current_time_stamp;

        if (frame_delta > 0.05f) frame_delta = 0.05f;

        double cursor_x, cursor_y;
        int win_w, win_h;
        glfwGetCursorPos(window, &cursor_x, &cursor_y);
        glfwGetWindowSize(window, &win_w, &win_h);

        g_game.hud_win_w = win_w;
        g_game.hud_win_h = win_h;

        float yaw_rad = g_camera.angleY * 3.14159f / 180.0f;
        float dir_fwd_x = sinf(yaw_rad);
        float dir_fwd_z = -cosf(yaw_rad);
        float dir_rgt_x = cosf(yaw_rad);
        float dir_rgt_z = sinf(yaw_rad);

        const int screen_margin = 15;
        if (cursor_x < screen_margin) { g_camera.cx -= dir_rgt_x * CAMERA_MOVE_SPEED * frame_delta; g_camera.cz -= dir_rgt_z * CAMERA_MOVE_SPEED * frame_delta; }
        if (cursor_x > win_w - screen_margin) { g_camera.cx += dir_rgt_x * CAMERA_MOVE_SPEED * frame_delta; g_camera.cz += dir_rgt_z * CAMERA_MOVE_SPEED * frame_delta; }
        if (cursor_y < screen_margin) { g_camera.cx += dir_fwd_x * CAMERA_MOVE_SPEED * frame_delta; g_camera.cz += dir_fwd_z * CAMERA_MOVE_SPEED * frame_delta; }
        if (cursor_y > win_h - screen_margin) { g_camera.cx -= dir_fwd_x * CAMERA_MOVE_SPEED * frame_delta; g_camera.cz -= dir_fwd_z * CAMERA_MOVE_SPEED * frame_delta; }

        if (g_camera.cx < -MAP_BOUNDARY) g_camera.cx = -MAP_BOUNDARY;
        if (g_camera.cx > MAP_BOUNDARY) g_camera.cx = MAP_BOUNDARY;
        if (g_camera.cz < -MAP_BOUNDARY) g_camera.cz = -MAP_BOUNDARY;
        if (g_camera.cz > MAP_BOUNDARY) g_camera.cz = MAP_BOUNDARY;

        if (!g_game.gameOver) g_game.update(frame_delta);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLfloat light_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f }; 
        GLfloat light_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f }; 
        GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f }; 

        glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

        g_camera.apply();
        g_game.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}