#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <iostream>
#include <cmath>
#include "Game.h"
#include "Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

static Game   g_game;
static Camera g_camera;
static double g_last_mouse_x = 0;
static double g_last_mouse_y = 0;
static bool   g_mouse_left_down = false;
static bool   g_mouse_right_down = false;
static bool   g_mouse_has_moved = false;
static float  g_previous_time = 0.0f;
const float   MAP_BOUNDARY = 45.0f;
const float   CAMERA_MOVE_SPEED = 60.0f;

// Update projection matrix on window resize
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (height == 0) height = 1;
	// Calculate aspect ratio and set perspective projection
    glm::mat4 projection_matrix = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 200.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection_matrix));
    glMatrixMode(GL_MODELVIEW);
}

// Handles mouse clicks for UI interaction, Camera Control, and Tower Placement
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	// Left click logic (used for both camera dragging and tower placement)
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        g_mouse_left_down = (action == GLFW_PRESS);

        // When the user first clicks down, reset the drag tracker
        if (action == GLFW_PRESS) {
            g_mouse_has_moved = false;
        }

		// if the user releases the left mouse button and they haven't dragged the camera, attempt to place a tower
        if (action == GLFW_RELEASE && !g_mouse_has_moved) {
            if (g_game.isBuilding) {
                double current_x, current_y;
                glfwGetCursorPos(window, &current_x, &current_y);

                // Fire a raycast into the 3D world to figure out what grid tile they clicked
                g_game.tryPlaceTower((int)current_x, (int)current_y, g_camera);

                // Exit build mode
                g_game.isBuilding = false;
            }
        }
    }

	// Right click logic (used for camera rotation)
    else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        // Track whether the right mouse button is being held down (used for camera rotation)
        g_mouse_right_down = (action == GLFW_PRESS);
    }
}

// Handles mouse movement for both camera dragging and rotation
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    float delta_x = (float)(xpos - g_last_mouse_x);
    float delta_y = (float)(ypos - g_last_mouse_y);

    if (g_mouse_left_down) {
        g_mouse_has_moved = true;
		float radian_yaw = g_camera.angleY * 3.14159f / 180.0f; // Convert camera yaw to radians for movement vector calculation
        
		// Calculate forward and right vectors based on camera yaw
        float forward_x = sinf(radian_yaw);
        float forward_z = -cosf(radian_yaw);

		// Right vector is perpendicular to forward vector on the XZ plane
        float right_x = cosf(radian_yaw);
        float right_z = sinf(radian_yaw);

		// Scale movement speed based on zoom level for consistent feel at different zooms
        float drag_sensitivity = g_camera.zoom * 0.002f;

		// Apply movement in the right and forward directions based on mouse movement, scaled by sensitivity
        g_camera.cx -= right_x * delta_x * drag_sensitivity;
        g_camera.cz -= right_z * delta_x * drag_sensitivity;
        g_camera.cx += forward_x * delta_y * drag_sensitivity;
        g_camera.cz += forward_z * delta_y * drag_sensitivity;
    }
	// If the right mouse button is held down, rotate the camera based on mouse movement
    if (g_mouse_right_down) {
        g_camera.handleMouse((int)delta_x, (int)delta_y);
    }
    g_last_mouse_x = xpos;
    g_last_mouse_y = ypos;
}

// Handles mouse scroll for zooming in and out, with limits to prevent excessive zooming
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (yoffset > 0) g_camera.zoom = fmaxf(5.0f, g_camera.zoom - 1.0f);
    else if (yoffset < 0) g_camera.zoom = fminf(50.0f, g_camera.zoom + 1.0f);
}

// Handles keyboard input for game controls such as starting waves, selecting towers, pausing, and changing game speed
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true); // Escape key to exit the game
		if (key == GLFW_KEY_R) { // R key to reset the game
            g_game.init();
            g_camera.cx = 0.0f;
            g_camera.cz = 0.0f;
        }
		if (key == GLFW_KEY_SPACE) g_game.startNextWave(); // Space key to start the next wave of enemies

		// Select tower types with number keys (1, 2, 3) for Machine Gun, Rockets, and Sniper respectively
        if (key == GLFW_KEY_1) g_game.selectTowerType(0);
        if (key == GLFW_KEY_2) g_game.selectTowerType(1);
        if (key == GLFW_KEY_3) g_game.selectTowerType(2);

		// Speed and pause controls: P to toggle pause, F to cycle through game speeds (1x, 2x, 3x)
        if (key == GLFW_KEY_P) g_game.paused = !g_game.paused;
        if (key == GLFW_KEY_F) {
            auto& s = g_game.gameSpeed;
            s = (s == 1.0f) ? 2.0f : (s == 2.0f) ? 3.0f : 1.0f;
        }
    }
}

