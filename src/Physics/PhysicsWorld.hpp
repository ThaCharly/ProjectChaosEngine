#pragma once

#include <box2d/box2d.h>
#include <vector>
#include <random>
#include <set>
#include <string>
#include "../Sound/SoundManager.hpp" 

struct RacerStatus {
    bool isAlive = true;
    b2Vec2 deathPos = {0, 0};
};

struct CustomWall {
    b2Body* body;
    float width;
    float height;
    float flashTimer = 0.0f;
    int soundID = 0; 

    bool isExpandable = false;
    float expansionDelay = 2.0f;   // Tiempo de gracia antes de empezar
    float expansionSpeed = 0.5f;   // Metros por segundo
    int expansionAxis = 2;         // 0 = X (Ancho), 1 = Y (Alto), 2 = Ambos
    float timeAlive = 0.0f;        // Timer interno

    bool stopOnContact = false;    // ¿Frenar si toca otra pared?
    int stopTargetIdx = -1;
    float maxSize = 0.0f;
};

class ChaosContactListener : public b2ContactListener {
public:
    std::set<b2Body*> bodiesToCheck;
    std::set<b2Body*> wallsHit;
    b2Body* winZoneBody = nullptr;
    b2Body* winnerBody = nullptr;
    
    SoundManager* soundManager = nullptr;
    float worldWidth = 10.0f; 

    void BeginContact(b2Contact* contact) override; 
};

class PhysicsWorld {
public:
    PhysicsWorld(float widthPixels, float heightPixels, SoundManager* soundMgr);

    const std::vector<RacerStatus>& getRacerStatus() const { return racerStatus; }

    void step(float timeStep, int velocityIterations, int positionIterations);
    void updateWallVisuals(float dt);

    const std::vector<b2Body*>& getDynamicBodies() const;
    b2Body* getWinZoneBody() const;
    void resetRacers();

    void saveMap(const std::string& filename);
    void loadMap(const std::string& filename);
    void clearCustomWalls(); 

    void addCustomWall(float x, float y, float w, float h, int soundID = 0);
    void updateCustomWall(int index, float x, float y, float w, float h, int soundID);
    
    void removeCustomWall(int index);
    std::vector<CustomWall>& getCustomWalls(); 

    static constexpr float SCALE = 30.0f;

    void updateRacerSize(float newSize);
    void updateRestitution(float newRest);
    void updateFixedRotation(bool fixed);
    void updateFriction(float newFriction);
    void updateWinZone(float x, float y, float w, float h);
    
    float currentRacerSize = 1.0f;
    float currentRestitution = 1.0f;
    float currentFriction = 0.0f;
    bool currentFixedRotation = true;

    float targetSpeed = 15.0f;
    bool enforceSpeed = true;
    bool enableGravity = false;
    bool isPaused = false;

    bool enableChaos = false;
    float chaosChance = 0.05f;
    float chaosBoost = 1.5f;

    bool gameOver = false;
    int winnerIndex = -1;

    float winZonePos[2] = {0.0f, 0.0f};
    float winZoneSize[2] = {2.0f, 2.0f};

    void updateWallExpansion(float dt);

private:
    void createWalls(float widthPixels, float heightPixels);
    void createRacers();
    void createWinZone();
    float randomFloat(float min, float max);

    b2World world;
    std::vector<b2Body*> dynamicBodies;
    std::vector<CustomWall> customWalls;
    b2Body* winZoneBody = nullptr;
    std::vector<RacerStatus> racerStatus;

    ChaosContactListener contactListener;
    std::mt19937 rng;
    SoundManager* soundManager; 

    float worldWidthMeters;
    float worldHeightMeters;
};