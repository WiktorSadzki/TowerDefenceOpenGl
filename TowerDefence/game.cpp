#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include "Game.h"
#include "lodepng.h"
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static float spawn_interval = 0.1f;

// Reads textures using lodepng
GLuint Game::readTexture(const char* filename) {
    GLuint tex = 0;
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* image_data = stbi_load(filename, &width, &height, &channels, 4);

    if (!image_data) {
        std::cout << "Texture load error! Could not find/read file: " << filename << std::endl;
        return 0;
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image_data);

    return tex;
}

// Binds all PBR maps and flags to the shader program locations
static void bindTextures(GLuint shader, const TextureBundle& bundle_config) {
    if (bundle_config.baseColor == 0) {
        glUniform1f(glGetUniformLocation(shader, "hasBaseColor"), 0.0f);
        return;
    }

    glUniform1i(glGetUniformLocation(shader, "texBaseColor"), 0);
    glUniform1i(glGetUniformLocation(shader, "texEmissive"), 1);
    glUniform1i(glGetUniformLocation(shader, "texNormal"), 2);
    glUniform1i(glGetUniformLocation(shader, "texMetallic"), 3);
    glUniform1i(glGetUniformLocation(shader, "texRoughness"), 4);
    glUniform1i(glGetUniformLocation(shader, "texAO"), 5);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bundle_config.baseColor);
    glUniform1f(glGetUniformLocation(shader, "hasBaseColor"),
        bundle_config.baseColor ? 1.0f : 0.0f);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bundle_config.emissive);
    glUniform1f(glGetUniformLocation(shader, "hasEmissive"),
        bundle_config.emissive ? 1.0f : 0.0f);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, bundle_config.normalMap);
    glUniform1f(glGetUniformLocation(shader, "hasNormal"),
        bundle_config.normalMap ? 1.0f : 0.0f);
    
    glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, bundle_config.metallic);
    glUniform1f(glGetUniformLocation(shader, "hasMetallic"),
        bundle_config.metallic ? 1.0f : 0.0f);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, bundle_config.roughness);
    glUniform1f(glGetUniformLocation(shader, "hasRoughness"),
        bundle_config.roughness ? 1.0f : 0.0f);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, bundle_config.ao);
    glUniform1f(glGetUniformLocation(shader, "hasAO"),
        bundle_config.ao ? 1.0f : 0.0f);

    glActiveTexture(GL_TEXTURE0);
}