int main(int argc, char** argv) {
	if (!glfwInit()) return -1; // GLFW initialization

	// Initialize GLUT for text rendering in the HUD
    int glut_argc = 1;
    char* glut_argv[] = { (char*)"TowerDefense" };
    glutInit(&glut_argc, glut_argv);

	// Fullscreen window setup using the primary monitor's video mode
    GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);

	// Disable window decorations and resizing for a clean fullscreen experience
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	// Create a fullscreen window with the dimensions of the primary monitor
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Tower Defense 3D", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }

	glfwSetWindowPos(window, 0, 0); // Position the window at the top-left corner of the screen
    glfwMakeContextCurrent(window);
    glfwGetCursorPos(window, &g_last_mouse_x, &g_last_mouse_y);

    glViewport(0, 0, mode->width, mode->height);

	if (glewInit() != GLEW_OK) return -1; // Initialize GLEW for OpenGL function loading

	// Set up the initial projection matrix based on the window size
    int screen_width, screen_height;
    glfwGetFramebufferSize(window, &screen_width, &screen_height);
    framebuffer_size_callback(window, screen_width, screen_height);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

	// HUD setup
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	glEnable(GL_BLEND); // Enable transparency for HUD elements
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Set blending function for proper alpha blending
	glClearColor(0.1f, 0.1f, 0.2f, 1.0f); // Set a dark background color for the game

	// Initialize game state and timing
    g_game.init();
    g_previous_time = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
		// Calculate frame delta time for smooth movement and animations, with a cap to prevent large jumps
        float current_time_stamp = (float)glfwGetTime();
        float frame_delta = current_time_stamp - g_previous_time;
        g_previous_time = current_time_stamp;

		// Cap the frame delta to maxium of 50ms
        if (frame_delta > 0.05f) frame_delta = 0.05f;

		// Get current cursor position and window size for camera movement based on mouse position at the edges of the screen
        double cursor_x, cursor_y;
        int win_w, win_h;
        glfwGetCursorPos(window, &cursor_x, &cursor_y);
        glfwGetWindowSize(window, &win_w, &win_h);

        g_game.hud_win_w = win_w;
        g_game.hud_win_h = win_h;

		// Calculate forward and right movement directions based on camera yaw for edge scrolling and WASD movement
        float yaw_rad = g_camera.angleY * 3.14159f / 180.0f;
        float dir_fwd_x = sinf(yaw_rad);
        float dir_fwd_z = -cosf(yaw_rad);
        float dir_rgt_x = cosf(yaw_rad);
        float dir_rgt_z = sinf(yaw_rad);

		// Move camera if cursor is near the edges of the screen for edge scrolling
        const int screen_margin = 15;
        if (cursor_x < screen_margin) { g_camera.cx -= dir_rgt_x * CAMERA_MOVE_SPEED * frame_delta; g_camera.cz -= dir_rgt_z * CAMERA_MOVE_SPEED * frame_delta; }
        if (cursor_x > win_w - screen_margin) { g_camera.cx += dir_rgt_x * CAMERA_MOVE_SPEED * frame_delta; g_camera.cz += dir_rgt_z * CAMERA_MOVE_SPEED * frame_delta; }
        if (cursor_y < screen_margin) { g_camera.cx += dir_fwd_x * CAMERA_MOVE_SPEED * frame_delta; g_camera.cz += dir_fwd_z * CAMERA_MOVE_SPEED * frame_delta; }
        if (cursor_y > win_h - screen_margin) { g_camera.cx -= dir_fwd_x * CAMERA_MOVE_SPEED * frame_delta; g_camera.cz -= dir_fwd_z * CAMERA_MOVE_SPEED * frame_delta; }

		// Camera movement with WASD keys
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            g_camera.cx -= dir_rgt_x * CAMERA_MOVE_SPEED * frame_delta;
            g_camera.cz -= dir_rgt_z * CAMERA_MOVE_SPEED * frame_delta;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            g_camera.cx += dir_rgt_x * CAMERA_MOVE_SPEED * frame_delta;
            g_camera.cz += dir_rgt_z * CAMERA_MOVE_SPEED * frame_delta;
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            g_camera.cx += dir_fwd_x * CAMERA_MOVE_SPEED * frame_delta;
            g_camera.cz += dir_fwd_z * CAMERA_MOVE_SPEED * frame_delta;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            g_camera.cx -= dir_fwd_x * CAMERA_MOVE_SPEED * frame_delta;
            g_camera.cz -= dir_fwd_z * CAMERA_MOVE_SPEED * frame_delta;
        }

		// Clamp camera position to stay within defined map boundaries
        if (g_camera.cx < -MAP_BOUNDARY) g_camera.cx = -MAP_BOUNDARY;
        if (g_camera.cx > MAP_BOUNDARY) g_camera.cx = MAP_BOUNDARY;
        if (g_camera.cz < -MAP_BOUNDARY) g_camera.cz = -MAP_BOUNDARY;
        if (g_camera.cz > MAP_BOUNDARY) g_camera.cz = MAP_BOUNDARY;

		// Update game state if not paused or game over
        if (!g_game.gameOver && !g_game.paused)
            g_game.update(frame_delta * g_game.gameSpeed);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the screen and depth buffer for the new frame

		// Projection matrix setup for 3D rendering with a perspective projection
        glm::mat4 P = glm::perspective(glm::radians(45.0f), (float)win_w / (float)win_h, 0.1f, 200.0f);
        
        float pitchRad = glm::radians(g_camera.angleX);
        float yawRad = glm::radians(g_camera.angleY);

        glm::vec3 camera_target = glm::vec3(g_camera.cx, 0.0f, g_camera.cz);

		// Calculate camera eye position in spherical coordinates based on camera angles and zoom level, looking at the target point on the ground
        float eyeX = camera_target.x - g_camera.zoom * sin(yawRad) * cos(pitchRad);
        float eyeY = camera_target.y + g_camera.zoom * sin(pitchRad);
        float eyeZ = camera_target.z + g_camera.zoom * cos(yawRad) * cos(pitchRad);

        glm::vec3 camera_eye = glm::vec3(eyeX, eyeY, eyeZ);

		// View matrix setup using glm::lookAt to create a view transformation from the camera's eye position to the target point, with an up vector pointing upwards
        glm::mat4 V = glm::lookAt(camera_eye, camera_target, glm::vec3(0.0f, 1.0f, 0.0f));

        g_game.render(P, V);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}