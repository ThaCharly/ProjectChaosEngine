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
            // ACÁ ESTÁ LA MAGIA: Agregamos el chequeo (speed > 1.0f)
            // Si la velocidad es muy baja, es porque acaba de chocar. 
            // NO normalices vectores enanos, dejá que la física respire un frame.
            else if ((speed < targetSpeed - 0.5f || speed > targetSpeed + 0.5f) && speed > 1.0f) { 
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

    for (const auto& w : customWalls) {
        b2Vec2 pos = w.body->GetPosition();
        // Guardamos TODOS los atributos en una sola línea larga
        file << "WALL " 
             << pos.x << " " << pos.y << " " 
             << w.width << " " << w.height << " " 
             << w.soundID << " "
             << w.colorIndex << " "      // <--- Color
             << w.isExpandable << " "    // <--- Expandible
             << w.expansionDelay << " "
             << w.expansionSpeed << " "
             << w.expansionAxis << " "
             << w.stopOnContact << " "
             << w.stopTargetIdx << " "
             << w.maxSize << "\n";
    }

    // ... (Guardado de Racers igual que antes) ...
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

// 5. LOAD MAP INTELIGENTE (Compatible con versiones viejas y nuevas)
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
            // Datos básicos
            ss >> x >> y >> w >> h >> sid;
            
            addCustomWall(x, y, w, h, sid);
            
            // Obtenemos la referencia a la pared recién creada (la última)
            CustomWall& newWall = customWalls.back();

            // Intentamos leer los datos extendidos (si existen)
            int cIdx;
            if (ss >> cIdx) updateWallColor(customWalls.size() - 1, cIdx);
            
            // Propiedades de Expansión
            bool isExp;
            if (ss >> isExp) {
                newWall.isExpandable = isExp;
                ss >> newWall.expansionDelay 
                   >> newWall.expansionSpeed 
                   >> newWall.expansionAxis 
                   >> newWall.stopOnContact 
                   >> newWall.stopTargetIdx 
                   >> newWall.maxSize;
            }
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
         }}
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

sf::Color getNeonColor(int index) {
    static const std::vector<sf::Color> palette = {
        sf::Color(0, 255, 255),    // Cyan
        sf::Color(255, 0, 255),    // Magenta
        sf::Color(57, 255, 20),    // Toxic Lime
        sf::Color(255, 165, 0),    // Electric Orange
        sf::Color(147, 0, 255),    // Plasma Purple
        sf::Color(255, 0, 60),      // Hot Red
        sf::Color(255, 215, 0),    // 6: Gold (NUEVO)
        sf::Color(0, 100, 255),    // 7: Deep Blue (NUEVO)
        sf::Color(255, 105, 180)   // 8: Hot Pink (NUEVO)
    };
    return palette[index % palette.size()];
}

const std::vector<sf::Color>& PhysicsWorld::getPalette() {
    static const std::vector<sf::Color> palette = {
        sf::Color(0, 255, 255),    // 0: Cyan (Tron)
        sf::Color(255, 0, 255),    // 1: Magenta (Synthwave)
        sf::Color(57, 255, 20),    // 2: Toxic Lime
        sf::Color(255, 165, 0),    // 3: Electric Orange
        sf::Color(147, 0, 255),    // 4: Plasma Purple
        sf::Color(255, 0, 60),     // 5: Hot Red
        sf::Color(255, 215, 0),    // 6: Gold
        sf::Color(0, 100, 255),    // 7: Deep Blue
        sf::Color(255, 105, 180)   // 8: Hot Pink
    };
    return palette;
}

void PhysicsWorld::addCustomWall(float x, float y, float w, float h, int soundID) {
b2BodyDef bd; bd.type = b2_staticBody; bd.position.Set(x, y);
    b2Body* body = world.CreateBody(&bd);
    b2PolygonShape box; box.SetAsBox(w / 2.0f, h / 2.0f);
    b2FixtureDef fd; fd.shape = &box; fd.friction = 0.0f; fd.restitution = 1.0f;
    body->CreateFixture(&fd);

    CustomWall newWall;
    newWall.body = body;
    newWall.width = w;
    newWall.height = h;
    newWall.soundID = soundID;

    // Asignación de Color Inicial
    // Si soundID > 0, tratamos de machear el color, sino aleatorio o por posición
    int colorIdx = (soundID > 0) ? (soundID - 1) : ((int)(x + y) % getPalette().size());
    
    // Forzamos que esté en rango
    if (colorIdx < 0) colorIdx = 0;
    colorIdx = colorIdx % getPalette().size();

    newWall.colorIndex = colorIdx; // Guardamos el índice
    
    // Calculamos los colores derivados
    const auto& pal = getPalette();
    sf::Color neon = pal[colorIdx];
    newWall.baseFillColor = sf::Color(neon.r / 5, neon.g / 5, neon.b / 5, 240);
    newWall.neonColor = neon;
    newWall.flashColor = sf::Color(
        std::min(255, neon.r + 100),
        std::min(255, neon.g + 100),
        std::min(255, neon.b + 100)
    );

    customWalls.push_back(newWall);
}

