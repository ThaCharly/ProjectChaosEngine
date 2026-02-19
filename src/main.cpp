#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <box2d/box2d.h>
#include <imgui.h>
#include <imgui-SFML.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <deque>

#include "Physics/PhysicsWorld.hpp"
#include "Recorder/Recorder.hpp"
#include "Sound/SoundManager.hpp" 

namespace fs = std::filesystem;

struct Trail {
    std::deque<sf::Vector2f> points;
    sf::Color color;
};

sf::Texture createGridTexture(int width, int height) {
    sf::RenderTexture rt;
    rt.create(width, height);
    rt.clear(sf::Color(10, 10, 10)); 
    sf::RectangleShape line;
    line.setFillColor(sf::Color(30, 30, 30)); 
    line.setSize(sf::Vector2f(2.0f, (float)height));
    for (int x = 0; x < width; x += 60) { 
        line.setPosition((float)x, 0.0f); rt.draw(line);
    }
    line.setSize(sf::Vector2f((float)width, 2.0f));
    for (int y = 0; y < height; y += 60) {
        line.setPosition(0.0f, (float)y); rt.draw(line);
    }
    rt.display();
    return rt.getTexture();
}

sf::Color lerpColor(const sf::Color& a, const sf::Color& b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return sf::Color(
        (sf::Uint8)(a.r + (b.r - a.r) * t),
        (sf::Uint8)(a.g + (b.g - a.g) * t),
        (sf::Uint8)(a.b + (b.b - a.b) * t),
        (sf::Uint8)(a.a + (b.a - a.a) * t)
    );
}

void SoundManager::sendToRecorder(const sf::Int16* samples, std::size_t count, float vol) {
    if (recorder) {
        recorder->addAudioEvent(samples, count, vol);
    }
}

