#include "PhysicsWorld.hpp"
#include <cmath>
#include <iostream>
#include <fstream> 
#include <sstream> 

void ChaosContactListener::BeginContact(b2Contact* contact) {
    b2Fixture* fa = contact->GetFixtureA();
    b2Fixture* fb = contact->GetFixtureB();
    b2Body* bodyA = fa->GetBody();
    b2Body* bodyB = fb->GetBody();

    if (winZoneBody) {
        if (bodyA == winZoneBody && bodyB->GetType() == b2_dynamicBody) winnerBody = bodyB;
        else if (bodyB == winZoneBody && bodyA->GetType() == b2_dynamicBody) winnerBody = bodyA;
    }

    if (fa->GetBody()->GetType() == b2_dynamicBody && fb->GetBody()->GetType() == b2_dynamicBody) {
        bodiesToCheck.insert(fa->GetBody());
        bodiesToCheck.insert(fb->GetBody());
    }

    // Lógica de Paredes
    b2Body* dynamicBody = nullptr;
    b2Body* staticBody = nullptr;

    if (bodyA->GetType() == b2_dynamicBody && bodyB->GetType() == b2_staticBody) {
        dynamicBody = bodyA; staticBody = bodyB;
    } else if (bodyB->GetType() == b2_dynamicBody && bodyA->GetType() == b2_staticBody) {
        dynamicBody = bodyB; staticBody = bodyA;
    }

    if (dynamicBody && staticBody) {
        bodiesToCheck.insert(dynamicBody);
        wallsHit.insert(staticBody); // <--- Marcamos la pared golpeada
    }
}

PhysicsWorld::PhysicsWorld(float widthPixels, float heightPixels, SoundManager* soundMgr)
    : world(b2Vec2(0.0f, 0.0f)) 
{
    rng.seed(77);
    this->soundManager = soundMgr;

    contactListener.soundManager = soundMgr;
    contactListener.worldWidth = widthPixels / SCALE;
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
   // contactListener.wallsHit.clear(); // Limpiamos para el nuevo frame
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

// --- ACTUALIZACIÓN VISUAL Y AUDIO (SIMPLIFICADO) ---
// --- ACTUALIZACIÓN VISUAL Y AUDIO ---
void PhysicsWorld::updateWallVisuals(float dt) {
    
    // Recorremos las paredes que fueron golpeadas (acumuladas en los steps)
    for (b2Body* body : contactListener.wallsHit) {
        for (auto& wall : customWalls) {
            if (wall.body == body) {
                // 1. FLASH VISUAL
                wall.flashTimer = 1.0f;

                // 2. SONIDO
                if (soundManager && wall.soundID > 0) {
                    soundManager->playSound(
                        wall.soundID, 
                        wall.body->GetPosition().x, 
                        worldWidthMeters
                    );
                }
            }
        }
    }

    // Fade out
    for (auto& wall : customWalls) {
        if (wall.flashTimer > 0.0f) {
            wall.flashTimer -= dt * 3.0f;
            if (wall.flashTimer < 0.0f) wall.flashTimer = 0.0f;
        }
    }

    // --- ¡AQUÍ ES DONDE SE LIMPIA! ---
    // Limpiamos la lista DESPUÉS de haber procesado los choques
    contactListener.wallsHit.clear();
}

// --- GUARDADO/CARGA (Sin cambios, ya soporta soundID) ---
void PhysicsWorld::saveMap(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return;

    file << "CONFIG " << targetSpeed << " " << currentRacerSize << " " 
         << currentRestitution << " " << enableChaos << "\n";
    file << "WINZONE " << winZonePos[0] << " " << winZonePos[1] << " " 
         << winZoneSize[0] << " " << winZoneSize[1] << "\n";

    for (const auto& wall : customWalls) {
        b2Vec2 pos = wall.body->GetPosition();
        file << "WALL " << pos.x << " " << pos.y << " " 
             << wall.width << " " << wall.height << " " << wall.soundID << "\n";
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
    file.close();
}

void PhysicsWorld::loadMap(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;

    clearCustomWalls();
    resetRacers(); 

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "CONFIG") {
            float spd, size, rest; bool chaos;
            ss >> spd >> size >> rest >> chaos;
            targetSpeed = spd; enableChaos = chaos;
            updateRacerSize(size); updateRestitution(rest);
        }
        else if (type == "WINZONE") {
            float x, y, w, h;
            ss >> x >> y >> w >> h;
            updateWinZone(x, y, w, h);
        } 
        else if (type == "WALL") {
            float x, y, w, h;
            int sid = 0;
            ss >> x >> y >> w >> h;
            if (!(ss >> sid)) sid = 0; 
            addCustomWall(x, y, w, h, sid);
        }
        else if (type == "RACER") {
            int id; float x, y, vx, vy, a, av;
            ss >> id >> x >> y >> vx >> vy >> a >> av;
            if (id >= 0 && id < dynamicBodies.size()) {
                b2Body* b = dynamicBodies[id];
                b->SetTransform(b2Vec2(x, y), a);
                b->SetLinearVelocity(b2Vec2(vx, vy));
                b->SetAngularVelocity(av);
                b->SetAwake(true);
            }
        }
    }
    isPaused = true;
    file.close();
}