// Resets texture targets and turns hasTexture uniform flags off
static void unbindTextures(GLuint shader) {
    for (int i = 0; i < 6; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glUniform1f(glGetUniformLocation(shader, "hasBaseColor"), 0.0f);
    glUniform1f(glGetUniformLocation(shader, "hasNormal"), 0.0f);
    glUniform1f(glGetUniformLocation(shader, "hasMetallic"), 0.0f);
    glUniform1f(glGetUniformLocation(shader, "hasRoughness"), 0.0f);
    glUniform1f(glGetUniformLocation(shader, "hasAO"), 0.0f);
    glUniform1f(glGetUniformLocation(shader, "hasEmissive"), 0.0f);
    glActiveTexture(GL_TEXTURE0);
}


static void renderMesh(const std::vector<VertexData>& mesh, bool useColor = true) {
    glBegin(GL_TRIANGLES);
    for (const auto& v : mesh) {
        glTexCoord2f(v.u, v.v);
        if (useColor) glColor4f(v.r, v.g, v.b, 1.0f);
        glNormal3f(v.nx, v.ny, v.nz);
        glVertex3f(v.x, v.y, v.z);
    }
    glEnd();
}

TowerInstance Game::getTowerDefaults(TowerType type) {
    TowerInstance t;
    t.tower_variant = type;
    t.cost = 20;
    t.tower_rotate_speed = 15.0f;
    t.tower_fire_rate = 0.15f;
    t.tower_dmg = 20.0f;
    t.tower_range = 10.0f;

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

    for (const auto& splinePt : game_pathway.splinePoints) {
        P sharpPt;
        sharpPt.x = splinePt.x;
        sharpPt.z = splinePt.z;
        pathWaypoints.push_back(sharpPt);
    }

    std::string tower_dir = "Assets/Towers/source/";
    loadModels(tower_dir, "base.obj", tower_assets[0].base_mesh);
    loadModels(tower_dir, "machine_rotate.obj", tower_assets[0].rotate_mesh);
    loadModels(tower_dir, "machine_gun.obj", tower_assets[0].gun_mesh);
    tower_assets[0].base_y_offset = 0.4f;
    tower_assets[0].gun_y_offset = 1.8f;
    tower_assets[0].rotate_y_offset = 0.8f;

    loadModels(tower_dir, "base.obj", tower_assets[1].base_mesh);
    loadModels(tower_dir, "rockets_rotate.obj", tower_assets[1].rotate_mesh);
    loadModels(tower_dir, "rockets_gun.obj", tower_assets[1].gun_mesh);
    tower_assets[1].base_y_offset = 0.4f;
    tower_assets[1].gun_y_offset = 0.6f;
    tower_assets[1].rotate_y_offset = 2.8f;

    loadModels(tower_dir, "base.obj", tower_assets[2].base_mesh);
    loadModels(tower_dir, "sniper_rotate.obj", tower_assets[2].rotate_mesh);
    loadModels(tower_dir, "sniper_gun.obj", tower_assets[2].gun_mesh);
    tower_assets[2].base_y_offset = 0.4f;
    tower_assets[2].gun_y_offset = 1.8f;
    tower_assets[2].rotate_y_offset = 0.8f;

    std::string troop_dir = "Assets/Troops/source/";
    loadModels(troop_dir, "car.obj", troop_assets[0].base_mesh);
    loadModels(troop_dir, "tank.obj", troop_assets[1].base_mesh);
    loadModels(troop_dir, "helicopter.obj", troop_assets[2].base_mesh);
    loadModels(troop_dir, "Wheel.obj", troop_assets[1].wheel_mesh);
    loadModels(troop_dir, "Propellers.obj", troop_assets[2].prop_mesh);

    std::string ammo_dir = "Assets/Ammo/source/";
    loadModels(ammo_dir, "Bullet.obj", projectile_assets.bullet_mesh);
    loadModels(ammo_dir, "Rocket.obj", projectile_assets.rocket_mesh);

    shader_id = loadShader("v_simplest.glsl", "f_simplest.glsl");

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
    bulletTex.baseColor = readTexture("Assets/Ammo/textures/Bullet_Base_Color.png");
    bulletTex.normalMap = readTexture("Assets/Ammo/textures/Bullet_Normal.png");
    bulletTex.emissive = readTexture("Assets/Ammo/textures/Bullet_Emissive.png");

    projectile_assets.bullet_tex = bulletTex;
    projectile_assets.rocket_tex = bulletTex;
    
    groundTex.baseColor = readTexture("Assets/Ground/textures/terrain_diff.jpg");
    groundTex.normalMap = readTexture("Assets/Ground/textures/terrain_normal.png");
    groundTex.roughness = readTexture("Assets/Ground/textures/terrain_rough.png");
    groundTex.metallic = readTexture("Assets/Ground/textures/terrain_spec.png");

    pathTex.baseColor = readTexture("Assets/Ground/textures/path_diff.jpg");
    pathTex.normalMap = readTexture("Assets/Ground/textures/path_normal.png");
    pathTex.roughness = readTexture("Assets/Ground/textures/path_rough.png");
}

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

void Game::update(float delta_step) {
    if (gameOver) return;

    if (waveActive && troopsRemainingInWave > 0) {
        spawnTimer += delta_step;
        if (spawnTimer > spawn_interval) {
            spawnTroop();
            troopsRemainingInWave--;
            spawnTimer = 0.0f;
        }
    }
    else if (troopsRemainingInWave <= 0 && troops.empty()) {
        if (waveActive) waveEndMessageTimer = 3.0f;
        waveActive = false;
    }

    if (waveEndMessageTimer > 0.0f) waveEndMessageTimer -= delta_step;

    for (auto it = troops.begin(); it != troops.end(); ) {
        float tx = pathWaypoints[it->currentWaypoint].x;
        float tz = pathWaypoints[it->currentWaypoint].z;
        float dx = tx - it->x;
        float dz = tz - it->z;
        float dist = sqrtf(dx * dx + dz * dz);
        float moveDist = it->speed * delta_step;

        if (dist > 0.001f) {
            float targetA = atan2f(dx, dz) * 180.0f / 3.14159f;
            float diff = targetA - it->rotation_yaw;
            while (diff < -180) diff += 360;
            while (diff > 180) diff -= 360;
            it->rotation_yaw += diff * delta_step * 10.0f;
        }

        if (dist <= moveDist) {
            it->x = tx; it->z = tz;
            it->currentWaypoint++;
        }
        else {
            it->x += (dx / dist) * moveDist;
            it->z += (dz / dist) * moveDist;
        }

        if (it->currentWaypoint >= (int)pathWaypoints.size()) {
            lives--; it = troops.erase(it);
        }
        else ++it;
    }

    for (size_t i = 0; i < active_defenses.size(); i++) {
        auto& tower = active_defenses[i];
        float range = tower.tower_range;

        if (tower.current_cooldown > 0.0f)
            tower.current_cooldown -= delta_step;

        Troop* target = nullptr;
        float  bestDist = range;
        for (auto& troop : troops) {
            float d = sqrtf(powf(troop.x - tower.x, 2) + powf(troop.z - tower.z, 2));
            if (d < bestDist) { bestDist = d; target = &troop; }
        }

        if (target) {
            const auto& assets = tower_assets[(int)tower.tower_variant];
            float adx = target->x - tower.x;
            float adz = target->z - tower.z;
            float dist2D = sqrtf(adx * adx + adz * adz);

            float gunPivotX = tower.x;
            float gunPivotY = assets.base_y_offset
                + 0.5f * assets.rotate_y_offset
                + 0.5f * assets.gun_y_offset;
            float gunPivotZ = tower.z;
            float ady = target->altitude - gunPivotY;

            float targetYaw = atan2f(adx, adz) * (180.0f / 3.14159f);
            float targetPitch = atan2f(ady, dist2D) * (180.0f / 3.14159f);

            float yDiff = targetYaw - tower.current_yaw;
            while (yDiff < -180) yDiff += 360;
            while (yDiff > 180) yDiff -= 360;
            tower.current_yaw += yDiff * delta_step * tower.tower_rotate_speed;
            tower.current_pitch += (targetPitch - tower.current_pitch)
                * delta_step * tower.tower_rotate_speed;

            float pitchDiff = targetPitch - tower.current_pitch;

            if (tower.current_cooldown <= 0.0f
                && fabs(yDiff) < 15.0f && fabs(pitchDiff) < 10.0f)
            {
                float yawRad = tower.current_yaw * (3.14159f / 180.0f);
                float pitchRad = tower.current_pitch * (3.14159f / 180.0f);

                float fwdX = sinf(yawRad) * cosf(pitchRad);
                float fwdY = sinf(pitchRad);
                float fwdZ = cosf(yawRad) * cosf(pitchRad);
                float rightX = cosf(yawRad);
                float rightZ = -sinf(yawRad);

                float muzzleFwd = 0.6f;
                float muzzleRight = 0.0f;
                float muzzleUp = 0.0f;
                bool  isRocket = false;

                if (tower.tower_variant == TowerType::MACHINE_GUN) {
                    const float gunRight[2] = { -0.32f, 0.32f };
                    muzzleFwd = 2.1f;
                    muzzleRight = gunRight[tower.nextBarrel % 2];
                    muzzleUp = 0.0f;
                    tower.nextBarrel = (tower.nextBarrel + 1) % 2;
                }
                else if (tower.tower_variant == TowerType::ROCKETS) {
                    float right = 1.5f;
                    const float tubeRight[8] = { -right,  right, -right + 0.2f, right - 0.2f,
                                                 -right,  right, -right + 0.2f, right - 0.2f };
                    const float tubeUp[8] = { 0.30f, 0.30f, 0.10f, 0.10f,
                                                -0.10f,-0.10f,-0.30f,-0.30f };
                    int idx = tower.nextBarrel % 8;
                    muzzleFwd = 0.6f;
                    muzzleRight = tubeRight[idx];
                    muzzleUp = tubeUp[idx];
                    isRocket = true;
                    tower.nextBarrel = (tower.nextBarrel + 1) % 8;
                }
                else if (tower.tower_variant == TowerType::SNIPER) {
                    muzzleFwd = 1.0f;
                }

                float upX = -sinf(yawRad) * sinf(pitchRad);
                float upY = cosf(pitchRad);
                float upZ = -cosf(yawRad) * sinf(pitchRad);

                float spawnX = gunPivotX + fwdX * muzzleFwd + rightX * muzzleRight + upX * muzzleUp;
                float spawnY = gunPivotY + fwdY * muzzleFwd + upY * muzzleUp;
                float spawnZ = gunPivotZ + fwdZ * muzzleFwd + rightZ * muzzleRight + upZ * muzzleUp;

                spawnProjectile(spawnX, spawnY, spawnZ,
                    target->x, target->altitude, target->z,
                    tower.tower_dmg, isRocket);
                tower.current_cooldown = tower.tower_fire_rate;
            }
        }
    }

    for (auto it = active_bullets.begin(); it != active_bullets.end(); ) {
        it->x += it->vx * delta_step;
        it->y += it->vy * delta_step;
        it->z += it->vz * delta_step;
        it->life_span -= delta_step;

        bool destroyed = false;
        for (auto& enemy : troops) {
            float dist = sqrtf(powf(enemy.x - it->x, 2)
                + powf(enemy.altitude - it->y, 2)
                + powf(enemy.z - it->z, 2));
            float hitbox_radius = (enemy.variant == CAR) ? 0.8f
                : (enemy.variant == TANK) ? 1.6f : 2.0f;
            if (dist < hitbox_radius) {
                enemy.health -= it->damage;
                destroyed = true;
                break;
            }
        }
        if (destroyed || it->life_span <= 0) it = active_bullets.erase(it);
        else ++it;
    }

    for (auto it = troops.begin(); it != troops.end(); ) {
        if (it->health <= 0) { gold += 15; it = troops.erase(it); }
        else ++it;
    }
    if (lives <= 0) { lives = 0; gameOver = true; }
}

static bool isOnRoad(float wx, float wz,
    const std::vector<P>& waypoints, float halfWidth) {
    for (int i = 0; i + 1 < (int)waypoints.size(); i++) {
        float ax = waypoints[i].x, az = waypoints[i].z;
        float bx = waypoints[i + 1].x, bz = waypoints[i + 1].z;
        float abx = bx - ax, abz = bz - az;
        float apx = wx - ax, apz = wz - az;
        float ab2 = abx * abx + abz * abz;
        float t = (ab2 > 0.f) ? (apx * abx + apz * abz) / ab2 : 0.f;
        t = fmaxf(0.f, fminf(1.f, t));
        float cx = ax + t * abx, cz = az + t * abz;
        float d = sqrtf(powf(wx - cx, 2) + powf(wz - cz, 2));
        if (d < halfWidth+2.13f) return true;
    }
    return false;
}

static bool isOnTower(float wx, float wz,
    const std::vector<TowerInstance>& towers) {
    for (const auto& t : towers) {
        float dx = t.x - wx;
        float dz = t.z - wz;
        if ((dx * dx + dz * dz) < 18.9f)
            return true;
    }
    return false;
}

bool Game::raycastGroundPlane(float mx, float my,
    int winW, int winH,
    float& outX, float& outZ) {
    GLdouble mv[16], pr[16]; GLint vp[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, pr);
    glGetIntegerv(GL_VIEWPORT, vp);

    GLdouble ax, ay, az, bx, by, bz;
    gluUnProject(mx, vp[3] - my, 0.0, mv, pr, vp, &ax, &ay, &az);
    gluUnProject(mx, vp[3] - my, 1.0, mv, pr, vp, &bx, &by, &bz);

    float rayOy = (float)ay;
    float rayDy = (float)(by - ay);
    if (fabsf(rayDy) < 0.0001f) return false;
    float t = -rayOy / rayDy;
    if (t < 0) return false;
    outX = (float)(ax + t * (bx - ax));
    outZ = (float)(az + t * (bz - az));
    return true;
}

void Game::render(glm::mat4 P, glm::mat4 V) {
    glUseProgram(shader_id);
    glEnable(GL_TEXTURE_2D);

    GLint pLoc = glGetUniformLocation(shader_id, "P");
    GLint vLoc = glGetUniformLocation(shader_id, "V");
    GLint mLoc = glGetUniformLocation(shader_id, "M");

    glUniformMatrix4fv(pLoc, 1, GL_FALSE, glm::value_ptr(P));
    glUniformMatrix4fv(vLoc, 1, GL_FALSE, glm::value_ptr(V));

    glm::mat4 identityModel = glm::mat4(1.0f);
    glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(identityModel));

    float sun_dx = 0.5f, sun_dy = 1.0f, sun_dz = 0.5f;
    glm::vec3 worldSunDir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.5f));
    glm::vec3 eyeSunDir = glm::mat3(V) * worldSunDir;

    glUniform3f(glGetUniformLocation(shader_id, "lightDirGlobal"), eyeSunDir.x, eyeSunDir.y, eyeSunDir.z);

    GLint bulletActiveLoc = glGetUniformLocation(shader_id, "bulletActive");
    GLint bulletPosLoc = glGetUniformLocation(shader_id, "bulletPos");
    GLint bulletColorLoc = glGetUniformLocation(shader_id, "bulletColor");

    if (!active_bullets.empty()) {
        glUniform1f(bulletActiveLoc, 1.0f);
        glm::vec4 worldBulletPos = glm::vec4(active_bullets[0].x, active_bullets[0].y, active_bullets[0].z, 1.0f);
        glm::vec4 viewBulletPos = V * worldBulletPos;

        glUniform3f(bulletPosLoc, viewBulletPos.x, viewBulletPos.y, viewBulletPos.z);

        glUniform3f(bulletColorLoc, 1.0f, 0.8f, 0.2f);
    }
    else {
        glUniform1f(bulletActiveLoc, 0.0f);
    }

    float shadowMat[16] = {
        sun_dy, 0.0f, 0.0f, 0.0f,
        -sun_dx, 0.0f, -sun_dz, 0.0f,
        0.0f, 0.0f, sun_dy, 0.0f,
        0.0f, 0.0f, 0.0f, sun_dy
    };

    unbindTextures(shader_id);

    glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(identityModel));

    const float TILE = 10.0f;
    const float Y = -0.01f;

    glm::mat3 groundNormal = glm::mat3(1.0f);
    glUniformMatrix3fv(glGetUniformLocation(shader_id, "normalMatrix"),
        1, GL_FALSE, glm::value_ptr(groundNormal));

    // Podłoga
    glm::mat4 mGround = glm::mat4(1.0f);
    glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mGround));
    glm::mat3 nGround = glm::mat3(1.0f);
    glUniformMatrix3fv(glGetUniformLocation(shader_id, "normalMatrix"),
        1, GL_FALSE, glm::value_ptr(nGround));

    glm::vec3 eyeT = glm::normalize(glm::vec3(V * glm::vec4(1, 0, 0, 0)));
    glm::vec3 eyeB = glm::normalize(glm::vec3(V * glm::vec4(0, 0, 1, 0)));
    glUniform3f(glGetUniformLocation(shader_id, "fixedTangent"), eyeT.x, eyeT.y, eyeT.z);
    glUniform3f(glGetUniformLocation(shader_id, "fixedBitangent"), eyeB.x, eyeB.y, eyeB.z);
    glUniform1f(glGetUniformLocation(shader_id, "useFixedTBN"), 1.0f);
    bindTextures(shader_id, groundTex);

    glColor3f(1.0f, 1.0f, 1.0f);
    glNormal3f(0, 1, 0);
    glBegin(GL_QUADS);
    const float T = 12.0f;
    glTexCoord2f(0, 0); glVertex3f(-200, -0.01f, -200);
    glTexCoord2f(T, 0); glVertex3f(200, -0.01f, -200);
    glTexCoord2f(T, T); glVertex3f(200, -0.01f, 200);
    glTexCoord2f(0, T); glVertex3f(-200, -0.01f, 200);
    glEnd();
    unbindTextures(shader_id);
    glUniform1f(glGetUniformLocation(shader_id, "useFixedTBN"), 0.0f);

	// Ścieżka
    glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mGround));
    glUniformMatrix3fv(glGetUniformLocation(shader_id, "normalMatrix"),
        1, GL_FALSE, glm::value_ptr(nGround));
    bindTextures(shader_id, pathTex);
    glColor3f(1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(shader_id, "texBlendScale"), 4.3f);
    bindTextures(shader_id, pathTex);
    game_pathway.Draw();
    unbindTextures(shader_id);
    glUniform1f(glGetUniformLocation(shader_id, "texBlendScale"), 0.0f);
    unbindTextures(shader_id);

    float time = (float)glfwGetTime();

    // RENDER SHADOWS
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glColor4f(0.05f, 0.12f, 0.05f, 0.45f);

    glClear(GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

    glUseProgram(shader_id);
    unbindTextures(shader_id);

    glm::mat3 shadowNormals = glm::mat3(1.0f);
    glUniformMatrix3fv(glGetUniformLocation(shader_id, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(shadowNormals));

    for (const auto& tower : active_defenses) {
        const auto& assets = tower_assets[(int)tower.tower_variant];

        glm::mat4 mBase = glm::mat4(1.0f);
        mBase = glm::translate(mBase, glm::vec3(tower.x, assets.base_y_offset, tower.z));
        mBase = glm::scale(mBase, glm::vec3(0.5f, 0.5f, 0.5f));
        glm::mat4 mShadowBase = glm::make_mat4(shadowMat) * mBase;
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mShadowBase));
        renderMesh(assets.base_mesh, false);

        glm::mat4 mRotate = glm::translate(mBase, glm::vec3(0.0f, assets.rotate_y_offset, 0.0f));
        mRotate = glm::rotate(mRotate, glm::radians(tower.current_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 mShadowRotate = glm::make_mat4(shadowMat) * mRotate;
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mShadowRotate));
        renderMesh(assets.rotate_mesh, false);

        glm::mat4 mGun = glm::translate(mRotate, glm::vec3(0.0f, assets.gun_y_offset, 0.0f));
        mGun = glm::rotate(mGun, glm::radians(-tower.current_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 mShadowGun = glm::make_mat4(shadowMat) * mGun;
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mShadowGun));
        renderMesh(assets.gun_mesh, false);
    }

    for (const auto& troop : troops) {
        glm::mat4 M = glm::mat4(1.0f);
        M = glm::translate(M, glm::vec3(troop.x, troop.altitude, troop.z));
        M = glm::rotate(M, glm::radians(troop.rotation_yaw), glm::vec3(0, 1, 0));
        M = glm::scale(M, glm::vec3(0.3f, 0.3f, 0.3f));
        glm::mat4 mShadow = glm::make_mat4(shadowMat) * M;
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mShadow));

        renderMesh(troop_assets[(int)troop.variant].base_mesh, false);
    }

    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    // RENDER TROOPS
    for (const auto& troop : troops) {
        const auto& assets = troop_assets[(int)troop.variant];

        glm::mat4 M = glm::mat4(1.0f);
        M = glm::translate(M, glm::vec3(troop.x, troop.altitude, troop.z));
        M = glm::rotate(M, glm::radians(troop.rotation_yaw), glm::vec3(0, 1, 0));
        M = glm::scale(M, glm::vec3(0.3f, 0.3f, 0.3f));
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(M));

        glm::mat4 MV = V * M;
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(MV)));
        glUniformMatrix3fv(glGetUniformLocation(shader_id, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMat));

        bindTextures(shader_id, assets.base_tex);
        renderMesh(assets.base_mesh, true);
        unbindTextures(shader_id);

        if (troop.variant == TroopType::TANK) {
            float wheelRollAngle = time * troop.speed * 120.0f;
            float wheelOffsetsZ[2] = { -3.5f, 4.65f };
            float wheelOffsetY = -1.9f;
            for (int w = 0; w < 2; w++) {
                glm::mat4 mWheel = M;
                mWheel = glm::translate(mWheel, glm::vec3(0.0f, wheelOffsetY, wheelOffsetsZ[w]));
                mWheel = glm::scale(mWheel, glm::vec3(0.95f, 0.95f, 0.95f));
                mWheel = glm::rotate(mWheel, glm::radians(wheelRollAngle), glm::vec3(1.0f, 0.0f, 0.0f));
                mWheel = glm::rotate(mWheel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mWheel));

                bindTextures(shader_id, assets.base_tex);
                renderMesh(assets.wheel_mesh, true);
            }
        }

        if (troop.variant == TroopType::HELICOPTER) {
            float propSpinAngle = time * 360.0f;
            glm::mat4 mProp = M;
            mProp = glm::translate(mProp, glm::vec3(0.0f, troop.altitude - 0.5f, 0.5f));
            mProp = glm::rotate(mProp, glm::radians(propSpinAngle), glm::vec3(0.0f, 1.0f, 0.0f));
            glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mProp));

            bindTextures(shader_id, assets.base_tex);
            renderMesh(assets.prop_mesh, true);
        }
    }

    // RENDER TOWERS
    for (const auto& tower : active_defenses) {
        const auto& assets = tower_assets[(int)tower.tower_variant];

        // BASE
        glm::mat4 mBase = glm::mat4(1.0f);
        mBase = glm::translate(mBase, glm::vec3(tower.x, assets.base_y_offset, tower.z));
        mBase = glm::scale(mBase, glm::vec3(0.5f, 0.5f, 0.5f));
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mBase));

        glm::mat3 nBase = glm::transpose(glm::inverse(glm::mat3(V * mBase)));
        glUniformMatrix3fv(glGetUniformLocation(shader_id, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(nBase));

        bindTextures(shader_id, assets.base_tex);
        renderMesh(assets.base_mesh, true);

        // ROTATE
        glm::mat4 mRotate = glm::translate(mBase, glm::vec3(0.0f, assets.rotate_y_offset, 0.0f));
        mRotate = glm::rotate(mRotate, glm::radians(tower.current_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mRotate));

        glm::mat3 nRotate = glm::transpose(glm::inverse(glm::mat3(V * mRotate)));
        glUniformMatrix3fv(glGetUniformLocation(shader_id, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(nRotate));

        bindTextures(shader_id, assets.rotate_tex);
        renderMesh(assets.rotate_mesh, true);

        // GUN
        glm::mat4 mGun = glm::translate(mRotate, glm::vec3(0.0f, assets.gun_y_offset, 0.0f));
        mGun = glm::rotate(mGun, glm::radians(-tower.current_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mGun));

        glm::mat3 nGun = glm::transpose(glm::inverse(glm::mat3(V * mGun)));
        glUniformMatrix3fv(glGetUniformLocation(shader_id, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(nGun));

        bindTextures(shader_id, assets.gun_tex);
        renderMesh(assets.gun_mesh, true);
    }

    // BULLETS PASS
    for (const auto& b : active_bullets) {
        float spd = sqrtf(b.vx * b.vx + b.vy * b.vy + b.vz * b.vz);
        float yaw = atan2f(b.vx, b.vz) * 180.0f / 3.14159f;
        float pitch = -asinf(b.vy / spd) * 180.0f / 3.14159f;

        glm::mat4 M = glm::mat4(1.0f);
        M = glm::translate(M, glm::vec3(b.x, b.y, b.z));
        M = glm::rotate(M, glm::radians(yaw), glm::vec3(0, 1, 0));
        M = glm::rotate(M, glm::radians(pitch), glm::vec3(1, 0, 0));
        M = glm::rotate(M, glm::radians(-90.0f), glm::vec3(0, 1, 0));

        if (b.isRocket) {
            M = glm::scale(M, glm::vec3(0.15f, 0.15f, 0.15f));
            glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(M));
            bindTextures(shader_id, projectile_assets.rocket_tex);
            renderMesh(projectile_assets.rocket_mesh, true);
        }
        else {
            M = glm::scale(M, glm::vec3(0.1f, 0.1f, 0.1f));
            glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(M));
            bindTextures(shader_id, projectile_assets.bullet_tex);
            renderMesh(projectile_assets.bullet_mesh, true);
        }
    }

    unbindTextures(shader_id);
    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);

    // GHOST PREVIEW PLACEMENT
    if (isBuilding && !gameOver) {
        double mx, my;
        int win_w, win_h;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        glfwGetWindowSize(glfwGetCurrentContext(), &win_w, &win_h);

        float ground_x, ground_z;
        if (raycastGroundPlane((float)mx, (float)my, win_w, win_h, ground_x, ground_z)) {
            ghost_wx = ground_x;
            ghost_wz = ground_z;
        }

        TowerInstance preview = getTowerDefaults(ghostType);
        float ghostRange = preview.tower_range;
        bool onRoad = isOnRoad(ghost_wx, ghost_wz, pathWaypoints, game_pathway.roadWidth);
        bool onTower = isOnTower(ghost_wx, ghost_wz, active_defenses);

        const auto& assets = tower_assets[(int)ghostType];
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glPushMatrix();
        glTranslatef(ghost_wx, 0.1f, ghost_wz);
        glBegin(GL_LINE_LOOP);
        glColor4f(1.0f, 1.0f, 1.0f, 0.6f);
        for (int i = 0; i < 36; i++) {
            float a = i * 10.0f * 3.1415f / 180.0f;
            glVertex3f(cosf(a) * ghostRange, 0, sinf(a) * ghostRange);
        }
        glEnd();
        glPopMatrix();

        glUseProgram(shader_id);
        glm::mat4 mGhost = glm::mat4(1.0f);
        mGhost = glm::translate(mGhost, glm::vec3(ghost_wx, assets.base_y_offset, ghost_wz));
        mGhost = glm::scale(mGhost, glm::vec3(0.5f, 0.5f, 0.5f));
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mGhost));

        if (onRoad || onTower) glColor4f(1.0f, 0.3f, 0.3f, 0.5f);
        else glColor4f(1.0f, 1.0f, 1.0f, 0.5f);

        renderMesh(assets.base_mesh, false);

        mGhost = glm::translate(mGhost, glm::vec3(0.0f, assets.rotate_y_offset, 0.0f));
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mGhost));
        renderMesh(assets.rotate_mesh, false);

        mGhost = glm::translate(mGhost, glm::vec3(0.0f, assets.gun_y_offset, 0.0f));
        glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(mGhost));
        renderMesh(assets.gun_mesh, false);

        glUseProgram(0);
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }

    renderHUD();
}

