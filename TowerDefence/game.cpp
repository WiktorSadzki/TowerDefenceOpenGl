#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Game.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glut.h>

// Spawn interval decreases each wave to speed up troop spawning
static float spawn_interval = 2.0f;
// Buffer distance to prevent towers from being placed too close to the road, which would look bad and cause clipping issues
static const float ROAD_PLACEMENT_BUFFER = 2.13f;
// Minimum distance between towers to prevent them from overlapping each other
static const float TOWER_EXCLUSION_RADIUS_SQ = 18.9f;

// Reads textures from disk
GLuint Game::readTexture(const char* filename) {
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true); // Flip textures vertically on load to match OpenGL's coordinate system
    unsigned char* texture_pixels = stbi_load(filename, &w, &h, &ch, 4);
    if (!texture_pixels) {
        std::cout << "Texture load error: " << filename << std::endl;
        return 0;
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Set texture parameters for filtering and wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(texture_pixels);
    return tex;
}

// Uploads full mesh vertex array to GPU
MeshBuffer Game::uploadMesh(const std::vector<VertexData>& verts) {
    MeshBuffer mb;
    mb.count = (int)verts.size();
    if (mb.count == 0) return mb;
    glGenVertexArrays(1, &mb.vao);
    glGenBuffers(1, &mb.vbo);
    glBindVertexArray(mb.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mb.vbo);
    glBufferData(GL_ARRAY_BUFFER, mb.count * sizeof(VertexData), verts.data(), GL_STATIC_DRAW);
    const GLsizei stride = sizeof(VertexData);
    // Set up vertex attribute pointers for position, normal, color, and texture coordinates
    // Position attribute (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, x));
    // Normal attribute (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, nx));
    // Color attribute (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, r));
    // Texture coordinate attribute (location = 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, u));
    // Tangent attribute (location = 4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, tx));
    glBindVertexArray(0);
    return mb;
}

// Upload simple line loop vertex array to GPU for drawing road paths and tower placement circles
MeshBuffer Game::uploadLineLoop(const std::vector<float>& xyzs) {
    MeshBuffer mb;
    mb.count = (int)(xyzs.size() / 3);
    if (mb.count == 0) return mb;

    // Only upload position data for line loops, no normals, colors, or texture coordinates needed
    glGenVertexArrays(1, &mb.vao);
    glGenBuffers(1, &mb.vbo); // Generate VAO and VBO for the line loop
    glBindVertexArray(mb.vao); // Bind the VAO to set up vertex attribute pointers
    glBindBuffer(GL_ARRAY_BUFFER, mb.vbo); // Bind the VBO to upload vertex data
    glBufferData(GL_ARRAY_BUFFER, xyzs.size() * sizeof(float), xyzs.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
    return mb;
}

// Draws an initialized mesh buffer
void Game::drawMesh(const MeshBuffer& buf) {
    if (buf.vao == 0 || buf.count == 0) return;
    glBindVertexArray(buf.vao);
    glDrawArrays(GL_TRIANGLES, 0, buf.count);
    glBindVertexArray(0);
}

// Flattens indexed road mesh into basic vertex buffer
static MeshBuffer uploadRoad(const Road& road) {
    std::vector<VertexData> flat;
    flat.reserve(road.indices.size());
    for (unsigned int idx : road.indices) {
        const Vertex& rv = road.vertices[idx];
        VertexData vd{};
        vd.x = rv.Position.x; vd.y = rv.Position.y; vd.z = rv.Position.z;
        vd.nx = 0; vd.ny = 1; vd.nz = 0; // Road normals are all up since it's flat
        vd.r = 1; vd.g = 1; vd.b = 1;
        vd.u = rv.TexCoords.x; vd.v = rv.TexCoords.y;
        vd.tx = 1.0f; vd.ty = 0.0f; vd.tz = 0.0f;
        flat.push_back(vd);
    }

    // Upload the flattened vertex data to the GPU and return the mesh buffer
    MeshBuffer mb;
    mb.count = (int)flat.size();
    glGenVertexArrays(1, &mb.vao);
    glGenBuffers(1, &mb.vbo);
    glBindVertexArray(mb.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mb.vbo);
    glBufferData(GL_ARRAY_BUFFER, flat.size() * sizeof(VertexData), flat.data(), GL_STATIC_DRAW);
    const GLsizei stride = sizeof(VertexData);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, nx));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, r));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, u));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, tx));
    glBindVertexArray(0);
    return mb;
}

// Binds grouped textures to standard uniform locations
static void bindTextures(GLuint shader, const TextureBundle& bundle_config) {
    // If no base color texture is present, skip binding and set flag to disable texturing in shader
    if (bundle_config.baseColor == 0) {
        glUniform1f(glGetUniformLocation(shader, "hasBaseColor"), 0.0f);
        return;
    }
    // Bind each texture in the bundle to a specific texture unit and set corresponding uniform flags for shader
    glUniform1i(glGetUniformLocation(shader, "texBaseColor"), 0);
    glUniform1i(glGetUniformLocation(shader, "texEmissive"), 1);
    glUniform1i(glGetUniformLocation(shader, "texNormal"), 2);
    glUniform1i(glGetUniformLocation(shader, "texMetallic"), 3);
    glUniform1i(glGetUniformLocation(shader, "texRoughness"), 4);
    glUniform1i(glGetUniformLocation(shader, "texAO"), 5);
    glUniform1i(glGetUniformLocation(shader, "texHeight"), 6);

    // Helper lambda to bind a texture to a unit and set the corresponding "has" flag in the shader
    auto bind = [&](GLenum unit, GLuint tex, const char* flag) {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1f(glGetUniformLocation(shader, flag), tex ? 1.0f : 0.0f);
        };
    bind(GL_TEXTURE0, bundle_config.baseColor, "hasBaseColor");
    bind(GL_TEXTURE1, bundle_config.emissive, "hasEmissive");
    bind(GL_TEXTURE2, bundle_config.normalMap, "hasNormal");
    bind(GL_TEXTURE3, bundle_config.metallic, "hasMetallic");
    bind(GL_TEXTURE4, bundle_config.roughness, "hasRoughness");
    bind(GL_TEXTURE5, bundle_config.ao, "hasAO");
    bind(GL_TEXTURE6, bundle_config.height, "hasHeight");

    // Reset active texture to default unit after binding
    glActiveTexture(GL_TEXTURE0);
}

