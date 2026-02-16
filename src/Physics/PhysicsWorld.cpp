#include "PhysicsWorld.hpp"
#include <cmath>
#include <iostream>
#include <fstream> 
#include <sstream> 

PhysicsWorld::PhysicsWorld(float widthPixels, float heightPixels)
    : world(b2Vec2(0.0f, 0.0f)) 
{
    rng.seed(42);

    world.SetContactListener(&contactListener);

    worldWidthMeters = widthPixels / SCALE;
    worldHeightMeters = heightPixels / SCALE;

    createWalls(widthPixels, heightPixels);
    createWinZone();
    createRacers();
}

float PhysicsWorld::randomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

void PhysicsWorld::step(float timeStep, int velIter, int posIter) {
    if (isPaused || gameOver) return;

    world.SetGravity(enableGravity ? b2Vec2(0.0f, 9.8f) : b2Vec2(0.0f, 0.0f));
    
    contactListener.bodiesToCheck.clear();
    contactListener.wallsHit.clear(); // Limpiamos choques viejos
    contactListener.winnerBody = nullptr;

    world.Step(timeStep, velIter, posIter);

    if (contactListener.winnerBody != nullptr) {
        gameOver = true;
        for (int i = 0; i < dynamicBodies.size(); ++i) {
            if (dynamicBodies[i] == contactListener.winnerBody) {
                winnerIndex = i;
                break;
            }
        }
        std::cout << ">>> VICTORY: RACER " << winnerIndex << " <<<" << std::endl;
        return; 
    }

    if (enableChaos) {
        for (b2Body* body : contactListener.bodiesToCheck) {
            if (randomFloat(0.0f, 1.0f) < chaosChance) {
                b2Vec2 vel = body->GetLinearVelocity();
                float angleDev = randomFloat(-0.5f, 0.5f);
                float cs = cos(angleDev); float sn = sin(angleDev);
                float px = vel.x * cs - vel.y * sn;
                float py = vel.x * sn + vel.y * cs;
                body->SetLinearVelocity(b2Vec2(px * chaosBoost, py * chaosBoost));
            }
        }
    }

    if (enforceSpeed) {
        for (b2Body* b : dynamicBodies) {
            b2Vec2 vel = b->GetLinearVelocity();
            float speed = vel.Length();
            
            if (speed > targetSpeed + 2.0f) {
                vel *= 0.98f;
                b->SetLinearVelocity(vel);
            }
            else if (speed < targetSpeed - 0.5f || speed > targetSpeed + 0.5f) { 
                vel.Normalize();
                vel *= targetSpeed; 
                b->SetLinearVelocity(vel);
            }
        }
    }
}

// --- ACTUALIZACIÓN VISUAL (Flashes) ---
void PhysicsWorld::updateWallVisuals(float dt) {
    // 1. Procesar nuevos impactos
    for (b2Body* body : contactListener.wallsHit) {
        // Buscar qué pared es (lineal es rápido para <100 paredes)
        for (auto& wall : customWalls) {
            if (wall.body == body) {
                wall.flashTimer = 1.0f; // Flash al máximo
            }
        }
    }

    // 2. Decaer timers
    for (auto& wall : customWalls) {
        if (wall.flashTimer > 0.0f) {
            wall.flashTimer -= dt * 3.0f; // Velocidad del fade out
            if (wall.flashTimer < 0.0f) wall.flashTimer = 0.0f;
        }
    }
}

// --- SISTEMA DE GUARDADO Y CARGA ---

void PhysicsWorld::saveMap(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error al guardar el mapa: " << filename << std::endl;
        return;
    }

    file << "CONFIG " << targetSpeed << " " << currentRacerSize << " " 
         << currentRestitution << " " << enableChaos << "\n";

    file << "WINZONE " << winZonePos[0] << " " << winZonePos[1] << " " 
         << winZoneSize[0] << " " << winZoneSize[1] << "\n";

    for (const auto& wall : customWalls) {
        b2Vec2 pos = wall.body->GetPosition();
        file << "WALL " << pos.x << " " << pos.y << " " 
             << wall.width << " " << wall.height << "\n";
    }

    for (size_t i = 0; i < dynamicBodies.size(); ++i) {
        b2Body* b = dynamicBodies[i];
        b2Vec2 pos = b->GetPosition();
        b2Vec2 vel = b->GetLinearVelocity();
        float angle = b->GetAngle();
        float angVel = b->GetAngularVelocity();

        file << "RACER " << i << " " 
             << pos.x << " " << pos.y << " " 
             << vel.x << " " << vel.y << " " 
             << angle << " " << angVel << "\n";
    }

    std::cout << "Mapa guardado: " << filename << std::endl;
    file.close();
}