void Game::spawnProjectile(float sx, float sy, float sz,
    float tx, float ty, float tz,
    float damage_val, bool isRocket) {
    Projectile p;
    p.x = sx; p.y = sy; p.z = sz;
    p.damage = damage_val;
    p.isRocket = isRocket;
    float dx = tx - sx, dy = ty - sy, dz = tz - sz;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    float speed = 50.0f;
    p.vx = (dx / dist) * speed; p.vy = (dy / dist) * speed; p.vz = (dz / dist) * speed;
    p.life_span = 2.0f;
    active_bullets.push_back(p);
}

void Game::spawnTroop() {
    if (pathWaypoints.empty()) return;
    Troop t;
    t.x = pathWaypoints[0].x;
    t.z = pathWaypoints[0].z;
    t.currentWaypoint = 1;
    t.rotation_yaw = 0.0f;

    int roll = rand() % 100;
    if (roll < 50) t.variant = CAR;
    else if (roll < 80) t.variant = TANK;
    else                t.variant = HELICOPTER;

    if (t.variant == CAR) { t.health = 40;  t.speed = 3.5f;  t.altitude = 0.8f; }
    else if (t.variant == TANK) { t.health = 150; t.speed = 2.0f;  t.altitude = 1.1f; }
    else { t.health = 60;  t.speed = 12.0f; t.altitude = 6.0f; }

    troops.push_back(t);
}