int main()
{
    // --- RESOLUCIÓN OFF-SCREEN (VIDEO FINAL) ---
    const unsigned int RENDER_WIDTH = 1080;
    const unsigned int RENDER_HEIGHT = 1080;

    // --- RESOLUCIÓN DE LA VENTANA (TU MONITOR) ---
    const unsigned int WINDOW_WIDTH = 720;
    const unsigned int WINDOW_HEIGHT = 720;
    
    const unsigned int FPS = 60;
    const std::string VIDEO_DIRECTORY = "../output/video.mp4";

    // Ventana bloqueada para que no la redimensionen y rompan el aspect ratio
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "ChaosEngine - Neon Lab", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(FPS);

    if (!ImGui::SFML::Init(window)) return -1;

    // --- FRAMEBUFFER GIGANTE ---
    sf::RenderTexture gameBuffer;
    if (!gameBuffer.create(RENDER_WIDTH, RENDER_HEIGHT)) {
        std::cerr << "Pah, te quedaste sin VRAM bo. Falló el RenderTexture." << std::endl;
        return -1;
    }

    SoundManager soundManager; 

    // OJO ACÁ: La física labura con el tamaño de renderizado gigante
    PhysicsWorld physics(RENDER_WIDTH, RENDER_HEIGHT, &soundManager);
    physics.isPaused = true; 
    const auto& bodies = physics.getDynamicBodies();

    fs::path videoPath(VIDEO_DIRECTORY);
    fs::path outputDir = videoPath.parent_path();
    if (!fs::exists(outputDir)) fs::create_directories(outputDir);

    // El grabador también labura con el tamaño gigante
    Recorder recorder(RENDER_WIDTH, RENDER_HEIGHT, FPS, VIDEO_DIRECTORY);
    recorder.isRecording = false; 

    soundManager.setRecorder(&recorder);

    const float timeStep = 1.0f / 60.0f;
    int32 velIter = 8;
    int32 posIter = 3;

    sf::Clock clock;
    sf::Clock deltaClock;
    float accumulator = 0.0f;
    float globalTime = 0.0f;

    float victoryTimer = 0.0f;       
    bool victorySequenceStarted = false; 
    const float VICTORY_DELAY = 0.5f; 

    const char* racerNames[] = { "Cyan", "Magenta", "Green", "Yellow" };
    
    sf::Color racerColors[] = {
        sf::Color(0, 255, 255),   
        sf::Color(255, 0, 255),   
        sf::Color(57, 255, 20),   
        sf::Color(255, 215, 0)    
    };

    ImVec4 guiColors[] = {
        ImVec4(0, 1, 1, 1),   
        ImVec4(1, 0, 1, 1),   
        ImVec4(0.2f, 1, 0.1f, 1),   
        ImVec4(1, 0.8f, 0, 1)    
    };

    // La grilla tiene que cubrir el mapa de 1080p entero
    sf::Texture gridTexture = createGridTexture(RENDER_WIDTH, RENDER_HEIGHT);
    sf::Sprite background(gridTexture);

    std::vector<Trail> trails(4);
    for(int i=0; i<4; ++i) trails[i].color = racerColors[i];

    static char mapFilename[128] = "../levels/level_01.txt";

    while (window.isOpen()) {

        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        sf::Time dt = clock.restart();
        float dtSec = dt.asSeconds();
        
        physics.updateWallVisuals(dtSec);
        globalTime += dtSec;

        if (!physics.isPaused) {
            accumulator += dtSec;
            while (accumulator >= timeStep) {
                physics.step(timeStep, velIter, posIter);
                physics.updateWallExpansion(dtSec);
                accumulator -= timeStep;
            }
        } else {
            accumulator = 0.0f;
        }

        if (!physics.isPaused) {
            for (size_t i = 0; i < bodies.size(); ++i) {
                if (i >= trails.size()) break;
                b2Vec2 pos = bodies[i]->GetPosition();
                sf::Vector2f p(pos.x * physics.SCALE, pos.y * physics.SCALE);
                trails[i].points.push_front(p);
                float speed = bodies[i]->GetLinearVelocity().Length();
                size_t maxPoints = (size_t)(speed * 3.0f) + 10; 
                if (trails[i].points.size() > maxPoints) trails[i].points.pop_back();
            }
        }

        if (physics.gameOver) {
            if (!victorySequenceStarted) {
                std::cout << ">>> VICTORY DETECTED: Racer " << physics.winnerIndex << ". Finishing recording..." << std::endl;
                victorySequenceStarted = true;
            }
            victoryTimer += dtSec;
            if (victoryTimer >= VICTORY_DELAY) {
                std::cout << ">>> CLOSING SIMULATION." << std::endl;
                recorder.stop(); 
                window.close();
            }
        }

        // --- IMGUI DIRECTOR ---
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 750), ImGuiCond_FirstUseEver);
        ImGui::Begin("Director Control", nullptr);

        static char songFile[128] = "song.txt";
        ImGui::InputText("Song File", songFile, 128);
        if (ImGui::Button("LOAD SONG")) {
            physics.loadSong(songFile);
        }

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
        if (ImGui::Button("RESET RACE", ImVec2(-1, 20))) {
            physics.resetRacers();
            for(auto& t : trails) t.points.clear();
            victoryTimer = 0.0f; 
            victorySequenceStarted = false; 
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1), "MAP SYSTEM");
        ImGui::InputText("Filename", mapFilename, IM_ARRAYSIZE(mapFilename));
        if (ImGui::Button("SAVE MAP", ImVec2(100, 30))) physics.saveMap(mapFilename);
        ImGui::SameLine();
        if (ImGui::Button("LOAD MAP", ImVec2(100, 30))) {
            physics.loadMap(mapFilename);
            for(auto& t : trails) t.points.clear();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "WALL BUILDER");
        
        static int nextWallSoundID = 1;
        const char* noteNames[] = { "Mute", "Do (C4)", "Re (D4)", "Mi (E4)", "Fa (F4)", "Sol (G4)", "La (A4)", "Si (B4)", "Do (C5)" };
        if (nextWallSoundID >= 0 && nextWallSoundID <= 8) {
            ImGui::Text("Next Sound: %s", noteNames[nextWallSoundID]);
        }
        ImGui::SliderInt("Next Sound ID", &nextWallSoundID, 0, 8);

        if (ImGui::Button("ADD WALL", ImVec2(-1, 30))) {
            physics.addCustomWall(12.0f, 20.0f, 10.0f, 1.0f, nextWallSoundID);
        }

        ImGui::Spacing();
        ImGui::Text("Custom Walls List:");
        const auto& walls = physics.getCustomWalls();
        int wallToDelete = -1;

        for (int i = 0; i < walls.size(); ++i) {
            ImGui::PushID(i);
            std::string label = "Wall " + std::to_string(i);
            if (walls[i].soundID > 0) label += " [Note " + std::to_string(walls[i].soundID) + "]";
            if (walls[i].isExpandable) label += " [EXP]"; 

            if (ImGui::CollapsingHeader(label.c_str())) {
                CustomWall& w = physics.getCustomWalls()[i]; 
                
                float pos[2] = { w.body->GetPosition().x, w.body->GetPosition().y };
                float size[2] = { w.width, w.height };
                int snd = w.soundID;
                
                bool changed = false;
                changed |= ImGui::DragFloat2("Position", pos, 0.1f);
                changed |= ImGui::DragFloat2("Size", size, 0.1f, 0.5f, 30.0f);
                changed |= ImGui::SliderInt("Sound ID", &snd, 0, 8);
                
                if (changed) physics.updateCustomWall(i, pos[0], pos[1], size[0], size[1], snd, w.shapeType, w.rotation);

                ImGui::Separator();
                ImGui::Text("Appearance");

                if (ImGui::Button("ADD SPIKE (1m)", ImVec2(-1, 30))) {
                    physics.addCustomWall(12.0f, 12.0f, 1.0f, 1.0f, 0, 1, 0.0f);
                }

                int sType = w.shapeType;
                float rotDeg = w.rotation * 180.0f / 3.14159f; 

                ImGui::Separator();
                ImGui::Text("Geometry");
                bool geoChanged = false;

                if (ImGui::RadioButton("Box", sType == 0)) { sType = 0; geoChanged = true; } ImGui::SameLine();
                if (ImGui::RadioButton("Spike", sType == 1)) { sType = 1; geoChanged = true; w.isDeadly = true; }

                if (ImGui::SliderFloat("Rotation", &rotDeg, 0.0f, 360.0f, "%.0f deg")) geoChanged = true;
                
                if (ImGui::Button("0°")) { rotDeg = 0.0f; geoChanged = true; } ImGui::SameLine();
                if (ImGui::Button("90°")) { rotDeg = 90.0f; geoChanged = true; } ImGui::SameLine();
                if (ImGui::Button("180°")) { rotDeg = 180.0f; geoChanged = true; } ImGui::SameLine();
                if (ImGui::Button("270°")) { rotDeg = 270.0f; geoChanged = true; }

                if (geoChanged || changed) { 
                    float rotRad = rotDeg * 3.14159f / 180.0f;
                    physics.updateCustomWall(i, pos[0], pos[1], size[0], size[1], snd, sType, rotRad);
                }

                sf::Color c = w.neonColor;
                ImVec4 imColor = ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);

                ImGui::ColorButton("##preview", imColor, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
                ImGui::SameLine();

                int currentColorIdx = w.colorIndex;
                const char* colorNames[] = { 
                    "Cyan (Tron)", "Magenta (Synth)", "Toxic Lime", "Electric Orange", 
                    "Plasma Purple", "Hot Red", "Gold", "Deep Blue", "Hot Pink" 
                };
                
                if (ImGui::Combo("Neon Color", &currentColorIdx, colorNames, IM_ARRAYSIZE(colorNames))) {
                    physics.updateWallColor(i, currentColorIdx);
                }

                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "DANGER ZONE"); 

                if (ImGui::Checkbox("IS DEADLY (Spike)", &w.isDeadly)) {
                    if (w.isDeadly) physics.updateWallColor(i, 5); 
                }

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "EXPANSION PROPERTIES");
                
                ImGui::Checkbox("Is Expandable", &w.isExpandable);

                if (w.isExpandable) {
                    ImGui::Indent();
                    ImGui::DragFloat("Start Delay (s)", &w.expansionDelay, 0.1f, 0.0f, 60.0f);
                    ImGui::DragFloat("Growth Speed (m/s)", &w.expansionSpeed, 0.05f, 0.01f, 10.0f);
                    
                    ImGui::Text("Growth Axis:"); 
                    ImGui::SameLine();
                    ImGui::RadioButton("X", &w.expansionAxis, 0); ImGui::SameLine();
                    ImGui::RadioButton("Y", &w.expansionAxis, 1); ImGui::SameLine();
                    ImGui::RadioButton("XY", &w.expansionAxis, 2);

                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "STOP CONDITIONS");
                    
                    ImGui::Checkbox("Stop on Contact", &w.stopOnContact);
                    if (w.stopOnContact) {
                        ImGui::Indent();
                        ImGui::TextDisabled("(?) -1 = Any Wall");
                        ImGui::InputInt("Target Wall ID", &w.stopTargetIdx);
                        ImGui::Unindent();
                    }

                    ImGui::DragFloat("Max Size Limit", &w.maxSize, 0.5f, 0.0f, 100.0f);
                    ImGui::ProgressBar(w.timeAlive / (w.expansionDelay + 0.001f), ImVec2(-1, 10), "Timer");
                    ImGui::Unindent();
                }

                if (ImGui::Button("DELETE WALL", ImVec2(-1, 20))) wallToDelete = i;
            }
            ImGui::PopID();
        }
        if (wallToDelete != -1) physics.removeCustomWall(wallToDelete);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "WIN ZONE CONFIG");
        bool updateZone = false;
        updateZone |= ImGui::DragFloat2("Pos", physics.winZonePos, 0.1f);
        updateZone |= ImGui::DragFloat2("Size", physics.winZoneSize, 0.1f);
        if (updateZone) physics.updateWinZone(physics.winZonePos[0], physics.winZonePos[1], physics.winZoneSize[0], physics.winZoneSize[1]);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "GLOBAL PHYSICS");
        ImGui::Checkbox("Gravity", &physics.enableGravity);
        ImGui::Checkbox("Chaos", &physics.enableChaos);
        if (physics.enableChaos) {
            ImGui::SliderFloat("Chaos %", &physics.chaosChance, 0.0f, 0.5f);
            ImGui::SliderFloat("Boost", &physics.chaosBoost, 1.0f, 3.0f);
        }

        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(370, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340, 600), ImGuiCond_FirstUseEver);
        ImGui::Begin("Racer Inspector", nullptr);
        
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1), "FLEET SETTINGS");
        float size = physics.currentRacerSize;
        if (ImGui::DragFloat("Size", &size, 0.05f, 0.1f, 10.0f)) physics.updateRacerSize(size);
        float rest = physics.currentRestitution;
        if (ImGui::DragFloat("Bounce", &rest, 0.01f, 0.0f, 2.0f)) physics.updateRestitution(rest);
        bool fixedRot = physics.currentFixedRotation;
        if (ImGui::Checkbox("Fixed Rotation", &fixedRot)) physics.updateFixedRotation(fixedRot);
        ImGui::Checkbox("Enforce Speed", &physics.enforceSpeed);
        ImGui::DragFloat("Target Vel", &physics.targetSpeed, 0.5f, 0.0f, 100.0f);

        ImGui::Separator();
        ImGui::Text("INDIVIDUAL CONTROLS");
        for (size_t i = 0; i < bodies.size(); ++i) {
            b2Body* b = bodies[i];
            ImGui::PushStyleColor(ImGuiCol_Header, guiColors[i % 4]);
            ImGui::PushID((int)i);
            std::string headerName = (i < 4) ? std::string(racerNames[i]) : "Racer " + std::to_string(i);
            if (ImGui::CollapsingHeader(headerName.c_str())) {
                ImGui::PopStyleColor(); 
                b2Vec2 pos = b->GetPosition(); float p[2] = { pos.x, pos.y };
                if (ImGui::DragFloat2("Pos", p, 0.1f)) { b->SetTransform(b2Vec2(p[0], p[1]), b->GetAngle()); b->SetAwake(true); }
                b2Vec2 vel = b->GetLinearVelocity(); float v[2] = { vel.x, vel.y };
                if (ImGui::DragFloat2("Vel", v, 0.1f)) { b->SetLinearVelocity(b2Vec2(v[0], v[1])); b->SetAwake(true); }
            } else {
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
        }
        ImGui::End(); 

        // ==============================================
        // --- DRAW: RENDERIZADO AL BUFFER GIGANTE ---
        // ==============================================
        gameBuffer.clear(sf::Color(10, 10, 10)); // Usamos el FBO en vez de window
        gameBuffer.draw(background);

        const auto& customWalls = physics.getCustomWalls();
        for (const auto& wall : customWalls) {
            b2Vec2 pos = wall.body->GetPosition();
            float wPx = wall.width * physics.SCALE;
            float hPx = wall.height * physics.SCALE;
            
            sf::Shape* shapeToDraw = nullptr;
            sf::RectangleShape rectShape;
            sf::ConvexShape triShape;

            if (wall.shapeType == 1) {
                triShape.setPointCount(3);
                triShape.setPoint(0, sf::Vector2f(0.0f, -hPx / 2.0f));       
                triShape.setPoint(1, sf::Vector2f(wPx / 2.0f, hPx / 2.0f));  
                triShape.setPoint(2, sf::Vector2f(-wPx / 2.0f, hPx / 2.0f)); 
                
                triShape.setPosition(pos.x * physics.SCALE, pos.y * physics.SCALE);
                triShape.setRotation(wall.body->GetAngle() * 180.0f / 3.14159f);
                
                shapeToDraw = &triShape;
            } else {
                rectShape.setSize(sf::Vector2f(wPx, hPx));
                rectShape.setOrigin(wPx / 2.0f, hPx / 2.0f);
                rectShape.setPosition(pos.x * physics.SCALE, pos.y * physics.SCALE);
                rectShape.setRotation(wall.body->GetAngle() * 180.0f / 3.14159f);
                
                shapeToDraw = &rectShape;
            }

            sf::Color currentFill = lerpColor(wall.baseFillColor, wall.flashColor, wall.flashTimer);
            sf::Color currentOutline = lerpColor(wall.neonColor, sf::Color::White, wall.flashTimer * 0.5f);

            if (wall.isDeadly) {
                float dangerPulse = (std::sin(globalTime * 10.0f) + 1.0f) * 0.5f; 
                currentFill = sf::Color(100 + (dangerPulse * 50), 0, 0, 255); 
                currentOutline = sf::Color::Red;
            }

            shapeToDraw->setFillColor(currentFill); 
            shapeToDraw->setOutlineColor(currentOutline);
            
            float thickness = 2.0f + (wall.flashTimer * 2.0f);
            shapeToDraw->setOutlineThickness(-thickness);

            gameBuffer.draw(*shapeToDraw); // DIBUJAMOS AL BUFFER

            b2Body* zone = physics.getWinZoneBody();
            if (zone) {
                b2Vec2 pos = zone->GetPosition();
                sf::RectangleShape zoneRect;
                float w = physics.winZoneSize[0] * physics.SCALE;
                float h = physics.winZoneSize[1] * physics.SCALE;
                float pulse = (std::sin(globalTime * 3.0f) + 1.0f) * 0.5f; 
                float alpha = 50.0f + pulse * 100.0f;
                zoneRect.setSize(sf::Vector2f(w, h));
                zoneRect.setOrigin(w/2.0f, h/2.0f);
                zoneRect.setPosition(pos.x * physics.SCALE, pos.y * physics.SCALE);
                zoneRect.setFillColor(sf::Color(255, 215, 0, (sf::Uint8)alpha)); 
                zoneRect.setOutlineColor(sf::Color::Yellow);
                zoneRect.setOutlineThickness(3.0f);
                gameBuffer.draw(zoneRect); // DIBUJAMOS AL BUFFER
            }
        }

        const auto& statuses = physics.getRacerStatus();
        float tombSize = 25.0f; 

        for (size_t i = 0; i < statuses.size(); ++i) {
            const auto& status = statuses[i];

            if (!status.isAlive) {
                float px = status.deathPos.x * physics.SCALE;
                float py = status.deathPos.y * physics.SCALE;

                sf::Color deathColor = (i < 4) ? racerColors[i] : sf::Color::White;

                sf::RectangleShape grave;
                grave.setSize(sf::Vector2f(tombSize, tombSize));
                grave.setOrigin(tombSize / 2.0f, tombSize / 2.0f);
                grave.setPosition(px, py);
                grave.setFillColor(sf::Color(20, 20, 20, 240)); 
                grave.setOutlineColor(deathColor);              
                grave.setOutlineThickness(2.0f);
                gameBuffer.draw(grave); // DIBUJAMOS AL BUFFER

                float crossThick = 5.0f;               
                float crossLen = tombSize * 0.8f;      

                sf::RectangleShape bar1(sf::Vector2f(crossLen, crossThick));
                sf::RectangleShape bar2(sf::Vector2f(crossLen, crossThick));

                bar1.setOrigin(crossLen / 2.0f, crossThick / 2.0f);
                bar2.setOrigin(crossLen / 2.0f, crossThick / 2.0f);

                bar1.setPosition(px, py);
                bar2.setPosition(px, py);

                bar1.setRotation(45.0f);
                bar2.setRotation(-45.0f);

                bar1.setFillColor(deathColor); 
                bar2.setFillColor(deathColor);

                gameBuffer.draw(bar1); // DIBUJAMOS AL BUFFER
                gameBuffer.draw(bar2);
            }
        }

// --- RENDERIZADO DE ESTELAS MEJORADO ---
        for (size_t i = 0; i < trails.size(); ++i) {
            const auto& pts = trails[i].points;
            if (pts.size() < 2) continue; // Necesitamos al menos 2 puntos para calcular normales

            // Dos pasadas: un halo exterior (glow) y un núcleo brillante (core)
            sf::VertexArray glowVA(sf::TriangleStrip, pts.size() * 2);
            sf::VertexArray coreVA(sf::TriangleStrip, pts.size() * 2);
            
            // 1. Escala dinámica: atamos el grosor al tamaño del cuadrado en el motor físico
            float baseWidth = physics.currentRacerSize * physics.SCALE; 

            for (size_t j = 0; j < pts.size(); ++j) {
                sf::Vector2f normal(1, 0);
                
                // 2. Cálculo de normal mejorado (evita que el último punto rompa la orientación)
                if (j < pts.size() - 1) {
                    sf::Vector2f dir = pts[j+1] - pts[j];
                    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
                    if (len > 0.001f) normal = sf::Vector2f(-dir.y/len, dir.x/len);
                } else if (j > 0) {
                    sf::Vector2f dir = pts[j] - pts[j-1];
                    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
                    if (len > 0.001f) normal = sf::Vector2f(-dir.y/len, dir.x/len);
                }

                // Porcentaje de vida del segmento (1.0 = pegado al racer, 0.0 = final de la cola)
                float lifePct = 1.0f - ((float)j / (float)pts.size());
                
                // 3. Tapering curvo: usamos raíz para que el grosor no caiga en línea recta
                float widthPct = std::pow(lifePct, 0.6f); 
                float currentWidth = baseWidth * widthPct;
                
                sf::Color baseColor = trails[i].color;
                
                // --- CAPA EXTERIOR (GLOW) ---
                sf::Color glowColor = baseColor;
                // El alpha cae cuadráticamente para que se disipe suave en los bordes
                glowColor.a = (sf::Uint8)(std::pow(lifePct, 1.5f) * 160.0f); 
                
                glowVA[j*2].position = pts[j] + normal * (currentWidth * 0.5f);
                glowVA[j*2].color = glowColor;
                glowVA[j*2+1].position = pts[j] - normal * (currentWidth * 0.5f);
                glowVA[j*2+1].color = glowColor;

                // --- CAPA INTERIOR (CORE) ---
                sf::Color coreColor = sf::Color::White;
                // Agregamos interpolación hacia blanco para quemar el color central
                coreColor.r = (baseColor.r + 255 * 2) / 3;
                coreColor.g = (baseColor.g + 255 * 2) / 3;
                coreColor.b = (baseColor.b + 255 * 2) / 3;
                coreColor.a = (sf::Uint8)(lifePct * 240.0f);
                
                // El núcleo luminoso ocupa solo el 35% del grosor de la estela
                float coreWidth = currentWidth * 0.35f; 
                
                coreVA[j*2].position = pts[j] + normal * (coreWidth * 0.5f);
                coreVA[j*2].color = coreColor;
                coreVA[j*2+1].position = pts[j] - normal * (coreWidth * 0.5f);
                coreVA[j*2+1].color = coreColor;
            }
            
            // Dibujamos con mezcla alfa tradicional por encima del fondo
            gameBuffer.draw(glowVA);
            gameBuffer.draw(coreVA);
        }

        const auto& currentStatuses = physics.getRacerStatus(); 

        for (size_t i = 0; i < bodies.size(); ++i) {
            if (i < currentStatuses.size() && !currentStatuses[i].isAlive) continue;
            b2Body* body = bodies[i];
            b2Vec2 pos = body->GetPosition();
            float angle = body->GetAngle();
            float drawSize = physics.currentRacerSize * physics.SCALE;
            sf::RectangleShape rect;
            rect.setSize(sf::Vector2f(drawSize, drawSize));
            rect.setOrigin(drawSize / 2.0f, drawSize / 2.0f);
            rect.setPosition(pos.x * physics.SCALE, pos.y * physics.SCALE);
            rect.setRotation(angle * 180.0f / 3.14159f);
            if (i < 4) rect.setOutlineColor(racerColors[i]);
            else rect.setOutlineColor(sf::Color::White);
            rect.setFillColor(sf::Color::White);
            rect.setOutlineThickness(-3.0f);
            gameBuffer.draw(rect); // DIBUJAMOS AL BUFFER
        }

        // Sellamos el frame gigante de 1080p
        gameBuffer.display();

        // Le mandamos el frame crudo de VRAM al grabador
        recorder.addFrame(gameBuffer.getTexture());

        // ==============================================
        // --- DRAW: PASAR A LA VENTANA CHICA (PANTALLA) ---
        // ==============================================
        window.clear();

        // Extraemos la textura completa y la convertimos a un Sprite
        sf::Sprite renderSprite(gameBuffer.getTexture());
        
        // Escalar de RENDER (1080) a WINDOW (720)
        float scaleX = (float)WINDOW_WIDTH / RENDER_WIDTH;
        float scaleY = (float)WINDOW_HEIGHT / RENDER_HEIGHT;
        renderSprite.setScale(scaleX, scaleY);
        
        // Dibujamos el "juego" comprimido en nuestra pantalla
        window.draw(renderSprite);

        // ImGui se dibuja ÚNICAMENTE acá, en la ventana del OS, por lo que NO SALE en el video!
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}