void PhysicsWorld::clearCustomWalls() {
    for (const auto& wall : customWalls) {
        world.DestroyBody(wall.body);
    }
    customWalls.clear();
}

void PhysicsWorld::addCustomWall(float x, float y, float w, float h, int soundID) {
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
    customWalls.push_back({body, w, h, 0.0f, soundID}); 
}

void PhysicsWorld::updateCustomWall(int index, float x, float y, float w, float h, int soundID) {
    if (index < 0 || index >= customWalls.size()) return;
    CustomWall& wall = customWalls[index];
    wall.soundID = soundID;
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

std::vector<CustomWall>& PhysicsWorld::getCustomWalls() { return customWalls; }
b2Body* PhysicsWorld::getWinZoneBody() const { return winZoneBody; }
void PhysicsWorld::createWinZone() { b2BodyDef bd; bd.type=b2_staticBody; winZonePos[0]=worldWidthMeters/1.0f; winZonePos[1]=worldHeightMeters*0.8f; bd.position.Set(winZonePos[0], winZonePos[1]); winZoneBody=world.CreateBody(&bd); b2PolygonShape b; b.SetAsBox(winZoneSize[0]/2, winZoneSize[1]/2); b2FixtureDef fd; fd.shape=&b; fd.isSensor=true; winZoneBody->CreateFixture(&fd); contactListener.winZoneBody=winZoneBody; }
void PhysicsWorld::updateWinZone(float x, float y, float w, float h) { if(!winZoneBody)return; winZoneBody->SetTransform(b2Vec2(x,y),0); winZoneBody->DestroyFixture(winZoneBody->GetFixtureList()); b2PolygonShape b; b.SetAsBox(w/2,h/2); b2FixtureDef fd; fd.shape=&b; fd.isSensor=true; winZoneBody->CreateFixture(&fd); winZonePos[0]=x;winZonePos[1]=y;winZoneSize[0]=w;winZoneSize[1]=h; }
void PhysicsWorld::updateRacerSize(float newSize) { currentRacerSize=newSize; for(b2Body* b:dynamicBodies){ b->DestroyFixture(b->GetFixtureList()); b2PolygonShape s; s.SetAsBox(newSize/2,newSize/2); b2FixtureDef fd; fd.shape=&s; fd.density=1; fd.friction=currentFriction; fd.restitution=currentRestitution; b->CreateFixture(&fd); b->SetAwake(true); } }
void PhysicsWorld::updateRestitution(float newRest) { currentRestitution=newRest; for(auto b:dynamicBodies) for(auto f=b->GetFixtureList();f;f=f->GetNext()) f->SetRestitution(newRest); }
void PhysicsWorld::updateFriction(float newFriction) { currentFriction=newFriction; for(auto b:dynamicBodies) for(auto f=b->GetFixtureList();f;f=f->GetNext()) f->SetFriction(newFriction); }
void PhysicsWorld::updateFixedRotation(bool fixed) { currentFixedRotation=fixed; for(auto b:dynamicBodies) { b->SetFixedRotation(fixed); b->SetAwake(true); } }
const std::vector<b2Body*>& PhysicsWorld::getDynamicBodies() const { return dynamicBodies; }
void PhysicsWorld::resetRacers() { int i=0; for(auto b:dynamicBodies){ float x=(worldWidthMeters/5.0f)*(i+1); float y=worldHeightMeters/2.0f; b->SetTransform(b2Vec2(x,y),0); b->SetLinearVelocity(b2Vec2(targetSpeed,targetSpeed)); b->SetAngularVelocity(0); b->SetAwake(true); i++; } gameOver=false; winnerIndex=-1; isPaused=true; }
void PhysicsWorld::createWalls(float widthPixels, float heightPixels) {
    float width = worldWidthMeters;
    float height = worldHeightMeters;
    float thick = 0.5f;

    // Usamos addCustomWall para que sean "Paredes con ID"
    // ID 1: Do (Piso)
    // ID 2: Re (Techo)
    // ID 3: Mi (Izq)
    // ID 4: Fa (Der)
    
    // Abajo
    addCustomWall(width / 2.0f, height, width / 2.0f, thick, 1);
    // Arriba
    addCustomWall(width / 2.0f, 0.0f, width / 2.0f, thick, 2);
    // Izquierda
    addCustomWall(0.0f, height / 2.0f, thick, height / 2.0f, 3);
    // Derecha
    addCustomWall(width, height / 2.0f, thick, height / 2.0f, 4);
}
void PhysicsWorld::createRacers() { float s=currentRacerSize; b2PolygonShape b; b.SetAsBox(s/2,s/2); b2FixtureDef fd; fd.shape=&b; fd.density=1; fd.friction=currentFriction; fd.restitution=currentRestitution; for(int i=0;i<4;++i){ b2BodyDef bd; bd.type=b2_dynamicBody; bd.bullet=true; bd.fixedRotation=currentFixedRotation; bd.position.Set((worldWidthMeters/5.0f)*(i+1), worldHeightMeters/2.0f); b2Body* bod=world.CreateBody(&bd); bod->CreateFixture(&fd); bod->SetLinearVelocity(b2Vec2(targetSpeed,targetSpeed)); dynamicBodies.push_back(bod); } }