void Game::renderHUD() {
    float W = (float)hud_win_w;
    float H = (float)hud_win_h;

    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, W, H, 0);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glDisable(GL_TEXTURE_2D);

    glColor3f(1, 1, 0);
    std::string wave_msg = (wave > 0) ? "  Wave: " + std::to_string(wave)
        : "  Press [SPACE] to start";
    std::string hud = "Gold: " + std::to_string(gold)
        + "  Lives: " + std::to_string(lives) + wave_msg;
    glRasterPos2f(20, 30);
    for (char c : hud) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, c);

    glColor3f(1, 1, 0);
    std::string speed_msg = paused
        ? "  [PAUSED]"
        : "  Speed: " + std::to_string((int)gameSpeed) + "x";
    glRasterPos2f(20, 55);
    for (char c : speed_msg) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, c);

    if (waveEndMessageTimer > 0.0f) {
        glColor3f(0.0f, 1.0f, 0.0f);
        std::string msg = "Wave " + std::to_string(wave) + " complete";
        int   textWidth = glutBitmapLength(GLUT_BITMAP_HELVETICA_18,
            (const unsigned char*)msg.c_str());
        glRasterPos2f((W - textWidth) / 2.0f, H / 2.0f);
        for (char c : msg) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    if (gameOver) {
        glColor3f(1.0f, 0.0f, 0.0f);
        std::string msg = "GAME OVER";
        int textWidth = glutBitmapLength(GLUT_BITMAP_HELVETICA_18,
            (const unsigned char*)msg.c_str());
        glRasterPos2f((W - textWidth) / 2.0f, H / 2.0f);
        for (char c : msg) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
}

void Game::selectTowerType(int i) {
    if (i >= 0 && i <= 2) selectedType = (TowerType)i;
}

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

    std::cout << "Postawiono wieze typu " << (int)selectedType
        << " | X=" << ghost_wx << " Z=" << ghost_wz
        << " | Zloto: " << gold << std::endl;
}

