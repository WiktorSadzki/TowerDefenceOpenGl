#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h" 
#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include "Game.h"
#include <sstream>

static float spawn_interval = 0.1f;

TowerInstance Game::getTowerDefaults(TowerType type) {
    TowerInstance t;
    t.tower_variant = type;
    t.cost = 20; t.tower_rotate_speed = 15.0f;
    t.tower_fire_rate = 0.15f; t.tower_dmg = 20.0f; t.tower_range = 10.0f;

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

    pathWaypoints.clear();
    for (const auto& splinePt : game_pathway.splinePoints) {
        P sharpPt;
        sharpPt.x = splinePt.x;
        sharpPt.z = splinePt.z;
        pathWaypoints.push_back(sharpPt);
    }

    std::string tower_directory = "Assets/Towers/source/";
    loadModels(tower_directory, "base.obj", tower_assets[0].base_mesh);
    loadModels(tower_directory, "machine_rotate.obj", tower_assets[0].rotate_mesh);
    loadModels(tower_directory, "machine_gun.obj", tower_assets[0].gun_mesh);
    tower_assets[0].base_y_offset = 0.4f;
    tower_assets[0].gun_y_offset = 1.8f;
    tower_assets[0].rotate_y_offset = 0.8f;

    loadModels(tower_directory, "base.obj", tower_assets[1].base_mesh);
    loadModels(tower_directory, "rockets_rotate.obj", tower_assets[1].rotate_mesh);
    loadModels(tower_directory, "rockets_gun.obj", tower_assets[1].gun_mesh);
    tower_assets[1].base_y_offset = 0.4f;
    tower_assets[1].gun_y_offset = 0.6f;
    tower_assets[1].rotate_y_offset = 2.8f;

    loadModels(tower_directory, "base.obj", tower_assets[2].base_mesh);
    loadModels(tower_directory, "sniper_rotate.obj", tower_assets[2].rotate_mesh);
    loadModels(tower_directory, "sniper_gun.obj", tower_assets[2].gun_mesh);
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
}