// Disables rendering textures in uniform limits
static void unbindTextures(GLuint shader) {
    for (int i = 0; i < 6; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    static const char* flags[] = {
        "hasBaseColor","hasEmissive","hasNormal",
        "hasMetallic","hasRoughness","hasAO","hasHeight"
    };
    for (auto f : flags) {
        glUniform1f(glGetUniformLocation(shader, f), 0.0f);
    }
}

// Assigns model uniforms given view and model transform matrices
// Normal matrix is transposed inverse of upper-left 3x3 of ModelView matrix to correct non-uniform scaling on normals
static void setModel(GLuint shader, const glm::mat4& V, const glm::mat4& M) {
    glUniformMatrix4fv(glGetUniformLocation(shader, "M"), 1, GL_FALSE, glm::value_ptr(M));
    glm::mat3 N = glm::transpose(glm::inverse(glm::mat3(V * M)));
    glUniformMatrix3fv(glGetUniformLocation(shader, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(N));
}

// Returns default tower instance values based on tower type, which can be modified for upgrades or special abilities later
TowerInstance Game::getTowerDefaults(TowerType type) {
    TowerInstance t;
    t.tower_variant = type;

    // Base stats for machine gun tower, which is the default type
    t.cost = 20;
    t.tower_rotate_speed = 15.0f;
    t.tower_fire_rate = 0.15f;
    t.tower_dmg = 20.0f;
    t.tower_range = 10.0f;

    // Adjust stats for rockets and sniper towers
    if (type == TowerType::ROCKETS) {
        t.cost = 30; t.tower_rotate_speed = 8.0f;
        t.tower_fire_rate = 0.08f; t.tower_dmg = 50.0f; t.tower_range = 8.0f;
    }
    else if (type == TowerType::SNIPER) {
        t.cost = 25; t.tower_rotate_speed = 25.0f;
        t.tower_fire_rate = 0.12f; t.tower_dmg = 35.0f; t.tower_range = 12.0f;
    }
    return t;
}

// Initializes game state, loads models and textures, and sets up mesh buffers for rendering
void Game::init() {
    lives = 20;
    gold = 200;
    wave = 0;
    waveActive = false;
    gameOver = false;
    isBuilding = false;
    ghost_wx = 0.0f;
    ghost_wz = 0.0f;
    active_defenses.clear();
    active_bullets.clear();
    troops.clear();
    pathWaypoints.clear();
    selectedType = TowerType::MACHINE_GUN;

    // Convert road spline points to simple 2D waypoints for troop movement logic, ignoring y since road is flat
    for (const auto& splinePt : game_pathway.splinePoints) {
        P sharpPt;
        sharpPt.x = splinePt.x;
        sharpPt.z = splinePt.z;
        pathWaypoints.push_back(sharpPt);
    }

    // Load tower models and textures, and set vertical offsets for each part to ensure proper stacking when rendering
    std::string tower_dir = "Assets/Towers/source/";
    loadModels(tower_dir, "base.obj", tower_assets[0].base_mesh);
    loadModels(tower_dir, "machine_rotate.obj", tower_assets[0].rotate_mesh);
    loadModels(tower_dir, "machine_gun.obj", tower_assets[0].gun_mesh);
    tower_assets[0].base_y_offset = 0.4f;
    tower_assets[0].rotate_y_offset = 0.8f;
    tower_assets[0].gun_y_offset = 1.8f;

    // Rocket and sniper towers use the same base model as machine gun, but different rotating and gun parts, so we can reuse the base texture for all three types
    loadModels(tower_dir, "base.obj", tower_assets[1].base_mesh);
    loadModels(tower_dir, "rockets_rotate.obj", tower_assets[1].rotate_mesh);
    loadModels(tower_dir, "rockets_gun.obj", tower_assets[1].gun_mesh);
    tower_assets[1].base_y_offset = 0.4f;
    tower_assets[1].rotate_y_offset = 2.8f;
    tower_assets[1].gun_y_offset = 0.6f;

    loadModels(tower_dir, "base.obj", tower_assets[2].base_mesh);
    loadModels(tower_dir, "sniper_rotate.obj", tower_assets[2].rotate_mesh);
    loadModels(tower_dir, "sniper_gun.obj", tower_assets[2].gun_mesh);
    tower_assets[2].base_y_offset = 0.4f;
    tower_assets[2].rotate_y_offset = 0.8f;
    tower_assets[2].gun_y_offset = 1.8f;

    std::string troop_dir = "Assets/Troops/source/";
    loadModels(troop_dir, "car.obj", troop_assets[0].base_mesh);
    loadModels(troop_dir, "tank.obj", troop_assets[1].base_mesh);
    loadModels(troop_dir, "helicopter.obj", troop_assets[2].base_mesh);
    loadModels(troop_dir, "Wheel.obj", troop_assets[1].wheel_mesh);
    loadModels(troop_dir, "Propellers.obj", troop_assets[2].prop_mesh);

    std::string ammo_dir = "Assets/Ammo/source/";
    loadModels(ammo_dir, "Bullet.obj", projectile_assets.bullet_mesh);
    loadModels(ammo_dir, "Rocket.obj", projectile_assets.rocket_mesh);

    // Upload all meshes to GPU and store mesh buffers for rendering
    for (int i = 0; i < 3; i++) {
        tower_assets[i].base_buf = uploadMesh(tower_assets[i].base_mesh);
        tower_assets[i].rotate_buf = uploadMesh(tower_assets[i].rotate_mesh);
        tower_assets[i].gun_buf = uploadMesh(tower_assets[i].gun_mesh);
    }
    for (int i = 0; i < 3; i++) {
        troop_assets[i].base_buf = uploadMesh(troop_assets[i].base_mesh);
        troop_assets[i].wheel_buf = uploadMesh(troop_assets[i].wheel_mesh);
        troop_assets[i].prop_buf = uploadMesh(troop_assets[i].prop_mesh);
    }
    projectile_assets.bullet_buf = uploadMesh(projectile_assets.bullet_mesh);
    projectile_assets.rocket_buf = uploadMesh(projectile_assets.rocket_mesh);

    // Create a large flat plane for the ground and a simple line loop for drawing the road path and tower placement circles
    const float S = 200.0f, Y = -0.01f, T = 12.0f;
    std::vector<VertexData> gv = {
        {-S,Y,-S, 0,1,0, 1,1,1, 0,0, 1,0,0},
        { S,Y,-S, 0,1,0, 1,1,1, T,0, 1,0,0},
        { S,Y, S, 0,1,0, 1,1,1, T,T, 1,0,0},
        {-S,Y,-S, 0,1,0, 1,1,1, 0,0, 1,0,0},
        { S,Y, S, 0,1,0, 1,1,1, T,T, 1,0,0},
        {-S,Y, S, 0,1,0, 1,1,1, 0,T, 1,0,0},
    };
    groundBuf = uploadMesh(gv);

    // The road mesh is generated from the Road class, which creates a flat mesh along the defined spline path. We flatten the indexed mesh into a simple vertex buffer for easier rendering with our basic shader.
    roadBuf = uploadRoad(game_pathway);

    // Create a simple circle geometry for drawing tower placement range indicators, consisting of 36 points around a unit circle in the XZ plane
    std::vector<float> circle;
    for (int i = 0; i < 36; i++) {
        float a = i * 10.0f * 3.14159f / 180.0f;
        circle.push_back(cosf(a));
        circle.push_back(0.0f);
        circle.push_back(sinf(a));
    }
    circleBuf = uploadLineLoop(circle);

    // Load shaders for rendering the main scene and the HUD, which are simple textured shaders that use the provided vertex attributes and texture bindings
    shader_id = loadShader("v_simplest.glsl", "f_simplest.glsl");
    hud_shader = loadShader("v_hud.glsl", "f_hud.glsl");

    // Load textures for towers, troops, and projectiles, grouping them into TextureBundle structs for easier binding during rendering. Each tower type has its own rotating and gun textures, but they can share the same base texture since the base model is the same for all three types.
    TextureBundle baseTex = {};
    baseTex.baseColor = readTexture("Assets/Towers/textures/base/Base_BaseColor_A.png");
    baseTex.normalMap = readTexture("Assets/Towers/textures/base/Base_GL_Normal.png");
    baseTex.metallic = readTexture("Assets/Towers/textures/base/Base_Metallic.png");
    baseTex.roughness = readTexture("Assets/Towers/textures/base/Base_Roughness.png");
    baseTex.ao = readTexture("Assets/Towers/textures/base/Base_AO.png");
    tower_assets[0].base_tex = baseTex;
    tower_assets[1].base_tex = baseTex;
    tower_assets[2].base_tex = baseTex;

    TextureBundle mgTex = {};
    mgTex.baseColor = readTexture("Assets/Towers/textures/machinegun/MachineGun_BaseColor_A.png");
    mgTex.normalMap = readTexture("Assets/Towers/textures/machinegun/MachineGun_GL_Normal.png");
    mgTex.metallic = readTexture("Assets/Towers/textures/machinegun/MachineGun_Metallic.png");
    mgTex.roughness = readTexture("Assets/Towers/textures/machinegun/MachineGun_Roughness.png");
    tower_assets[0].rotate_tex = mgTex;
    tower_assets[0].gun_tex = mgTex;

    TextureBundle rocketTowerTex = {};
    rocketTowerTex.baseColor = readTexture("Assets/Towers/textures/rocket/Rockets_BaseColor_A.png");
    rocketTowerTex.normalMap = readTexture("Assets/Towers/textures/rocket/Rockets_GL_Normal.png");
    rocketTowerTex.metallic = readTexture("Assets/Towers/textures/rocket/Rockets_Metallic.png");
    rocketTowerTex.roughness = readTexture("Assets/Towers/textures/rocket/Rockets_Roughness.png");
    tower_assets[1].rotate_tex = rocketTowerTex;
    tower_assets[1].gun_tex = rocketTowerTex;

    TextureBundle sniperTex = {};
    sniperTex.baseColor = readTexture("Assets/Towers/textures/sniper/Sniper_BaseColor_A.png");
    sniperTex.normalMap = readTexture("Assets/Towers/textures/sniper/Sniper_GL_Normal.png");
    sniperTex.metallic = readTexture("Assets/Towers/textures/sniper/Sniper_Metallic.png");
    sniperTex.roughness = readTexture("Assets/Towers/textures/sniper/Sniper_Roughness.png");
    tower_assets[2].rotate_tex = sniperTex;
    tower_assets[2].gun_tex = sniperTex;

    TextureBundle carTex = {};
    carTex.baseColor = readTexture("Assets/Troops/textures/car/Car_Base_Color.jpg");
    carTex.normalMap = readTexture("Assets/Troops/textures/car/Car_Normal.png");
    carTex.metallic = readTexture("Assets/Troops/textures/car/Car_Metalic.jpg");
    carTex.roughness = readTexture("Assets/Troops/textures/car/Car_Roughness.jpg");
    carTex.emissive = readTexture("Assets/Troops/textures/car/Car_Emissive.jpg");
    carTex.ao = readTexture("Assets/Troops/textures/car/Car_AO.jpg");
    troop_assets[0].base_tex = carTex;

    TextureBundle tankTex = {};
    tankTex.baseColor = readTexture("Assets/Troops/textures/tank/Tank_Base_Color.png");
    tankTex.normalMap = readTexture("Assets/Troops/textures/tank/Tank_Normal.png");
    tankTex.metallic = readTexture("Assets/Troops/textures/tank/Tank_Metallic.png");
    tankTex.roughness = readTexture("Assets/Troops/textures/tank/Tank_Roughness.png");
    tankTex.ao = readTexture("Assets/Troops/textures/tank/Tank_AO.png");
    troop_assets[1].base_tex = tankTex;

    TextureBundle heliTex = {};
    heliTex.baseColor = readTexture("Assets/Troops/textures/helicopter/DefaultMaterial_Base_Color.png");
    heliTex.normalMap = readTexture("Assets/Troops/textures/helicopter/DefaultMaterial_Normal_DirectX.png");
    heliTex.metallic = readTexture("Assets/Troops/textures/helicopter/DefaultMaterial_Metallic.png");
    heliTex.roughness = readTexture("Assets/Troops/textures/helicopter/DefaultMaterial_Roughness.png");
    heliTex.ao = readTexture("Assets/Troops/textures/helicopter/DefaultMaterial_Mixed_AO.png");
    troop_assets[2].base_tex = heliTex;

    TextureBundle bulletTex = {};
    bulletTex.baseColor = readTexture("Assets/Ammo/textures/Bullet_albedo.png");
    bulletTex.normalMap = readTexture("Assets/Ammo/textures/Bullet_Normal.png");
    bulletTex.metallic = readTexture("Assets/Ammo/textures/Bullet_Metallic.png");
    bulletTex.roughness = readTexture("Assets/Ammo/textures/Bullet_Roughness.png");
    bulletTex.ao = readTexture("Assets/Ammo/textures/Bullet_ao.png");
    projectile_assets.bullet_tex = bulletTex;

    TextureBundle rocketTex = {};
    rocketTex.baseColor = readTexture("Assets/Ammo/textures/Rocket_Base_Color.png");
    rocketTex.normalMap = readTexture("Assets/Ammo/textures/Rocket_Normal.png");
    rocketTex.emissive = readTexture("Assets/Ammo/textures/Rocket_Emissive.png");
    projectile_assets.rocket_tex = rocketTex;

    groundTex.baseColor = readTexture("Assets/Ground/textures/terrain_diff.jpg");
    groundTex.normalMap = readTexture("Assets/Ground/textures/terrain_normal.png");
    groundTex.roughness = readTexture("Assets/Ground/textures/terrain_rough.png");
    groundTex.metallic = readTexture("Assets/Ground/textures/terrain_spec.png");
    groundTex.height = readTexture("Assets/Ground/textures/terrain_height.png");

    pathTex.baseColor = readTexture("Assets/Ground/textures/path_diff.jpg");
    pathTex.normalMap = readTexture("Assets/Ground/textures/path_normal.jpg");
    pathTex.roughness = readTexture("Assets/Ground/textures/path_rough.png");
    pathTex.height = readTexture("Assets/Ground/textures/path_height.png");
}

// Initializes wave parameters and starts spawning troops, with increasing difficulty each wave by reducing spawn interval and increasing troop count
void Game::startNextWave() {
    if (waveActive || waveEndMessageTimer > 0.0f) return;
    waveActive = true;
    wave++;
    if (wave <= 3)
        troopsRemainingInWave = 10 + wave * 3;
    else
        troopsRemainingInWave = 10 + (wave * 4);
    spawnTimer = 0.0f;
    spawn_interval = 2.0f - (wave * 0.1f);
    if (spawn_interval < 0.1f) spawn_interval = 0.1f;
}

// Main game update loop, called every frame with the time delta since the last update. Handles troop spawning, movement, tower targeting and firing, and wave progression logic.
void Game::update(float delta_step) {
    if (gameOver) return;

    // Spawning enemies in waves with a timer, and checking for wave end conditions
    if (waveActive && troopsRemainingInWave > 0) {
        spawnTimer += delta_step;
        if (spawnTimer > spawn_interval) {
            spawnTroop();
            troopsRemainingInWave--;
            spawnTimer = 0.0f;
        }
    }
    // End wave if all troops have been spawned and defeated, with a timer for the "Wave Complete" message
    else if (troopsRemainingInWave <= 0 && troops.empty()) {
        if (waveActive) waveEndMessageTimer = 3.0f;
        waveActive = false;
    }

    if (waveEndMessageTimer > 0.0f) waveEndMessageTimer -= delta_step;

    // Troop movement along the path, with rotation towards the next waypoint and removal if they reach the end
    for (auto it = troops.begin(); it != troops.end(); ) {
        // Target coordinates for the current waypoint
        float tx = pathWaypoints[it->currentWaypoint].x;
        float tz = pathWaypoints[it->currentWaypoint].z;

        // Calculate distance to next waypoint and move towards it, rotating to face the direction of movement
        float dx = tx - it->x;
        float dz = tz - it->z;
        float dist = sqrtf(dx * dx + dz * dz);
        float moveDist = it->speed * delta_step;

        // Calculate rotation using arc tangent
        if (dist > 0.001f) {
            float targetA = atan2f(dx, dz) * 180.0f / 3.14159f; // Convert radians to degrees
            float diff = targetA - it->rotation_yaw;

            // Normalize the angle difference to stay within -180 to 180 degrees
            while (diff < -180) diff += 360;
            while (diff > 180) diff -= 360;
            it->rotation_yaw += diff * delta_step * 10.0f; // delta step - smooth rotation
        }

        // Move towards the waypoint, and if close enough, snap to it and advance to the next waypoint
        if (dist <= moveDist) {
            it->x = tx; it->z = tz;
            it->currentWaypoint++;
        }
        else {
            it->x += (dx / dist) * moveDist;
            it->z += (dz / dist) * moveDist;
        }

        // Subtract health if reached end of path, and remove troop from list
        if (it->currentWaypoint >= (int)pathWaypoints.size()) {
            lives--; it = troops.erase(it);
        }
        else ++it;
    }

    // Towers + fireing logic: find closest target in range, rotate towards it, and spawn projectiles if can
    for (size_t i = 0; i < active_defenses.size(); i++) {
        auto& tower = active_defenses[i];
        float range = tower.tower_range;

        if (tower.current_cooldown > 0.0f)
            tower.current_cooldown -= delta_step;

        // Find closest target in range
        Troop* target = nullptr;
        float  bestDist = range;
        for (auto& troop : troops) {
            float d = sqrtf(powf(troop.x - tower.x, 2) + powf(troop.z - tower.z, 2));
            if (d < bestDist) { bestDist = d; target = &troop; }
        }

        // If target found
        if (target) {
            const auto& assets = tower_assets[(int)tower.tower_variant];

            // Calculate distance and angles to target for aiming
            float adx = target->x - tower.x; // Horizontal distance to target
            float adz = target->z - tower.z; // Vertical distance to target
            float dist2D = sqrtf(adx * adx + adz * adz); // 2D distance for yaw calculation

            // Calculate the height difference to target for pitch calculation, using the gun pivot point
            float gunPivotY = assets.base_y_offset
                + 0.5f * assets.rotate_y_offset
                + 0.5f * assets.gun_y_offset;
            float ady = target->altitude - gunPivotY;

            // Calculate target yaw and pitch angles in degrees
            float targetYaw = atan2f(adx, adz) * (180.0f / 3.14159f);
            float targetPitch = atan2f(ady, dist2D) * (180.0f / 3.14159f);

            // Smoothly rotate tower towards target using a simple proportional controller
            float yDiff = targetYaw - tower.current_yaw;
            while (yDiff < -180) yDiff += 360;
            while (yDiff > 180) yDiff -= 360;
            tower.current_yaw += yDiff * delta_step * tower.tower_rotate_speed;
            tower.current_pitch += (targetPitch - tower.current_pitch)
                * delta_step * tower.tower_rotate_speed;

            float pitchDiff = targetPitch - tower.current_pitch;

            // Fire if within range, roughly facing target, and cooldown ready. Spawn projectile with initial velocity towards target.
            if (tower.current_cooldown <= 0.0f && fabs(yDiff) < 15.0f && fabs(pitchDiff) < 10.0f) {
                // Angle back to radians for forward vector calculation
                float yawRad = tower.current_yaw * (3.14159f / 180.0f);
                float pitchRad = tower.current_pitch * (3.14159f / 180.0f);

                // Calculate forward, right, and up vectors for projectile spawn position based on tower rotation using spherical coordinates
                float fwdX = sinf(yawRad) * cosf(pitchRad);
                float fwdY = sinf(pitchRad);
                float fwdZ = cosf(yawRad) * cosf(pitchRad);
                float rightX = cosf(yawRad);
                float rightZ = -sinf(yawRad);
                float upX = -sinf(yawRad) * sinf(pitchRad);
                float upY = cosf(pitchRad);
                float upZ = -cosf(yawRad) * sinf(pitchRad);

                // Muzzle offsets for different tower types
                float muzzleFwd = 0.6f;
                float muzzleRight = 0.0f;
                float muzzleUp = 0.0f;
                bool  isRocket = false;

                // Machine gun alternates between two barrels, rockets alternate between 8 tubes, and sniper fires from center
                if (tower.tower_variant == TowerType::MACHINE_GUN) {
                    const float gunRight[2] = { -0.32f, 0.32f };
                    muzzleFwd = 2.1f;
                    muzzleRight = gunRight[tower.nextBarrel % 2];
                    tower.nextBarrel = (tower.nextBarrel + 1) % 2;
                }
                else if (tower.tower_variant == TowerType::ROCKETS) {
                    const float tubeRight[8] = { -1.5f, 1.5f, -1.3f, 1.3f, -1.5f, 1.5f, -1.3f, 1.3f };
                    const float tubeUp[8] = { 0.30f, 0.30f, 0.10f, 0.10f, -0.10f, -0.10f, -0.30f, -0.30f };
                    int idx = tower.nextBarrel % 8;
                    muzzleFwd = 0.6f;
                    muzzleRight = tubeRight[idx];
                    muzzleUp = tubeUp[idx];
                    isRocket = true;
                    tower.nextBarrel = (tower.nextBarrel + 1) % 8;
                }
                else {
                    muzzleFwd = 1.0f;
                }

                // Calculate spawn position of projectile based on tower position, rotation, and muzzle offsets
                float spawnX = tower.x + fwdX * muzzleFwd + rightX * muzzleRight + upX * muzzleUp;
                float spawnY = gunPivotY + fwdY * muzzleFwd + upY * muzzleUp;
                float spawnZ = tower.z + fwdZ * muzzleFwd + rightZ * muzzleRight + upZ * muzzleUp;

                // Spawn projectile with initial velocity towards target, using tower damage and type for projectile properties
                spawnProjectile(spawnX, spawnY, spawnZ,
                    target->x, target->altitude, target->z,
                    tower.tower_dmg, isRocket);
                tower.current_cooldown = tower.tower_fire_rate;
            }
        }
    }

    // Update projectiles: move based on velocity, check for collisions with troops, and remove if hit or expired
    for (auto it = active_bullets.begin(); it != active_bullets.end(); ) {
        // Move projectile based on velocity and time step
        it->x += it->vx * delta_step;
        it->y += it->vy * delta_step;
        it->z += it->vz * delta_step;
        it->life_span -= delta_step;

        // Check for collisions with troops by calculating distance to each troop and comparing to hitbox radius based on troop type. If hit, reduce troop health by projectile damage and mark projectile for removal.
        bool destroyed = false;
        for (auto& enemy : troops) {
            float dist = sqrtf(powf(enemy.x - it->x, 2)
                + powf(enemy.altitude - it->y, 2)
                + powf(enemy.z - it->z, 2));
            // Adjust hitbox radius
            float hitbox_radius = (enemy.variant == CAR) ? 0.8f
                : (enemy.variant == TANK) ? 1.6f : 2.0f;
            if (dist < hitbox_radius) {
                enemy.health -= it->damage; // Damage
                destroyed = true;
                break;
            }
        }
        // Remove projectile if it hit a target or its lifespan expired
        if (destroyed || it->life_span <= 0) it = active_bullets.erase(it);
        else ++it;
    }

    // Process defeated troops: if health <= 0, remove from list and add gold reward. If lives <= 0, set game over state
    for (auto it = troops.begin(); it != troops.end(); ) {
        if (it->health <= 0) { gold += 15; it = troops.erase(it); }
        else ++it;
    }
    if (lives <= 0) { lives = 0; gameOver = true; }
}

// Helper function to check if a world coordinate is on the road mesh, by calculating the distance from the point to each segment of the road defined by the waypoints. If the distance is less than half the road width plus a small buffer, we consider it on the road for placement purposes.
static bool isOnRoad(float wx, float wz, const std::vector<P>& waypoints, float halfWidth) {
    for (int i = 0; i + 1 < (int)waypoints.size(); i++) {
        // Get the endpoints of the current road segment
        float ax = waypoints[i].x, az = waypoints[i].z;
        float bx = waypoints[i + 1].x, bz = waypoints[i + 1].z;
        // Calculate the projection of the point onto the line segment and find the closest point on the segment to the world coordinate
        float abx = bx - ax, abz = bz - az;
        float ab2 = abx * abx + abz * abz;
        float t = (ab2 > 0.f) ? ((wx - ax) * abx + (wz - az) * abz) / ab2 : 0.f; // Avoid division by zero for degenerate segments
        t = fmaxf(0.f, fminf(1.f, t)); // Clamp t to the range [0, 1] to stay within the segment
        float cx = ax + t * abx, cz = az + t * abz; // Closest point on the segment to (wx, wz)
        float d = sqrtf(powf(wx - cx, 2) + powf(wz - cz, 2)); // Distance from the world coordinate to the closest point on the road segment
        if (d < halfWidth + ROAD_PLACEMENT_BUFFER) return true; // If the distance is less than the half width of the road plus a small buffer, we consider it on the road for placement purposes
    }
    return false;
}

// Helper function to check if a world coordinate is within the exclusion radius of any existing tower, by calculating the distance from the point to each tower and comparing it to the defined exclusion radius. This prevents placing towers too close to each other for better gameplay balance.
static bool isOnTower(float wx, float wz, const std::vector<TowerInstance>& towers) {
    // Go through each existing tower and calculate the distance from the world coordinate to the tower's position. If the distance is less than the defined exclusion radius, we consider it on a tower for placement purposes.
    for (const auto& t : towers) {
        float dx = t.x - wx, dz = t.z - wz;
        if ((dx * dx + dz * dz) < TOWER_EXCLUSION_RADIUS_SQ) return true;
    }
    return false;
}

// Cast a ray from the camera through the mouse position and find where it intersects the ground plane (y=0). This is used to convert 2D screen coordinates to 3D world coordinates for tower placement. 
// It uses the current modelview and projection matrices to unproject the mouse position at both the near and far planes, then calculates the intersection with the ground plane.
bool Game::raycastGroundPlane(float mx, float my, int winW, int winH, float& outX, float& outZ) {
    GLdouble mvRaw[16], prRaw[16];
    GLint vpRaw[4];

    // Get the current modelview matrix, projection matrix, and viewport parameters from OpenGL. These are needed to convert screen coordinates to world coordinates using unprojection.
    glGetDoublev(GL_MODELVIEW_MATRIX, mvRaw);
    glGetDoublev(GL_PROJECTION_MATRIX, prRaw);
    glGetIntegerv(GL_VIEWPORT, vpRaw);

    glm::mat4 MV = glm::make_mat4(mvRaw);
    glm::mat4 P = glm::make_mat4(prRaw);
    glm::vec4 viewport(vpRaw[0], vpRaw[1], vpRaw[2], vpRaw[3]);

    // Define the screen coordinates
    float screenY = viewport[3] - my;

    // Near plane point (z = 0.0)
    glm::vec3 winNear(mx, screenY, 0.0f);
    glm::vec3 posNear = glm::unProject(winNear, MV, P, viewport);

    // Far plane point (z = 1.0)
    glm::vec3 winFar(mx, screenY, 1.0f);
    glm::vec3 posFar = glm::unProject(winFar, MV, P, viewport);

    // Calculate the ray direction and find where it intersects the ground (Y = 0)
    float rayOy = posNear.y;
    float rayDy = posFar.y - posNear.y;

    if (fabsf(rayDy) < 0.0001f) return false; // Ray is perfectly horizontal

    float t = -rayOy / rayDy; // Calculate the parameter t for the ray equation to find the intersection with the ground plane. If t is negative, it means the ray is pointing away from the ground, so we return false.
    if (t < 0) return false; // Looking away from the ground

    // Calculate the world coordinates of the intersection point using the parameter t and the ray equation. This gives us the X and Z coordinates on the ground plane where the mouse is pointing.
    outX = posNear.x + t * (posFar.x - posNear.x);
    outZ = posNear.z + t * (posFar.z - posNear.z);

    return true;
}

// Main rendering function for the game scene, called every frame with the current projection and view matrices. It sets up shader uniforms for lighting and bullet effects, renders the ground and road, and then renders shadows and models for towers, troops, and projectiles. It also handles blending and stencil operations for shadows.
void Game::render(glm::mat4 P, glm::mat4 V) {
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(P));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(V));

    glUseProgram(shader_id);
    glUniformMatrix4fv(glGetUniformLocation(shader_id, "P"), 1, GL_FALSE, glm::value_ptr(P));
    glUniformMatrix4fv(glGetUniformLocation(shader_id, "V"), 1, GL_FALSE, glm::value_ptr(V));

    // Global directional light for the sun, transformed into view space. This is a simple directional light coming from a fixed direction in world space, which we transform by the view matrix to get the light direction in eye space for the shader.
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.5f));
    glm::vec3 eyeSun = glm::mat3(V) * sunDir;
    glUniform3f(glGetUniformLocation(shader_id, "lightDirGlobal"), eyeSun.x, eyeSun.y, eyeSun.z);

    // Ambient point light for some extra lighting on the scene, positioned in world space and transformed to view space. This is a simple point light that adds some ambient lighting to the scene, which can help make the models look better without needing a full global illumination solution. We transform the position of the light into view space for the shader.
    glm::vec4 ambPosW = glm::vec4(-45.0f, 12.0f, 10.0f, 1.0f);
    glm::vec3 ambPosE = glm::vec3(V * ambPosW);
    glUniform3f(glGetUniformLocation(shader_id, "ambientPointPos"), ambPosE.x, ambPosE.y, ambPosE.z);
    glUniform3f(glGetUniformLocation(shader_id, "ambientPointColor"), 0.4f, 0.5f, 1.0f);

    // Bullet lighting effect: if there are active bullets, set shader uniforms to enable a bright point light at the position of the first bullet. This creates a simple lighting effect for projectiles, making them appear brighter and more visible as they move through the scene. We transform the bullet position into view space for the shader.
    if (!active_bullets.empty()) {
        glUniform1f(glGetUniformLocation(shader_id, "bulletActive"), 1.0f);
        glm::vec3 viewBulletPos = glm::vec3(V * glm::vec4(active_bullets[0].x, active_bullets[0].y, active_bullets[0].z, 1.0f));
        glUniform3f(glGetUniformLocation(shader_id, "bulletPos"), viewBulletPos.x, viewBulletPos.y, viewBulletPos.z);
        glUniform3f(glGetUniformLocation(shader_id, "bulletColor"), 1.0f, 0.8f, 0.2f);
    }
    else {
        glUniform1f(glGetUniformLocation(shader_id, "bulletActive"), 0.0f);
    }

    // Shadow rendering: we use a simple planar shadow technique by applying a shadow projection matrix to the models when rendering the shadows. This creates a flat shadow on the ground that roughly corresponds to the shape of the model, without needing complex shadow mapping. We also set shader uniforms to render the shadows with a dark color and some transparency.
    const float sdx = 0.5f, sdy = 1.0f, sdz = 0.5f;
    float shadowMat[16] = {
        sdy, 0.0f, 0.0f, 0.0f,
        -sdx, 0.0f, -sdz, 0.0f,
        0.0f, 0.0f, sdy, 0.0f,
        0.0f, 0.0f, 0.0f, sdy
    };
    glm::mat4 identityModel(1.0f);

    // For the ground plane, we want to use a fixed TBN (tangent, bitangent, normal) basis that is aligned with the world axes, since the ground is flat and we want the normal mapping to be consistent regardless of the view direction. We calculate the tangent and bitangent vectors in view space and pass them as uniforms to the shader, along with a flag to indicate that we want to use the fixed TBN for the ground rendering.
    glm::vec3 eyeT = glm::normalize(glm::vec3(V * glm::vec4(1, 0, 0, 0)));
    glm::vec3 eyeB = glm::normalize(glm::vec3(V * glm::vec4(0, 0, 1, 0)));
    glUniform3f(glGetUniformLocation(shader_id, "fixedTangent"), eyeT.x, eyeT.y, eyeT.z);
    glUniform3f(glGetUniformLocation(shader_id, "fixedBitangent"), eyeB.x, eyeB.y, eyeB.z);
    glUniform1f(glGetUniformLocation(shader_id, "useFixedTBN"), 1.0f);

    // Render the ground plane and road with their respective textures, using the fixed TBN for correct normal mapping. We set a uniform to scale the texture coordinates for the road to make it tile properly, and then reset it after rendering.
    setModel(shader_id, V, identityModel);
    bindTextures(shader_id, groundTex);
    drawMesh(groundBuf);
    unbindTextures(shader_id);

    glUniform1f(glGetUniformLocation(shader_id, "useFixedTBN"), 0.0f);

    // Render the road with a different texture and a tiling factor for the texture coordinates, to make the road texture repeat along its length. We use the same fixed TBN for the road since it's also flat, and then reset it after rendering.
    setModel(shader_id, V, identityModel);
    glUniform1f(glGetUniformLocation(shader_id, "texBlendScale"), 4.3f);
    bindTextures(shader_id, pathTex);
    drawMesh(roadBuf);
    unbindTextures(shader_id);
    glUniform1f(glGetUniformLocation(shader_id, "texBlendScale"), 0.0f);

    // Render shadows for towers and troops using the shadow projection matrix, with blending and stencil operations to create a simple planar shadow effect. 
    // We disable depth testing and enable blending to render the shadows as transparent dark shapes on the ground, and use the stencil buffer to avoid rendering shadows on top of each other for better visual clarity.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Set up stencil buffer to only render shadows where the stencil value is 0, and increment the stencil value for each shadow rendered. This way, if multiple shadows overlap, they will only be drawn once and won't become darker with each overlap.
    glEnable(GL_STENCIL_TEST);
    glClear(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_EQUAL, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    // Set shader uniforms for shadow rendering: we use a flat normal matrix since the shadows are rendered as flat shapes on the ground, and set a dark green color with some transparency for the shadows. We also set a uniform to indicate that we are rendering in shadow mode, which can be used in the shader to adjust the lighting calculations accordingly.
    unbindTextures(shader_id);
    glm::mat3 flatNorm(1.0f);
    glUniformMatrix3fv(glGetUniformLocation(shader_id, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(flatNorm));
    glUniform3f(glGetUniformLocation(shader_id, "shadowColor"), 0.05f, 0.12f, 0.05f);
    glUniform1f(glGetUniformLocation(shader_id, "shadowAlpha"), 0.45f);
    glUniform1f(glGetUniformLocation(shader_id, "renderMode"), 1.0f); // Shadow rendering mode

    // Draw shadows for towers and troops by applying the shadow projection matrix to their models. We loop through each active tower and troop, calculate their model matrices, apply the shadow projection, and render them with the shadow shader settings. This creates a simple planar shadow effect on the ground that roughly corresponds to the shape of the models.
    for (const auto& tower : active_defenses) {
        const auto& assets = tower_assets[(int)tower.tower_variant];

        glm::mat4 mBase = glm::scale(glm::translate(identityModel, { tower.x, assets.base_y_offset, tower.z }), { 0.5f, 0.5f, 0.5f });
        glUniformMatrix4fv(glGetUniformLocation(shader_id, "M"), 1, GL_FALSE, glm::value_ptr(glm::make_mat4(shadowMat) * mBase));
        drawMesh(assets.base_buf);

        glm::mat4 mRotate = glm::rotate(glm::translate(mBase, { 0.0f, assets.rotate_y_offset, 0.0f }), glm::radians(tower.current_yaw), { 0.0f, 1.0f, 0.0f });
        glUniformMatrix4fv(glGetUniformLocation(shader_id, "M"), 1, GL_FALSE, glm::value_ptr(glm::make_mat4(shadowMat) * mRotate));
        drawMesh(assets.rotate_buf);

        glm::mat4 mGun = glm::rotate(glm::translate(mRotate, { 0.0f, assets.gun_y_offset, 0.0f }), glm::radians(-tower.current_pitch), { 1.0f, 0.0f, 0.0f });
        glUniformMatrix4fv(glGetUniformLocation(shader_id, "M"), 1, GL_FALSE, glm::value_ptr(glm::make_mat4(shadowMat) * mGun));
        drawMesh(assets.gun_buf);
    }

    for (const auto& troop : troops) {
        glm::mat4 M = glm::scale(glm::rotate(glm::translate(identityModel, { troop.x, troop.altitude, troop.z }), glm::radians(troop.rotation_yaw), { 0.0f, 1.0f, 0.0f }), { 0.3f, 0.3f, 0.3f });
        glUniformMatrix4fv(glGetUniformLocation(shader_id, "M"), 1, GL_FALSE, glm::value_ptr(glm::make_mat4(shadowMat) * M));
        drawMesh(troop_assets[(int)troop.variant].base_buf);
    }
    // Clean up shadow rendering state: disable stencil test, re-enable depth testing and writing, and disable blending. Reset the render mode uniform to normal rendering for the rest of the objects.
    glUniform1f(glGetUniformLocation(shader_id, "renderMode"), 0.0f);
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    float time = (float)glfwGetTime();

    // Render towers, troops, and projectiles with their respective models and textures. We loop through each active tower and troop, calculate their model matrices based on their position and rotation, bind the appropriate textures, and draw their meshes. For troops, we also render additional parts like tank wheels or helicopter rotors with their own transformations to create simple animations.
    for (const auto& troop : troops) {
        const auto& assets = troop_assets[(int)troop.variant];

        // Base transformation for the troop model, based on its position, altitude, and rotation. We scale it down to fit the scene and apply the rotation around the Y axis to make it face the direction of movement.
        glm::mat4 M = glm::scale(glm::rotate(glm::translate(identityModel, { troop.x, troop.altitude, troop.z }), glm::radians(troop.rotation_yaw), { 0.0f, 1.0f, 0.0f }), { 0.35f, 0.35f, 0.35f });
        setModel(shader_id, V, M);
        bindTextures(shader_id, assets.base_tex);
        drawMesh(assets.base_buf);
        unbindTextures(shader_id);

        // Additional rendering for specific troop types: for tanks, we render rotating wheels; for helicopters, we render spinning rotors. We calculate the appropriate transformations for these parts to create simple animations based on the current time and the troop's speed.
        if (troop.variant == TroopType::TANK) {
            float wheelRollAngle = time * troop.speed * 120.0f;
            float wheelOffsetsZ[2] = { -3.5f, 4.65f };
            for (int w = 0; w < 2; w++) {
                glm::mat4 mWheel = glm::rotate(glm::rotate(glm::scale(glm::translate(M, { 0.0f, -1.9f, wheelOffsetsZ[w] }), { 0.95f, 0.95f, 0.95f }), glm::radians(wheelRollAngle), { 1.0f, 0.0f, 0.0f }), glm::radians(90.0f), { 1.0f, 0.0f, 0.0f });
                setModel(shader_id, V, mWheel);
                bindTextures(shader_id, assets.base_tex);
                drawMesh(assets.wheel_buf);
                unbindTextures(shader_id);
            }
        }

        if (troop.variant == TroopType::HELICOPTER) {
            float propSpinAngle = time * 360.0f;
            glm::mat4 mProp = glm::rotate(glm::translate(M, { 0.0f, troop.altitude - 0.5f, 0.5f }), glm::radians(propSpinAngle), { 0.0f, 1.0f, 0.0f });
            setModel(shader_id, V, mProp);
            bindTextures(shader_id, assets.base_tex);
            drawMesh(assets.prop_buf);
            unbindTextures(shader_id);
        }
    }

    // Render towers with separate meshes for the base, rotating part, and gun, to allow for independent rotation of the turret and gun. We calculate the model matrices for each part based on the tower's position and rotation, bind the appropriate textures, and draw the meshes. This allows the towers to visually rotate their turrets and aim their guns towards targets.
    for (const auto& tower : active_defenses) {
        const auto& assets = tower_assets[(int)tower.tower_variant];

        // Base
        glm::mat4 mBase = glm::scale(glm::translate(identityModel, { tower.x, assets.base_y_offset, tower.z }), { 0.5f, 0.5f, 0.5f });
        setModel(shader_id, V, mBase);
        bindTextures(shader_id, assets.base_tex);
        drawMesh(assets.base_buf);

        // Rotating turret part, rotated by the tower's current yaw angle. We apply the rotation around the Y axis to make the turret face the correct direction based on the tower's aiming logic.
        glm::mat4 mRotate = glm::rotate(glm::translate(mBase, { 0.0f, assets.rotate_y_offset, 0.0f }), glm::radians(tower.current_yaw), { 0.0f, 1.0f, 0.0f });
        setModel(shader_id, V, mRotate);
        bindTextures(shader_id, assets.rotate_tex);
        drawMesh(assets.rotate_buf);

        // Gun part, rotated by the tower's current pitch angle. We apply the rotation around the X axis to make the gun aim up or down based on the tower's aiming logic.
        glm::mat4 mGun = glm::rotate(glm::translate(mRotate, { 0.0f, assets.gun_y_offset, 0.0f }), glm::radians(-tower.current_pitch), { 1.0f, 0.0f, 0.0f });
        setModel(shader_id, V, mGun);
        bindTextures(shader_id, assets.gun_tex);
        drawMesh(assets.gun_buf);
        unbindTextures(shader_id);
    }

    // Render projectiles with their respective models and textures, oriented based on their velocity vector. We loop through each active projectile, calculate its speed and direction, and create a model matrix that positions it at the projectile's location and rotates it to face the direction of movement. This makes the projectiles visually align with their trajectory as they move through the scene.
    for (const auto& b : active_bullets) {
        float spd = sqrtf(b.vx * b.vx + b.vy * b.vy + b.vz * b.vz);
        // Calculate yaw and pitch angles from the velocity vector to orient the projectile model. The yaw is calculated from the horizontal components of the velocity, while the pitch is calculated from the vertical component relative to the speed. We convert these angles to degrees for use in the rotation transformations.
        float yaw = atan2f(b.vx, b.vz) * 180.0f / 3.14159f;
        float pitch = -asinf(b.vy / spd) * 180.0f / 3.14159f;

        glm::mat4 M = glm::rotate(glm::rotate(glm::rotate(glm::translate(identityModel, { b.x, b.y, b.z }), glm::radians(yaw), { 0.0f, 1.0f, 0.0f }), glm::radians(pitch), { 1.0f, 0.0f, 0.0f }), glm::radians(-90.0f), { 0.0f, 1.0f, 0.0f });

        // Choose the model and texture based on whether the projectile is a rocket or a bullet, and scale it appropriately. Rockets are larger and use a different model and texture than bullets. We set the model matrix for the projectile, bind the appropriate textures, and draw the mesh.
        if (b.isRocket) {
            glm::mat4 Ms = glm::scale(M, { 0.15f, 0.15f, 0.15f });
            setModel(shader_id, V, Ms);
            bindTextures(shader_id, projectile_assets.rocket_tex);
            drawMesh(projectile_assets.rocket_buf);
        }
        else {
            glm::mat4 Ms = glm::scale(M, { 0.1f, 0.1f, 0.1f });
            setModel(shader_id, V, Ms);
            bindTextures(shader_id, projectile_assets.bullet_tex);
            drawMesh(projectile_assets.bullet_buf);
        }
        unbindTextures(shader_id);
    }

    // Build mode: we render a "ghost" preview of the tower at the mouse position on the ground
    if (isBuilding && !gameOver) {
        double mx2, my2;
        int ww, wh;
        // Get the current mouse position and window size to perform raycasting for tower placement. We need the mouse coordinates and the window dimensions to convert the 2D screen coordinates to 3D world coordinates using the raycastGroundPlane function.
        glfwGetCursorPos(glfwGetCurrentContext(), &mx2, &my2);
        glfwGetWindowSize(glfwGetCurrentContext(), &ww, &wh);
        float gx, gz;

        // Raycast from the mouse position to the ground plane to find the world coordinates for the ghost tower. If the raycast is successful, we get the X and Z coordinates on the ground where the mouse is pointing, which we can use to position the ghost tower preview.
        if (raycastGroundPlane((float)mx2, (float)my2, ww, wh, gx, gz)) {
            ghost_wx = gx; ghost_wz = gz;
        }

        bool collisionFound = isOnRoad(ghost_wx, ghost_wz, pathWaypoints, game_pathway.roadWidth)
            || isOnTower(ghost_wx, ghost_wz, active_defenses);

        const auto& assets = tower_assets[(int)ghostType];
        float range = getTowerDefaults(ghostType).tower_range;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Draw a circle on the ground to indicate the tower's range, using a simple line loop and a semi-transparent white color. This gives the player a visual indication of how far the tower will be able to shoot when placed at the current location.
        glUseProgram(hud_shader);
        glm::mat4 circleM = glm::scale(glm::translate(identityModel, { ghost_wx, 0.05f, ghost_wz }), { range, 1.0f, range });
        glUniformMatrix4fv(glGetUniformLocation(hud_shader, "MVP"), 1, GL_FALSE, glm::value_ptr(P * V * circleM));
        glUniform4f(glGetUniformLocation(hud_shader, "uColor"), 1.0f, 1.0f, 1.0f, 0.6f);
        glBindVertexArray(circleBuf.vao);
        glDrawArrays(GL_LINE_LOOP, 0, circleBuf.count);
        glBindVertexArray(0);

        glUseProgram(shader_id);

        // Draw translucent "ghost" tower preview at the mouse position
        GLint renderModeLoc = glGetUniformLocation(shader_id, "renderMode");
        glUniform1f(renderModeLoc, 2.0f);

        // Red if colliding with road or another tower, turqoise otherwise
        glUniform3f(glGetUniformLocation(shader_id, "ghostColor"),
            collisionFound ? 1.0f : 0.0f, 0.0f, collisionFound ? 0.0f : 1.0f);
        glUniform1f(glGetUniformLocation(shader_id, "isGhost"), 1.0f);

        // Render the tower meshes at the ghost position, using the same transformations as a normal tower but with the ghost shader settings. This allows the player to see a preview of how the tower will look and where it will be placed before actually building it.
        glm::mat4 mGhost = glm::scale(glm::translate(identityModel, { ghost_wx, assets.base_y_offset, ghost_wz }), { 0.5f, 0.5f, 0.5f });
        setModel(shader_id, V, mGhost);
        drawMesh(assets.base_buf);

        glm::mat4 mR2 = glm::translate(mGhost, { 0.0f, assets.rotate_y_offset, 0.0f });
        setModel(shader_id, V, mR2);
        drawMesh(assets.rotate_buf);

        glm::mat4 mG2 = glm::translate(mR2, { 0.0f, assets.gun_y_offset, 0.0f });
        setModel(shader_id, V, mG2);
        drawMesh(assets.gun_buf);

        glUniform1f(renderModeLoc, 0.0f);
        glDisable(GL_BLEND);
    }

    glUseProgram(0);
    renderHUD();
}

// Function to spawn a projectile with given start and target positions, damage, and type (bullet or rocket). It calculates the velocity vector based on the direction from the start to the target and normalizes it to a fixed speed. The projectile is then added to the list of active bullets to be updated and rendered in the game loop.
void Game::spawnProjectile(float sx, float sy, float sz, float tx, float ty, float tz, float dmg, bool isRocket) {
    Projectile p;
    p.x = sx; p.y = sy; p.z = sz;
    p.damage = dmg; p.isRocket = isRocket;
    // Calculate the velocity vector from the start position to the target position, normalize it, and scale it by a fixed speed. This gives the projectile a consistent speed regardless of the distance to the target, while still ensuring it moves in the correct direction.
    float dx = tx - sx, dy = ty - sy, dz = tz - sz;
    float d = sqrtf(dx * dx + dy * dy + dz * dz);
    float spd = 50.0f; // Projectile speed can be adjusted as needed for gameplay balance

    // Avoid division by zero in case the start and target positions are the same, which would result in a zero-length direction vector. In that case, we can simply set the velocity to zero or choose a default direction.
    p.vx = (dx / d) * spd; p.vy = (dy / d) * spd; p.vz = (dz / d) * spd;
    p.life_span = 2.0f; // Life span of the projectile in seconds, after which it will be removed from the game
    active_bullets.push_back(p);
}

// Spawns a new troop at the start of the path with a random variant (car, tank, or helicopter) and initializes its properties such as health, speed, altitude, and rotation. The new troop is added to the list of active troops to be updated and rendered in the game loop.
void Game::spawnTroop() {
    if (pathWaypoints.empty()) return;

    Troop t;
    t.x = pathWaypoints[0].x; t.z = pathWaypoints[0].z;
    t.currentWaypoint = 1; t.rotation_yaw = 0.0f;

    int r = rand() % 100;
    if (r < 50) t.variant = CAR;
    else if (r < 80) t.variant = TANK;
    else t.variant = HELICOPTER;

    if (t.variant == CAR) { t.health = 40; t.speed = 3.5f; t.altitude = 0.8f; }
    else if (t.variant == TANK) { t.health = 150; t.speed = 2.0f; t.altitude = 1.1f; }
    else { t.health = 60; t.speed = 12.0f; t.altitude = 6.0f; }

    troops.push_back(t);
}

// Renders the heads-up display (HUD) with game information such as gold, lives, wave number, and build mode status. 
// Uses fixed-function pipeline to draw text on the screen, and sets up an orthographic projection for 2D rendering. It also displays messages for wave completion and game over conditions.
void Game::renderHUD() {
    float W = (float)hud_win_w, H = (float)hud_win_h;
    glm::mat4 orthoP = glm::ortho(0.0f, W, H, 0.0f, -1.0f, 1.0f);
    glm::mat4 orthoV(1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(orthoP));

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(orthoV));

    glDisable(GL_DEPTH_TEST);

    // Helper function to draw text on the screen at a given position with a specified color. It uses GLUT bitmap characters to render the text, and sets the raster position for where the text should start. This allows us to easily display game information and messages on the HUD.
    auto drawText = [](float x, float y, const std::string& s, float r, float g, float b) {
        glColor3f(r, g, b);
        glRasterPos2f(x, y);
        for (char c : s) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, c);
        };

    std::string hud = "Gold: " + std::to_string(gold) + "  Lives: " + std::to_string(lives) + ((wave > 0) ? "  Wave: " + std::to_string(wave) : "  [SPACE] to start");
    drawText(20, 30, hud, 1.0f, 1.0f, 0.0f);

    std::string spd = paused ? "  [PAUSED]" : "  Speed: " + std::to_string((int)gameSpeed) + "x";
    drawText(20, 55, spd, 1.0f, 1.0f, 0.0f);

    std::string buildStatus = "1=MG  2=Rockets  3=Sniper";
    if (isBuilding) {
        if (ghostType == TowerType::MACHINE_GUN) buildStatus = "Building: MachineGun";
        else if (ghostType == TowerType::ROCKETS) buildStatus = "Building: Rockets";
        else buildStatus = "Building: Sniper";
    }
    drawText(20, 80, buildStatus, 0.7f, 1.0f, 0.7f);

    if (waveEndMessageTimer > 0.0f) {
        std::string msg = "Wave " + std::to_string(wave) + " complete!";
        glColor3f(0.0f, 1.0f, 0.0f);
        glRasterPos2f((W - msg.size() * 9) / 2.0f, H / 2.0f);
        for (char c : msg) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    if (gameOver) {
        std::string msg = "GAME OVER  (R to restart)";
        glColor3f(1.0f, 0.0f, 0.0f);
        glRasterPos2f((W - msg.size() * 9) / 2.0f, H / 2.0f);
        for (char c : msg) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    glEnable(GL_DEPTH_TEST);
}

void Game::selectTowerType(int i) {
    if (i >= 0 && i <= 2) selectedType = (TowerType)i;

    isBuilding = true;
}

// Toggles build mode for a specific tower type based on the input index. If the player is already in build mode for that tower type, it will exit build mode. Otherwise, it will enter build mode for the selected tower type and set it as the currently selected type for placement.
void Game::toggleBuildMode(int typeIndex) {
    TowerType t = (TowerType)typeIndex;
    if (isBuilding && ghostType == t) isBuilding = false;
    else { isBuilding = true; ghostType = t; selectedType = t; }
}

void Game::tryPlaceTower(int mouse_x, int mouse_y, Camera& world_camera) {
    if (!isBuilding) return;

    TowerInstance turret = getTowerDefaults(selectedType);
    if (gold < turret.cost) return;
    if (isOnRoad(ghost_wx, ghost_wz, pathWaypoints, game_pathway.roadWidth) ||
        isOnTower(ghost_wx, ghost_wz, active_defenses)) return;

    turret.x = ghost_wx;
    turret.z = ghost_wz;
    turret.current_yaw = 0.0f;

    active_defenses.push_back(turret);
    gold -= turret.cost;

    std::cout << "Tower placed | type=" << (int)selectedType << " x=" << ghost_wx << " z=" << ghost_wz << " | gold=" << gold << "\n";
}

// Loads a 3D model from an OBJ file using the tinyobjloader library and populates a vertex buffer with the vertex data. It reads the vertex positions, normals, texture coordinates, and material information from the OBJ file, and constructs a list of VertexData structures that can be used for rendering the model in OpenGL. The function takes the root directory and filename of the OBJ file, as well as a reference to a vector that will be filled with the vertex data.
void Game::loadModels(std::string root, std::string file, std::vector<VertexData>& buffer) {
    tinyobj::attrib_t attr;
    std::vector<tinyobj::shape_t> shp;
    std::vector<tinyobj::material_t> mat;
    std::string w, e;

    if (!tinyobj::LoadObj(&attr, &shp, &mat, &w, &e, (root + file).c_str(), root.c_str())) return;

    for (const auto& s : shp) {
        size_t off = 0;
        for (size_t f = 0; f < s.mesh.num_face_vertices.size(); f++) {
            int fv = s.mesh.num_face_vertices[f];
            size_t baseIndex = buffer.size();

            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = s.mesh.indices[off + v];
                VertexData vd{};

                vd.x = attr.vertices[3 * idx.vertex_index + 0];
                vd.y = attr.vertices[3 * idx.vertex_index + 1];
                vd.z = attr.vertices[3 * idx.vertex_index + 2];

                if (idx.normal_index >= 0) {
                    vd.nx = attr.normals[3 * idx.normal_index + 0];
                    vd.ny = attr.normals[3 * idx.normal_index + 1];
                    vd.nz = attr.normals[3 * idx.normal_index + 2];
                }
                else {
                    vd.nx = 0.0f; vd.ny = 1.0f; vd.nz = 0.0f;
                }

                if (idx.texcoord_index >= 0) {
                    vd.u = attr.texcoords[2 * idx.texcoord_index + 0];
                    vd.v = attr.texcoords[2 * idx.texcoord_index + 1];
                }

                int mat_id = s.mesh.material_ids[f];
                if (mat_id >= 0 && mat_id < (int)mat.size()) {
                    vd.r = mat[mat_id].diffuse[0];
                    vd.g = mat[mat_id].diffuse[1];
                    vd.b = mat[mat_id].diffuse[2];
                }
                else {
                    vd.r = 0.0f; vd.g = 0.0f; vd.b = 0.0f;
                }

                buffer.push_back(vd);
            }
            off += fv;

            if (fv == 3) {
                VertexData& v0 = buffer[baseIndex];
                VertexData& v1 = buffer[baseIndex + 1];
                VertexData& v2 = buffer[baseIndex + 2];

                float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
                float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
                float du1 = v1.u - v0.u, dv1 = v1.v - v0.v;
                float du2 = v2.u - v0.u, dv2 = v2.v - v0.v;
                float det = du1 * dv2 - du2 * dv1;
                if (fabsf(det) > 0.00001f) {
                    float invDet = 1.0f / det;
                    float tx = invDet * (dv2 * e1x - dv1 * e2x);
                    float ty = invDet * (dv2 * e1y - dv1 * e2y);
                    float tz = invDet * (dv2 * e1z - dv1 * e2z);
                    float len = sqrtf(tx * tx + ty * ty + tz * tz);
                    if (len > 0.0001f) { tx /= len; ty /= len; tz /= len; }
                    v0.tx = v1.tx = v2.tx = tx;
                    v0.ty = v1.ty = v2.ty = ty;
                    v0.tz = v1.tz = v2.tz = tz;
                }
            }
        }
    }
}