void Game::loadModels(std::string root, std::string file, std::vector<VertexData>& buffer) {
    tinyobj::attrib_t                attr;
    std::vector<tinyobj::shape_t>    shp;
    std::vector<tinyobj::material_t> mat;
    std::string w, e;

    if (!tinyobj::LoadObj(&attr, &shp, &mat, &w, &e,
        (root + file).c_str(), root.c_str())) return;

    for (const auto& s : shp) {
        size_t off = 0;
        for (size_t f = 0; f < s.mesh.num_face_vertices.size(); f++) {
            int fv = s.mesh.num_face_vertices[f];
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = s.mesh.indices[off + v];
                VertexData vd;

                vd.x = attr.vertices[3 * idx.vertex_index + 0];
                vd.y = attr.vertices[3 * idx.vertex_index + 1];
                vd.z = attr.vertices[3 * idx.vertex_index + 2];

                if (idx.normal_index >= 0) {
                    vd.nx = attr.normals[3 * idx.normal_index + 0];
                    vd.ny = attr.normals[3 * idx.normal_index + 1];
                    vd.nz = attr.normals[3 * idx.normal_index + 2];
                }
                else {
                    vd.nx = 0; vd.ny = 1; vd.nz = 0;
                }

                if (idx.texcoord_index >= 0) {
                    vd.u = attr.texcoords[2 * idx.texcoord_index + 0];
                    vd.v = attr.texcoords[2 * idx.texcoord_index + 1];
                }
                else {
                    vd.u = 0.0f;
                    vd.v = 0.0f;
                }

                int mat_id = s.mesh.material_ids[f];
                if (mat_id >= 0 && mat_id < (int)mat.size()) {
                    vd.r = mat[mat_id].diffuse[0];
                    vd.g = mat[mat_id].diffuse[1];
                    vd.b = mat[mat_id].diffuse[2];
                }
                else {
                    vd.r = 0.4f; vd.g = 0.4f; vd.b = 0.45f;
                }

                buffer.push_back(vd);
            }
            off += fv;
        }
    }
}

