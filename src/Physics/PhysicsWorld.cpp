#include "PhysicsWorld.hpp"

PhysicsWorld::PhysicsWorld(float widthPixels, float heightPixels)
    : world(b2Vec2(0.0f, 0.0f)) // gravedad cero
{
    worldWidthMeters = widthPixels / SCALE;
    worldHeightMeters = heightPixels / SCALE;

    createWalls(widthPixels, heightPixels);
    createRacers();
}

// En PhysicsWorld::step
void PhysicsWorld::step(float timeStep, int velIter, int posIter) {
    world.SetGravity(enableGravity ? b2Vec2(0.0f, 9.8f) : b2Vec2(0.0f, 0.0f));
    world.Step(timeStep, velIter, posIter);

if (enforceSpeed) {
        for (b2Body* b : dynamicBodies) {
            b2Vec2 vel = b->GetLinearVelocity();
            float speed = vel.Length();
            
            // Usamos targetSpeed variable
            if (speed < targetSpeed - 0.5f || speed > targetSpeed + 0.5f) { 
                vel.Normalize();
                vel *= targetSpeed; 
                b->SetLinearVelocity(vel);
            }
        }
    }
}

const std::vector<b2Body*>& PhysicsWorld::getDynamicBodies() const {
    return dynamicBodies;
}

void PhysicsWorld::resetRacers() {
    // Reinicia posiciones a una formación básica
    int i = 0;
    for (b2Body* b : dynamicBodies) {
        float x = (worldWidthMeters / 5.0f) * (i + 1);
        float y = worldHeightMeters / 2.0f;
        
        b->SetTransform(b2Vec2(x, y), 0.0f);
        b->SetLinearVelocity(b2Vec2(targetSpeed, targetSpeed)); // Reinicia velocidad
        b->SetAngularVelocity(0.0f);
        b->SetAwake(true);
        i++;
    }
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

    // Bottom
    wallBodyDef.position.Set(width / 2.0f, height);
    wall = world.CreateBody(&wallBodyDef);
    wallShape.SetAsBox(width / 2.0f, 0.5f);
    wall->CreateFixture(&fixtureDef);

    // Top
    wallBodyDef.position.Set(width / 2.0f, 0.0f);
    wall = world.CreateBody(&wallBodyDef);
    wallShape.SetAsBox(width / 2.0f, 0.5f);
    wall->CreateFixture(&fixtureDef);

    // Left
    wallBodyDef.position.Set(0.0f, height / 2.0f);
    wall = world.CreateBody(&wallBodyDef);
    wallShape.SetAsBox(0.5f, height / 2.0f);
    wall->CreateFixture(&fixtureDef);

    // Right
    wallBodyDef.position.Set(width, height / 2.0f);
    wall = world.CreateBody(&wallBodyDef);
    wallShape.SetAsBox(0.5f, height / 2.0f);
    wall->CreateFixture(&fixtureDef);
}

void PhysicsWorld::createRacers() {

    float size = 1.0f; // 1 metro

    b2PolygonShape dynamicBox;
    dynamicBox.SetAsBox(size / 2.0f, size / 2.0f);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &dynamicBox;
    fixtureDef.density = 1.0f;
    fixtureDef.friction = 0.0f;
    fixtureDef.restitution = 1.02f; // pequeño boost anti-pérdida, cambiar a 1.0f para física más realista

    for (int i = 0; i < 4; ++i) {

        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.bullet = true;
        bodyDef.fixedRotation = true;

        float x = 3.0f + i * 3.0f;
        float y = 3.0f + i * 2.0f;

        bodyDef.position.Set(x, y);

        b2Body* body = world.CreateBody(&bodyDef);
        body->CreateFixture(&fixtureDef);

        body->SetLinearVelocity(b2Vec2(targetSpeed, targetSpeed)); // Velocidad inicial positiva

        dynamicBodies.push_back(body);
    }
}
