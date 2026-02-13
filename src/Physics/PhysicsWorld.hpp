#pragma once

#include <box2d/box2d.h>
#include <vector>

class PhysicsWorld {
public:
    PhysicsWorld(float widthPixels, float heightPixels);

    void step(float timeStep, int velocityIterations, int positionIterations);

    const std::vector<b2Body*>& getDynamicBodies() const;

    static constexpr float SCALE = 30.0f;

private:
    void createWalls(float widthPixels, float heightPixels);
    void createRacers();

    b2World world;
    std::vector<b2Body*> dynamicBodies;

    float worldWidthMeters;
    float worldHeightMeters;
};