void Game::loadTroopWave(std::string resource_path) {
    std::ifstream f(resource_path);
    if (!f.is_open()) return;
    std::string variant;
    float speed, hp;
    while (f >> variant >> speed >> hp) {
        Troop t;
        t.x = pathWaypoints[0].x;
        t.z = pathWaypoints[0].z;
        t.currentWaypoint = 1;
        t.speed = speed;
        t.health = hp;
        troops.push_back(t);
    }
}

void Game::tileCenter(int col, int row,
    float& world_x, float& world_z) {
    float half = 24 * 5.0f * 0.5f;
    world_x = -half + col * 5.0f + 2.5f;
    world_z = -half + row * 5.0f + 2.5f;
}

GLuint Game::loadShader(const char* vertexPath, const char* fragmentPath) {
    std::string  vertexCode, fragmentCode;
    std::ifstream vFile, fFile;

    try {
        vFile.open(vertexPath);  fFile.open(fragmentPath);
        std::stringstream vs, fs;
        vs << vFile.rdbuf();  fs << fFile.rdbuf();
        vFile.close();        fFile.close();
        vertexCode = vs.str();
        fragmentCode = fs.str();
    }
    catch (std::ifstream::failure&) {
        std::cout << "Blad: Nie udalo sie wczytac plikow shadera!" << std::endl;
    }

    const char* vSrc = vertexCode.c_str();
    const char* fSrc = fragmentCode.c_str();

    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vSrc, NULL);
    glCompileShader(vertex);

    GLint success;
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertex, 512, NULL, infoLog);
        std::cout << "VERTEX SHADER COMPILE ERROR:\n" << infoLog << std::endl;
    }

    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fSrc, NULL);
    glCompileShader(fragment);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertex);
    glAttachShader(prog, fragment);
    glLinkProgram(prog);

    GLint linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);

    if (!linked) {
        char infoLog[1024];
        glGetProgramInfoLog(prog, 1024, NULL, infoLog);
        std::cout << "SHADER LINK ERROR:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return prog;
}