void PhysicsWorld::loadMap(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error al cargar el mapa: " << filename << std::endl;
        return;
    }

    clearCustomWalls();
    resetRacers(); 

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "CONFIG") {
            float spd, size, rest;
            bool chaos;
            ss >> spd >> size >> rest >> chaos;
            targetSpeed = spd;
            enableChaos = chaos;
            updateRacerSize(size); 
            updateRestitution(rest);
        }
        else if (type == "WINZONE") {
            float x, y, w, h;
            ss >> x >> y >> w >> h;
            updateWinZone(x, y, w, h);
        } 
        else if (type == "WALL") {
            float x, y, w, h;
            ss >> x >> y >> w >> h;
            addCustomWall(x, y, w, h);
        }
        else if (type == "RACER") {
            int id;
            float x, y, vx, vy, angle, angVel;
            ss >> id >> x >> y >> vx >> vy >> angle >> angVel;

            if (id >= 0 && id < dynamicBodies.size()) {
                b2Body* b = dynamicBodies[id];
                b->SetTransform(b2Vec2(x, y), angle);
                b->SetLinearVelocity(b2Vec2(vx, vy));
                b->SetAngularVelocity(angVel);
                b->SetAwake(true);
            }
        }
    }

    isPaused = true;
    std::cout << "Mapa cargado: " << filename << std::endl;
    file.close();
}

void PhysicsWorld::clearCustomWalls() {
    for (const auto& wall : customWalls) {
        world.DestroyBody(wall.body);
    }
    customWalls.clear();
}

void PhysicsWorld::addCustomWall(float x, float y, float w, float h) {
    b2BodyDef bd;
    bd.type = b2_staticBody;
    bd.position.Set(x, y);
    b2Body* body = world.CreateBody(&bd);
    b2PolygonShape box;
    box.SetAsBox(w / 2.0f, h / 2.0f);
    b2FixtureDef fd;
    fd.shape = &box;
    fd.friction = 0.0f;
    fd.restitution = 1.0f;
    body->CreateFixture(&fd);
    customWalls.push_back({body, w, h, 0.0f}); // FlashTimer = 0
}

void PhysicsWorld::updateCustomWall(int index, float x, float y, float w, float h) {
    if (index < 0 || index >= customWalls.size()) return;
    CustomWall& wall = customWalls[index];
    wall.body->SetTransform(b2Vec2(x, y), 0.0f);
    if (wall.width != w || wall.height != h) {
        wall.body->DestroyFixture(wall.body->GetFixtureList());
        b2PolygonShape box;
        box.SetAsBox(w / 2.0f, h / 2.0f);
        b2FixtureDef fd;
        fd.shape = &box;
        fd.friction = 0.0f;
        fd.restitution = 1.0f;
        wall.body->CreateFixture(&fd);
        wall.width = w;
        wall.height = h;
    }
}

void PhysicsWorld::removeCustomWall(int index) {
    if (index < 0 || index >= customWalls.size()) return;
    world.DestroyBody(customWalls[index].body);
    customWalls.erase(customWalls.begin() + index);
}

std::vector<CustomWall>& PhysicsWorld::getCustomWalls() {
    return customWalls;
}

b2Body* PhysicsWorld::getWinZoneBody() const { return winZoneBody; }

void PhysicsWorld::createWinZone() {
    b2BodyDef bodyDef;
    bodyDef.type = b2_staticBody;
    winZonePos[0] = worldWidthMeters / 1.0f;
    winZonePos[1] = worldHeightMeters * 0.8f;
    bodyDef.position.Set(winZonePos[0], winZonePos[1]);
    winZoneBody = world.CreateBody(&bodyDef);
    b2PolygonShape box;
    box.SetAsBox(winZoneSize[0] / 2.0f, winZoneSize[1] / 2.0f);
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &box;
    fixtureDef.isSensor = true;
    winZoneBody->CreateFixture(&fixtureDef);
    contactListener.winZoneBody = winZoneBody;
}

