#pragma once

#include <box2d/box2d.h>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
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
    float expansionDelay = 2.0f;
    float expansionSpeed = 0.5f;
    int expansionAxis = 2;
    float timeAlive = 0.0f;

    bool stopOnContact = false;
    int stopTargetIdx = -1;
    float maxSize = 0.0f;
    bool isDeadly = false;
    
    bool isMoving = false;
    b2Vec2 pointA = {0.0f, 0.0f};
    b2Vec2 pointB = {0.0f, 0.0f};
    float moveSpeed = 3.0f;
    bool movingTowardsB = true;
    

    // --- NUEVO: GEOMETRÍA ---
    int shapeType = 0; // 0 = Box, 1 = Spike
    float rotation = 0.0f; // Radianes
    // ------------------------

    int colorIndex = 0; 
    sf::Color baseFillColor = sf::Color(20, 20, 25);
    sf::Color neonColor = sf::Color::White;
    sf::Color flashColor = sf::Color(255, 255, 255);
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

    // --- ACTUALIZADO: Aceptan shapeType y rotation ---
    void addCustomWall(float x, float y, float w, float h, int soundID = 0, int shapeType = 0, float rotation = 0.0f);
    void updateCustomWall(int index, float x, float y, float w, float h, int soundID, int shapeType, float rotation);
    // --------------------------------------------------
    
    void removeCustomWall(int index);
    std::vector<CustomWall>& getCustomWalls(); 

    static const std::vector<sf::Color>& getPalette();
    void updateWallColor(int wallIndex, int newColorIndex);

    float SCALE = 30.0f;

    void updateRacerSize(float newSize);
    void updateRestitution(float newRest);
    void updateFixedRotation(bool fixed);
    void updateFriction(float newFriction);
    void updateWinZone(float x, float y, float w, float h);
    
    float currentRacerSize = 1.0f;
    float currentRestitution = 1.0f;
    float currentFriction = 0.0f;
    bool currentFixedRotation = true;

    float targetSpeed = 8.0f;
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
    void updateMovingPlatforms(float dt);

    void loadSong(const std::string& filename);
    bool isSongLoaded = false;

private:
    std::vector<int> songNotes;
    int currentNoteIndex = 0;

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