void Game::startNextWave() {
    if (waveActive || waveEndMessageTimer > 0.0f) return;
    waveActive = true;
    wave++;
    if (wave <= 3)
        troopsRemainingInWave = 3 + wave;
    else
        troopsRemainingInWave = 5 + (wave * 2);
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

        if (tower.current_cooldown > 0.0f) {
            tower.current_cooldown -= delta_step;
        }

        Troop* target = nullptr;
        float bestDist = range;
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
            tower.current_pitch += (targetPitch - tower.current_pitch) * delta_step * tower.tower_rotate_speed;

            float pitchDiff = targetPitch - tower.current_pitch;

            if (tower.current_cooldown <= 0.0f && fabs(yDiff) < 15.0f && fabs(pitchDiff) < 10.0f) {
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
                    const float tubeRight[8] = { -right,  right,  -right+0.2,  right-0.2,  -right,  right,  -right+0.2,  right-0.2 };
                    const float tubeUp[8] = { 0.30f,  0.30f,   0.10f,  0.10f,  -0.10f, -0.10f,  -0.30f, -0.30f };
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
            float dist = sqrtf(powf(enemy.x - it->x, 2) + powf(enemy.altitude - it->y, 2) + powf(enemy.z - it->z, 2));
            
            float hitbox_radius = 1.2f;
            if (enemy.variant == CAR) {
                hitbox_radius = 0.8f;
            }
            else if (enemy.variant == TANK) {
                hitbox_radius = 1.6f;
            }
            else {
                hitbox_radius = 2.0f;
            }
            
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

static void renderMesh(const std::vector<VertexData>& mesh, bool useColor = true) {
    glBegin(GL_TRIANGLES);
    for (const auto& v : mesh) {
        if (useColor) glColor3f(v.r, v.g, v.b);
        glNormal3f(v.nx, v.ny, v.nz);
        glVertex3f(v.x, v.y, v.z);
    }
    glEnd();
}

static bool isOnRoad(float wx, float wz, const std::vector<P>& waypoints, float halfWidth) {
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
        if (d < halfWidth) return true;
    }
    return false;
}

bool Game::raycastGroundPlane(float mx, float my, int winW, int winH, float& outX, float& outZ) {
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

void Game::render() {
    glUseProgram(shader_id);

    float sun_dx = 0.5f;
    float sun_dy = 1.0f;
    float sun_dz = 0.5f;

    GLint globalDirLoc = glGetUniformLocation(shader_id, "lightDirGlobal");
    glUniform3f(globalDirLoc, sun_dx, sun_dy, sun_dz);

    GLint bulletActiveLoc = glGetUniformLocation(shader_id, "bulletActive");
    GLint bulletPosLoc = glGetUniformLocation(shader_id, "bulletPos");
    GLint bulletColorLoc = glGetUniformLocation(shader_id, "bulletColor");

    if (!active_bullets.empty()) {
        glUniform1f(bulletActiveLoc, 1.0f); 
        glUniform3f(bulletPosLoc, active_bullets[0].x, active_bullets[0].y, active_bullets[0].z);
        glUniform3f(bulletColorLoc, 1.0f, 0.8f, 0.2f); 
    }
    else {
        glUniform1f(bulletActiveLoc, 0.0f); 
    }

    float shadowMat[16] = {
        sun_dy,   0.0f,    0.0f,    0.0f,
       -sun_dx,   0.0f,   -sun_dz,  0.0f,
        0.0f,     0.0f,    sun_dy,  0.0f,
        0.0f,     0.0f,    0.0f,    sun_dy
    };

    glBegin(GL_QUADS);
    glColor3f(0.2f, 0.5f, 0.2f); glNormal3f(0, 1, 0);
    glVertex3f(-50, -0.01f, -50); glVertex3f(50, -0.01f, -50);
    glVertex3f(50, -0.01f, 50); glVertex3f(-50, -0.01f, 50);
    glEnd();

    game_pathway.Draw();

    float time = (float)glfwGetTime();

    for (const auto& troop : troops) {
        const auto& assets = troop_assets[(int)troop.variant];

        glPushMatrix();
        glTranslatef(troop.x, troop.altitude, troop.z);
        glRotatef(troop.rotation_yaw, 0, 1, 0);
        glScalef(0.3f, 0.3f, 0.3f);
        renderMesh(assets.base_mesh, true);

        if (troop.variant == TroopType::TANK) {
            float wheelRollAngle = time * troop.speed * 120.0f;
            float wheelOffsetsX[2] = { 0.0f, 0.0f };
            float wheelOffsetsZ[2] = { -3.5f, 4.65f};
            float wheelOffsetY = -1.9f;

            for (int w = 0; w < 2; w++) {
                glPushMatrix();
                glTranslatef(wheelOffsetsX[w], wheelOffsetY, wheelOffsetsZ[w]);
                glScalef(0.95f, 0.95f, 0.95f);
                glRotatef(wheelRollAngle, 1.0f, 0.0f, 0.0f);
                glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
                renderMesh(assets.wheel_mesh, true);
                glPopMatrix();
            }
        }

        if (troop.variant == TroopType::HELICOPTER) {
            float propSpinAngle = time * 360.0f;
            glPushMatrix();
            glTranslatef(0.0f, troop.altitude - 0.5f, 0.5f);
            glRotatef(propSpinAngle, 0.0f, 1.0f, 0.0f);
            renderMesh(assets.prop_mesh, true);
            glPopMatrix();

        }
        glPopMatrix();
    }

    for (const auto& tower : active_defenses) {
        const auto& assets = tower_assets[(int)tower.tower_variant];

        glPushMatrix();
        glTranslatef(tower.x, assets.base_y_offset, tower.z);
        glScalef(0.5f, 0.5f, 0.5f);
        renderMesh(assets.base_mesh, true);
        glTranslatef(0, assets.rotate_y_offset, 0);
        glRotatef(tower.current_yaw, 0, 1, 0);
        renderMesh(assets.rotate_mesh, true);
        glTranslatef(0, assets.gun_y_offset, 0);
        glRotatef(-tower.current_pitch, 1, 0, 0);
        renderMesh(assets.gun_mesh, true);
        glPopMatrix();

        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);

        glPushMatrix();
        glMultMatrixf(shadowMat);
        glTranslatef(tower.x, assets.base_y_offset, tower.z);
        glScalef(0.5f, 0.5f, 0.5f);
        glColor4f(0.05f, 0.12f, 0.05f, 0.45f);
        renderMesh(assets.base_mesh, false);
        glTranslatef(0, assets.rotate_y_offset, 0);
        glRotatef(tower.current_yaw, 0, 1, 0);
        renderMesh(assets.rotate_mesh, false);
        glTranslatef(0, assets.gun_y_offset, 0);
        glRotatef(-tower.current_pitch, 1, 0, 0);
        renderMesh(assets.gun_mesh, false);
        glPopMatrix();

        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }

    glUseProgram(0);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);

    glPushMatrix();
    glMultMatrixf(shadowMat);

    glColor4f(0.05f, 0.12f, 0.05f, 0.45f);

    for (const auto& troop : troops) {
        glPushMatrix();
        glTranslatef(troop.x, troop.altitude, troop.z);
        glRotatef(troop.rotation_yaw, 0, 1, 0);
        glScalef(0.3f, 0.3f, 0.3f);
        renderMesh(troop_assets[(int)troop.variant].base_mesh, false);
        glPopMatrix();
    }

    for (const auto& tower : active_defenses) {
        const auto& assets = tower_assets[(int)tower.tower_variant];
        glPushMatrix();
        glTranslatef(tower.x, assets.base_y_offset, tower.z);
        glScalef(0.5f, 0.5f, 0.5f);
        renderMesh(assets.base_mesh, false);
        glTranslatef(0, assets.rotate_y_offset, 0);
        glRotatef(tower.current_yaw, 0, 1, 0);
        renderMesh(assets.rotate_mesh, false);
        glTranslatef(0, assets.gun_y_offset, 0);
        glRotatef(-tower.current_pitch, 1, 0, 0);
        renderMesh(assets.gun_mesh, false);
        glPopMatrix();
    }

    glPopMatrix();

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    glDisable(GL_LIGHTING);
    for (const auto& b : active_bullets) {
        float spd = sqrtf(b.vx * b.vx + b.vy * b.vy + b.vz * b.vz);
        float yaw = atan2f(b.vx, b.vz) * 180.0f / 3.14159f;
        float pitch = -asinf(b.vy / spd) * 180.0f / 3.14159f;

        glPushMatrix();
        glTranslatef(b.x, b.y, b.z);
        glRotatef(yaw, 0, 1, 0);
        glRotatef(pitch, 1, 0, 0);
        glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);

        if (b.isRocket) {
            glScalef(0.15f, 0.15f, 0.15f);
            renderMesh(projectile_assets.rocket_mesh, true);
        }
        else {
            glScalef(0.1f, 0.1f, 0.1f);
            renderMesh(projectile_assets.bullet_mesh, true);
        }
        glPopMatrix();
    }
    glEnable(GL_LIGHTING);

    if (isBuilding && !gameOver) {
        double mx, my;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        int win_w, win_h;
        glfwGetWindowSize(glfwGetCurrentContext(), &win_w, &win_h);

        float ground_x, ground_z;
        if (raycastGroundPlane((float)mx, (float)my, win_w, win_h, ground_x, ground_z)) {
            ghost_wx = ground_x;
            ghost_wz = ground_z;
        }

        TowerInstance preview = getTowerDefaults(ghostType);
        float ghostRange = preview.tower_range;

        bool onRoad = isOnRoad(ghost_wx, ghost_wz, pathWaypoints, game_pathway.roadWidth);

        const auto& assets = tower_assets[(int)ghostType];
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glPushMatrix();
        glTranslatef(ghost_wx, 0.1f, ghost_wz);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 36; i++) {
            float a = i * 10.0f * 3.1415f / 180.0f;
            glVertex3f(cosf(a) * ghostRange, 0, sinf(a) * ghostRange);
        }
        glEnd();
        glPopMatrix();

        glPushMatrix();
        glTranslatef(ghost_wx, assets.base_y_offset, ghost_wz);
        glScalef(0.5f, 0.5f, 0.5f);
        if (onRoad) glColor4f(1.0f, 0.3f, 0.3f, 0.5f);
        else        glColor4f(1.0f, 1.0f, 1.0f, 0.5f);

        renderMesh(assets.base_mesh, false);
        glTranslatef(0, assets.rotate_y_offset, 0);
        renderMesh(assets.rotate_mesh, false);
        glTranslatef(0, assets.gun_y_offset, 0);
        renderMesh(assets.gun_mesh, false);
        glPopMatrix();

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

    if (roll < 50)       t.variant = CAR;
    else if (roll < 80)  t.variant = TANK;
    else                 t.variant = HELICOPTER;

    if (t.variant == CAR) { t.health = 40;  t.speed = 3.5f; t.altitude = 0.8f; }
    else if (t.variant == TANK) { t.health = 150; t.speed = 2.0f; t.altitude = 1.1f; }
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
    std::string wave_msg = (wave > 0) ? "  Wave: " + std::to_string(wave) : "  Press [SPACE] to start";
    std::string hud = "Gold: " + std::to_string(gold) + "  Lives: " + std::to_string(lives) + wave_msg;
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

        int textWidth = glutBitmapLength(GLUT_BITMAP_HELVETICA_18, (const unsigned char*)msg.c_str());
        float centerX = (W - textWidth) / 2.0f;
        float centerY = H / 2.0f;

        glRasterPos2f(centerX, centerY);
        for (char c : msg) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    if (gameOver) {
        glColor3f(1.0f, 0.0f, 0.0f);
        std::string msg = "GAME OVER";

        int textWidth = glutBitmapLength(GLUT_BITMAP_HELVETICA_18, (const unsigned char*)msg.c_str());
        float centerX = (W - textWidth) / 2.0f;
        float centerY = H / 2.0f;

        glRasterPos2f(centerX, centerY);
        for (char c : msg) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
}