void PhysicsWorld::updateWinZone(float x, float y, float w, float h) {
    if (!winZoneBody) return;
    winZoneBody->SetTransform(b2Vec2(x, y), 0.0f);
    winZoneBody->DestroyFixture(winZoneBody->GetFixtureList());
    b2PolygonShape box;
    box.SetAsBox(w / 2.0f, h / 2.0f);
    b2FixtureDef fd;
    fd.shape = &box;
    fd.isSensor = true;
    winZoneBody->CreateFixture(&fd);
    winZonePos[0] = x; winZonePos[1] = y;
    winZoneSize[0] = w; winZoneSize[1] = h;
}

void PhysicsWorld::updateRacerSize(float newSize) {
    currentRacerSize = newSize;
    for (b2Body* b : dynamicBodies) {
        b->DestroyFixture(b->GetFixtureList());
        b2PolygonShape dynamicBox;
        dynamicBox.SetAsBox(newSize / 2.0f, newSize / 2.0f);
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &dynamicBox;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = currentFriction;
        fixtureDef.restitution = currentRestitution;
        b->CreateFixture(&fixtureDef);
        b->SetAwake(true);
    }
}

void PhysicsWorld::updateRestitution(float newRest) {
    currentRestitution = newRest;
    for (b2Body* b : dynamicBodies)
        for (b2Fixture* f = b->GetFixtureList(); f; f = f->GetNext())
            f->SetRestitution(newRest);
}

void PhysicsWorld::updateFriction(float newFriction) {
    currentFriction = newFriction;
    for (b2Body* b : dynamicBodies) {
        for (b2Fixture* f = b->GetFixtureList(); f; f = f->GetNext()) {
            f->SetFriction(newFriction);
        }
    }
}

void PhysicsWorld::updateFixedRotation(bool fixed) {
    currentFixedRotation = fixed;
    for (b2Body* b : dynamicBodies) {
        b->SetFixedRotation(fixed);
        b->SetAwake(true);
    }
}

const std::vector<b2Body*>& PhysicsWorld::getDynamicBodies() const { return dynamicBodies; }

void PhysicsWorld::resetRacers() {
    int i = 0;
    for (b2Body* b : dynamicBodies) {
        float x = (worldWidthMeters / 5.0f) * (i + 1);
        float y = worldHeightMeters / 2.0f;
        b->SetTransform(b2Vec2(x, y), 0.0f);
        b->SetLinearVelocity(b2Vec2(targetSpeed, targetSpeed));
        b->SetAngularVelocity(0.0f);
        b->SetAwake(true);
        i++;
    }
    gameOver = false;
    winnerIndex = -1;
    isPaused = true;
}

void PhysicsWorld::createWalls(float widthPixels, float heightPixels) {
    float width = worldWidthMeters;
    float height = worldHeightMeters;
    b2BodyDef wallBodyDef;
    wallBodyDef.type = b2_staticBody;
    b2Body* wall;
    b2PolygonShape wallShape;
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &wallShape;
    fixtureDef.friction = 0.0f;
    fixtureDef.restitution = 1.0f;

    auto makeWall = [&](float x, float y, float hx, float hy) {
        wallBodyDef.position.Set(x, y);
        wall = world.CreateBody(&wallBodyDef);
        wallShape.SetAsBox(hx, hy);
        wall->CreateFixture(&fixtureDef);
    };
    makeWall(width/2.0f, height, width/2.0f, 0.5f); 
    makeWall(width/2.0f, 0.0f, width/2.0f, 0.5f);   
    makeWall(0.0f, height/2.0f, 0.5f, height/2.0f); 
    makeWall(width, height/2.0f, 0.5f, height/2.0f);
}

void PhysicsWorld::createRacers() {
    float size = currentRacerSize;
    b2PolygonShape dynamicBox;
    dynamicBox.SetAsBox(size / 2.0f, size / 2.0f);
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &dynamicBox;
    fixtureDef.density = 1.0f;
    fixtureDef.friction = currentFriction;
    fixtureDef.restitution = currentRestitution;

    for (int i = 0; i < 4; ++i) {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.bullet = true;
        bodyDef.fixedRotation = currentFixedRotation;
        float x = (worldWidthMeters / 5.0f) * (i + 1);
        float y = worldHeightMeters / 2.0f;
        bodyDef.position.Set(x, y);
        b2Body* body = world.CreateBody(&bodyDef);
        body->CreateFixture(&fixtureDef);
        body->SetLinearVelocity(b2Vec2(targetSpeed, targetSpeed));
        dynamicBodies.push_back(body);
    }
}