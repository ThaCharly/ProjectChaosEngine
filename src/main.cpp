#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <box2d/box2d.h>
#include <imgui.h>
#include <imgui-SFML.h>
#include <iostream>
#include <filesystem>
#include <string>

#include "Physics/PhysicsWorld.hpp"
#include "Recorder/Recorder.hpp"

namespace fs = std::filesystem;

int main()
{
    const unsigned int WIDTH = 720;
    const unsigned int HEIGHT = 720;
    const unsigned int FPS = 60;
    const std::string VIDEO_DIRECTORY = "../output/video.mp4";

    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "ChaosEngine - Dual Deck");
    window.setFramerateLimit(FPS);

    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Fallo al inicializar ImGui-SFML" << std::endl;
        return -1;
    }

    PhysicsWorld physics(WIDTH, HEIGHT);
    physics.isPaused = true; 

    fs::path videoPath(VIDEO_DIRECTORY);
    fs::path outputDir = videoPath.parent_path();
    if (!fs::exists(outputDir)) fs::create_directories(outputDir);

    Recorder recorder(WIDTH, HEIGHT, FPS, VIDEO_DIRECTORY);
    recorder.isRecording = false; 

    const float timeStep = 1.0f / 60.0f;
    int32 velIter = 8;
    int32 posIter = 3;

    sf::Clock clock;
    sf::Clock deltaClock;
    float accumulator = 0.0f;

    const char* racerNames[] = { "Cyan", "Magenta", "Green", "Yellow" };
    ImVec4 guiColors[] = {
        ImVec4(0, 1, 1, 1),   
        ImVec4(1, 0, 1, 1),   
        ImVec4(0, 1, 0, 1),   
        ImVec4(1, 1, 0, 1)    
    };
    sf::Color racerColors[] = {
        sf::Color::Cyan, sf::Color::Magenta, sf::Color::Green, sf::Color::Yellow
    };

    // Buffer para el nombre del archivo de mapa
    static char mapFilename[128] = "level_01.txt";

    while (window.isOpen()) {

        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // ============================================================
        //                VENTANA 1: DIRECTOR DECK
        // ============================================================
        
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 700), ImGuiCond_FirstUseEver); // Un poco más alta
        
        ImGui::Begin("Director Control", nullptr);

        ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "CAMERA & ACTION");
        if (recorder.isRecording) {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
            if (ImGui::Button("STOP REC", ImVec2(-1, 40))) recorder.isRecording = false;
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.33f, 0.6f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.33f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.33f, 0.8f, 0.8f));
            if (ImGui::Button("START REC", ImVec2(-1, 40))) recorder.isRecording = true;
            ImGui::PopStyleColor(3);
        }

        ImGui::Spacing();
        if (physics.isPaused) {
            if (ImGui::Button("RESUME PHYS", ImVec2(-1, 30))) physics.isPaused = false;
        } else {
            if (ImGui::Button("PAUSE PHYS", ImVec2(-1, 30))) physics.isPaused = true;
        }
        if (ImGui::Button("RESET RACE", ImVec2(-1, 20))) physics.resetRacers();

        ImGui::Separator();

        // --- SISTEMA DE MAPAS ---
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1), "MAP SYSTEM");
        ImGui::InputText("Filename", mapFilename, IM_ARRAYSIZE(mapFilename));
        
        if (ImGui::Button("SAVE MAP", ImVec2(100, 30))) {
            physics.saveMap(mapFilename);
        }
        ImGui::SameLine();
        if (ImGui::Button("LOAD MAP", ImVec2(100, 30))) {
            physics.loadMap(mapFilename);
        }

        ImGui::Separator();

        // --- WALL BUILDER ---
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "WALL BUILDER");
        
        if (ImGui::Button("ADD WALL", ImVec2(-1, 30))) {
            physics.addCustomWall(12.0f, 20.0f, 10.0f, 1.0f);
        }

        ImGui::Spacing();
        ImGui::Text("Custom Walls List:");
        
        const auto& walls = physics.getCustomWalls();
        int wallToDelete = -1;

        for (int i = 0; i < walls.size(); ++i) {
            ImGui::PushID(i);
            std::string label = "Wall " + std::to_string(i);
            if (ImGui::CollapsingHeader(label.c_str())) {
                CustomWall w = walls[i];
                float pos[2] = { w.body->GetPosition().x, w.body->GetPosition().y };
                float size[2] = { w.width, w.height };
                bool changed = false;

                changed |= ImGui::DragFloat2("Position", pos, 0.1f);
                changed |= ImGui::DragFloat2("Size", size, 0.1f, 0.5f, 30.0f);

                if (changed) {
                    physics.updateCustomWall(i, pos[0], pos[1], size[0], size[1]);
                }

                if (ImGui::Button("DELETE WALL", ImVec2(-1, 20))) {
                    wallToDelete = i;
                }
            }
            ImGui::PopID();
        }

        if (wallToDelete != -1) physics.removeCustomWall(wallToDelete);

        ImGui::Separator();

        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "WIN ZONE CONFIG");
        bool updateZone = false;
        updateZone |= ImGui::DragFloat2("Posicion (m)", physics.winZonePos, 0.1f);
        updateZone |= ImGui::DragFloat2("Tamaño (m)", physics.winZoneSize, 0.1f, 0.1f, 30.0f);
        if (updateZone) {
            physics.updateWinZone(physics.winZonePos[0], physics.winZonePos[1], 
                                  physics.winZoneSize[0], physics.winZoneSize[1]);
        }

        ImGui::Separator();

        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "GLOBAL PHYSICS");
        ImGui::Checkbox("Enable Gravity", &physics.enableGravity);
        ImGui::Checkbox("Enable Glitches (Chaos)", &physics.enableChaos);
        if (physics.enableChaos) {
            ImGui::SliderFloat("Chaos %", &physics.chaosChance, 0.0f, 0.5f);
            ImGui::SliderFloat("Chaos Boost", &physics.chaosBoost, 1.0f, 3.0f);
        }
        
        ImGui::End(); 


        // ============================================================
        //                VENTANA 2: RACER INSPECTOR
        // ============================================================
        
        ImGui::SetNextWindowPos(ImVec2(370, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340, 600), ImGuiCond_FirstUseEver);

        ImGui::Begin("Racer Inspector", nullptr);

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1), "FLEET SETTINGS");
        
        float size = physics.currentRacerSize;
        if (ImGui::DragFloat("Size (m)", &size, 0.05f, 0.1f, 10.0f, "%.2f")) {
            physics.updateRacerSize(size);
        }

        float rest = physics.currentRestitution;
        if (ImGui::DragFloat("Bounce", &rest, 0.01f, 0.0f, 2.0f, "%.2f")) {
            physics.updateRestitution(rest);
        }

        bool fixedRot = physics.currentFixedRotation;
        if (ImGui::Checkbox("Fixed Rotation", &fixedRot)) {
            physics.updateFixedRotation(fixedRot);
        }

        ImGui::Checkbox("Enforce Speed", &physics.enforceSpeed);
        ImGui::DragFloat("Target Vel", &physics.targetSpeed, 0.5f, 0.0f, 100.0f, "%.1f m/s");

        ImGui::Separator();

        ImGui::Text("INDIVIDUAL CONTROLS");
        
        const auto& bodies = physics.getDynamicBodies();
        
        for (size_t i = 0; i < bodies.size(); ++i) {
            b2Body* b = bodies[i];
            
            ImGui::PushStyleColor(ImGuiCol_Header, guiColors[i % 4]);
            ImGui::PushID((int)i);

            std::string headerName;
            if (i < 4) {
                headerName = std::string(racerNames[i]);
            } else {
                headerName = "Racer " + std::to_string(i);
            }
            
            if (ImGui::CollapsingHeader(headerName.c_str())) {
                ImGui::PopStyleColor(); 
                
                b2Vec2 pos = b->GetPosition();
                float p[2] = { pos.x, pos.y };
                if (ImGui::DragFloat2("Pos", p, 0.1f)) {
                    b->SetTransform(b2Vec2(p[0], p[1]), b->GetAngle());
                    b->SetAwake(true);
                }

                b2Vec2 vel = b->GetLinearVelocity();
                float v[2] = { vel.x, vel.y };
                if (ImGui::DragFloat2("Vel", v, 0.1f)) {
                    b->SetLinearVelocity(b2Vec2(v[0], v[1]));
                    b->SetAwake(true);
                }

                float angleDeg = b->GetAngle() * 180.0f / 3.14159f;
                ImGui::Text("Angle: %.1f deg", angleDeg);
                if (ImGui::Button("Zero Rot")) {
                    b->SetTransform(b->GetPosition(), 0.0f);
                    b->SetAngularVelocity(0.0f);
                }

            } else {
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
        }

        ImGui::End(); 


        // --- UPDATE FÍSICA ---
        sf::Time dt = clock.restart();
        if (!physics.isPaused) {
            accumulator += dt.asSeconds();
            while (accumulator >= timeStep) {
                physics.step(timeStep, velIter, posIter);
                accumulator -= timeStep;
            }
        } else {
            accumulator = 0.0f;
        }

        if (physics.gameOver) {
            std::cout << ">>> WINNER: " << physics.winnerIndex << std::endl;
            window.close();
        }

        // --- RENDER ---
        window.clear(sf::Color(18, 18, 18)); 

        // 1. DIBUJAR PAREDES CUSTOM
        const auto& customWalls = physics.getCustomWalls();
        for (const auto& wall : customWalls) {
            b2Vec2 pos = wall.body->GetPosition();
            sf::RectangleShape r;
            float wPx = wall.width * PhysicsWorld::SCALE;
            float hPx = wall.height * PhysicsWorld::SCALE;
            
            r.setSize(sf::Vector2f(wPx, hPx));
            r.setOrigin(wPx / 2.0f, hPx / 2.0f);
            r.setPosition(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
            
            r.setFillColor(sf::Color(200, 200, 200)); 
            r.setOutlineColor(sf::Color::White);
            r.setOutlineThickness(1.0f);
            
            window.draw(r);
        }

        // 2. Win Zone
        b2Body* zone = physics.getWinZoneBody();
        if (zone) {
            b2Vec2 pos = zone->GetPosition();
            sf::RectangleShape zoneRect;
            float w = physics.winZoneSize[0] * PhysicsWorld::SCALE;
            float h = physics.winZoneSize[1] * PhysicsWorld::SCALE;
            zoneRect.setSize(sf::Vector2f(w, h));
            zoneRect.setOrigin(w/2.0f, h/2.0f);
            zoneRect.setPosition(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
            zoneRect.setFillColor(sf::Color(255, 215, 0, 80)); 
            zoneRect.setOutlineColor(sf::Color::Yellow);
            zoneRect.setOutlineThickness(2.0f);
            window.draw(zoneRect);
        }

        // 3. Racers
        for (size_t i = 0; i < bodies.size(); ++i) {
            b2Body* body = bodies[i];
            b2Vec2 pos = body->GetPosition();
            float angle = body->GetAngle();
            float drawSize = physics.currentRacerSize * PhysicsWorld::SCALE;

            sf::RectangleShape rect;
            rect.setSize(sf::Vector2f(drawSize, drawSize));
            rect.setOrigin(drawSize / 2.0f, drawSize / 2.0f);
            rect.setPosition(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
            rect.setRotation(angle * 180.0f / 3.14159f);

            if (i < 4) rect.setFillColor(racerColors[i]);
            else rect.setFillColor(sf::Color::White);

            window.draw(rect);
        }

        recorder.addFrame(window);
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}