void Game::selectTowerType(int i) { 
    if (i >= 0 && i <= 2) selectedType = (TowerType)i; 
}

void Game::loadModels(std::string root, std::string file, std::vector<VertexData>& buffer) {
    tinyobj::attrib_t attr; std::vector<tinyobj::shape_t> shp; std::vector<tinyobj::material_t> mat; std::string w, e;
    if (!tinyobj::LoadObj(&attr, &shp, &mat, &w, &e, (root + file).c_str(), root.c_str())) return;
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
    std::ifstream wave_input_stream(resource_path);
    if (!wave_input_stream.is_open()) return;
    std::string troop_variant;
    float current_move_speed, current_max_hp;
    while (wave_input_stream >> troop_variant >> current_move_speed >> current_max_hp) {
        Troop troop_instance;
        troop_instance.x = pathWaypoints[0].x;
        troop_instance.z = pathWaypoints[0].z;
        troop_instance.currentWaypoint = 1;
        troop_instance.speed = current_move_speed;
        troop_instance.health = current_max_hp;
        troops.push_back(troop_instance);
    }
    wave_input_stream.close();
}

void Game::tileCenter(int col_idx, int row_idx, float& world_x_pos, float& world_z_pos) {
    float half = 24 * 5.0f * 0.5f;
    world_x_pos = -half + col_idx * 5.0f + 2.5f;
    world_z_pos = -half + row_idx * 5.0f + 2.5f;
}