void PhysicsWorld::updateWallColor(int index, int newColorIndex) {
    if (index < 0 || index >= customWalls.size()) return;
    
    CustomWall& w = customWalls[index];
    const auto& pal = getPalette();
    
    // Safety check
    if (newColorIndex < 0) newColorIndex = 0;
    newColorIndex = newColorIndex % pal.size();

    w.colorIndex = newColorIndex;
    sf::Color neon = pal[newColorIndex];
    
    w.baseFillColor = sf::Color(neon.r / 5, neon.g / 5, neon.b / 5, 240);
    w.neonColor = neon;
    w.flashColor = sf::Color(
        std::min(255, neon.r + 100),
        std::min(255, neon.g + 100),
        std::min(255, neon.b + 100)
    );
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

void PhysicsWorld::updateWallExpansion(float dt) {
    if (isPaused) return;

    for (size_t i = 0; i < customWalls.size(); ++i) {
        CustomWall& wall = customWalls[i];

        if (!wall.isExpandable) continue;

        wall.timeAlive += dt;
        if (wall.timeAlive < wall.expansionDelay) continue;

        float growth = wall.expansionSpeed * dt;
        float newWidth = wall.width;
        float newHeight = wall.height;
        bool sizeChanged = false;

        // Calcular crecimiento potencial
        if (wall.expansionAxis == 0 || wall.expansionAxis == 2) { newWidth += growth; sizeChanged = true; }
        if (wall.expansionAxis == 1 || wall.expansionAxis == 2) { newHeight += growth; sizeChanged = true; }

        if (!sizeChanged) continue;

        // --- CHECK 1: MAX SIZE ---
        if (wall.maxSize > 0.0f) {
            float checkDim = (wall.expansionAxis == 0) ? newWidth : newHeight;
            if (wall.expansionAxis == 2) checkDim = std::max(newWidth, newHeight);

            if (checkDim >= wall.maxSize) {
                wall.isExpandable = false;
                if (wall.expansionAxis == 0 || wall.expansionAxis == 2) newWidth = wall.maxSize;
                if (wall.expansionAxis == 1 || wall.expansionAxis == 2) newHeight = wall.maxSize;
            }
        }

        // --- CHECK 2: STOP ON SPECIFIC CONTACT ---
        if (wall.stopOnContact) {
            b2Vec2 myPos = wall.body->GetPosition();
            
            for (size_t j = 0; j < customWalls.size(); ++j) {
                if (i == j) continue; 
                if (wall.stopTargetIdx != -1 && (int)j != wall.stopTargetIdx) continue;

                const CustomWall& other = customWalls[j];
                b2Vec2 otherPos = other.body->GetPosition();

                float dx = std::abs(myPos.x - otherPos.x);
                float dy = std::abs(myPos.y - otherPos.y);

                float sumHalfWidths = (newWidth / 2.0f) + (other.width / 2.0f);
                float sumHalfHeights = (newHeight / 2.0f) + (other.height / 2.0f);

                if (dx < sumHalfWidths - 0.01f && dy < sumHalfHeights - 0.01f) {
                    wall.isExpandable = false; 
                    std::cout << "Wall " << i << " stopped by target Wall " << j << std::endl;
                    newWidth = wall.width;
                    newHeight = wall.height;
                    sizeChanged = false; 
                    break; 
                }
            }
        }

        // Si la pared paró de crecer, no calculamos muertes ni actualizamos fixtures
        if (!sizeChanged || (newWidth == wall.width && newHeight == wall.height)) continue;

        // >>> ZONA DE CRUSH: DETECTAR APLASTAMIENTO DE RACERS <<<
        b2Vec2 wallPos = wall.body->GetPosition();
        float wallMinX = wallPos.x - newWidth / 2.0f;
        float wallMaxX = wallPos.x + newWidth / 2.0f;
        float wallMinY = wallPos.y - newHeight / 2.0f;
        float wallMaxY = wallPos.y + newHeight / 2.0f;

        float margin = currentRacerSize * 0.1f; 

        for (size_t r = 0; r < dynamicBodies.size(); ++r) {
            if (!racerStatus[r].isAlive) continue;

            b2Body* racerBody = dynamicBodies[r];
            b2Vec2 racerPos = racerBody->GetPosition();

            bool isInsideX = (racerPos.x > wallMinX + margin) && (racerPos.x < wallMaxX - margin);
            bool isInsideY = (racerPos.y > wallMinY + margin) && (racerPos.y < wallMaxY - margin);

            if (isInsideX && isInsideY) {
                std::cout << ">>> RACER " << r << " CRUSHED by Wall " << i << " <<<" << std::endl;
                
                // 1. Marcar como muerto
                racerStatus[r].isAlive = false;
                racerStatus[r].deathPos = racerPos;

                // 2. DESACTIVAR DEL MUNDO (FIX BUG)
                racerBody->SetEnabled(false); // <--- CORREGIDO: SetEnabled
            }
        }
        // >>> FIN ZONA DE CRUSH <<<

        // Actualizar física de la pared
        wall.width = newWidth;
        wall.height = newHeight;
        wall.body->DestroyFixture(wall.body->GetFixtureList());
        b2PolygonShape box;
        box.SetAsBox(wall.width / 2.0f, wall.height / 2.0f);
        b2FixtureDef fd;
        fd.shape = &box;
        fd.friction = 0.0f;
        fd.restitution = 1.0f;
        wall.body->CreateFixture(&fd);
    }
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
void PhysicsWorld::resetRacers() { 
    // --- REVIVIR A TODOS ---
    for(auto& status : racerStatus) {
        status.isAlive = true;
    }

    int i = 0; 
    for(auto b : dynamicBodies){ 
        float x = (worldWidthMeters/5.0f)*(i+1); 
        float y = worldHeightMeters/2.0f; 
        
        b->SetEnabled(true); // <--- CORREGIDO: Usamos SetEnabled en lugar de SetActive
        
        b->SetTransform(b2Vec2(x, y), 0); 
        b->SetLinearVelocity(b2Vec2(targetSpeed, targetSpeed)); 
        b->SetAngularVelocity(0); 
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
    float thick = 0.5f;

    // AHORA LAS PAREDES DEL BORDE TIENEN SONIDO Y COLOR
    // ID 1: Cyan (Piso)
    // ID 2: Magenta (Techo)
    // ID 3: Lime (Izq)
    // ID 4: Orange (Der)
    
    addCustomWall(width / 2.0f, height, width / 2.0f, thick, 1); // Piso
    addCustomWall(width / 2.0f, 0.0f, width / 2.0f, thick, 2);   // Techo
    addCustomWall(0.0f, height / 2.0f, thick, height / 2.0f, 3); // Izq
    addCustomWall(width, height / 2.0f, thick, height / 2.0f, 4);// Der
}
void PhysicsWorld::createRacers() { 
    float s = currentRacerSize; 
    b2PolygonShape b; 
    b.SetAsBox(s/2, s/2); 
    b2FixtureDef fd; 
    fd.shape = &b; 
    fd.density = 1; 
    fd.friction = currentFriction; 
    fd.restitution = currentRestitution; 
    
    for(int i=0; i<4; ++i){ 
        b2BodyDef bd; 
        bd.type = b2_dynamicBody; 
        bd.bullet = true; 
        bd.fixedRotation = currentFixedRotation; 
        bd.position.Set((worldWidthMeters/5.0f)*(i+1), worldHeightMeters/2.0f); 
        b2Body* bod = world.CreateBody(&bd); 
        bod->CreateFixture(&fd); 
        bod->SetLinearVelocity(b2Vec2(targetSpeed, targetSpeed)); 
        dynamicBodies.push_back(bod); 
    }
    
    // --- INICIALIZAR ESTADO DE VIDA ---
    racerStatus.clear();
    racerStatus.resize(dynamicBodies.size(), {true, {0,0}});
}