// Loads vertex and fragment shader source code from files, compiles them, and links them into an OpenGL shader program. The function reads the shader source code from the specified file paths, creates shader objects, compiles them, checks for compilation errors, and then links them into a shader program. It also checks for linking errors and outputs any error messages to the console. Finally, it returns the ID of the created shader program.
GLuint Game::loadShader(const char* vertexPath, const char* fragmentPath) {
    auto readFile = [](const char* path) -> std::string {
        std::ifstream f(path);
        if (!f) { std::cout << "Cannot open shader: " << path << "\n"; return ""; }
        std::stringstream ss; ss << f.rdbuf(); return ss.str();
        };
    std::string vs = readFile(vertexPath), fs = readFile(fragmentPath);
    const char* vsc = vs.c_str(); const char* fsc = fs.c_str();

    // Compile vertex shader and check for errors
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vsc, NULL); glCompileShader(vertex);
    GLint ok; glGetShaderiv(vertex, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buffer[512]; glGetShaderInfoLog(vertex, 512, NULL, buffer);
        std::cout << "VERT ERROR:\n" << buffer << "\n";
    }

    // Compile fragment shader and check for errors
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fsc, NULL); glCompileShader(fragment);
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buffer[512]; glGetShaderInfoLog(fragment, 512, NULL, buffer);
        std::cout << "FRAG ERROR:\n" << buffer << "\n";
    }

    // Link shaders into a program and check for errors
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertex); glAttachShader(prog, fragment); glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buffer[1024]; glGetProgramInfoLog(prog, 1024, NULL, buffer);
        std::cout << "LINK ERROR:\n" << buffer << "\n";
    }

    // Clean up shader objects after linking, as they are no longer needed once linked into a program
    glDeleteShader(vertex); glDeleteShader(fragment);
    return prog;
}