void Game::toggleBuildMode(int typeIndex) {
    TowerType t = (TowerType)typeIndex;
    if (isBuilding && ghostType == t) isBuilding = false;
    else { isBuilding = true; ghostType = t; selectedType = t; }
}

void Game::tryPlaceTower(int mouse_x, int mouse_y, Camera& world_camera) {
    if (!isBuilding) return;

    TowerInstance turret_unit = getTowerDefaults(selectedType);

    int current_cost = turret_unit.cost;
    if (gold < current_cost) return;
    if (isOnRoad(ghost_wx, ghost_wz, pathWaypoints, game_pathway.roadWidth)) return;
    
    turret_unit.x = ghost_wx;
    turret_unit.z = ghost_wz;
    turret_unit.current_yaw = 0.0f;

    active_defenses.push_back(turret_unit);
    gold -= current_cost;

    std::cout << "Postawiono wieze typu " << (int)selectedType
        << " | X=" << ghost_wx << " Z=" << ghost_wz
        << " | Zloto: " << gold << std::endl;
}

GLuint Game::loadShader(const char* vertexPath, const char* fragmentPath) {
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;

    try {
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;

        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        vShaderFile.close();
        fShaderFile.close();

        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cout << "Blad: Nie udalo sie wczytac plikow shadera!" << std::endl;
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    GLuint vertex, fragment;

    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);

    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);

    GLuint programID = glCreateProgram();
    glAttachShader(programID, vertex);
    glAttachShader(programID, fragment);
    glLinkProgram(programID);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return programID;
}