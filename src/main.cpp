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
#include "Sound/SoundManager.hpp" // <--- Incluir

namespace fs = std::filesystem;

// ... (Struct Trail y createGridTexture quedan igual) ...
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
    const unsigned int WIDTH = 720;
    const unsigned int HEIGHT = 720;
    const unsigned int FPS = 60;
    const std::string VIDEO_DIRECTORY = "../output/video.mp4";

    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "ChaosEngine - Neon Lab");
    window.setFramerateLimit(FPS);

    if (!ImGui::SFML::Init(window)) return -1;

    // 1. INICIALIZAR AUDIO (Sintetizador Automático)
    SoundManager soundManager; 
    // Ya no cargamos nada, el constructor generó los tonos.

    // 2. FÍSICA
    PhysicsWorld physics(WIDTH, HEIGHT, &soundManager);
    physics.isPaused = true; 
    const auto& bodies = physics.getDynamicBodies();

    fs::path videoPath(VIDEO_DIRECTORY);
    fs::path outputDir = videoPath.parent_path();
    if (!fs::exists(outputDir)) fs::create_directories(outputDir);

    Recorder recorder(WIDTH, HEIGHT, FPS, VIDEO_DIRECTORY);
    recorder.isRecording = false; 

    soundManager.setRecorder(&recorder);

    const float timeStep = 1.0f / 60.0f;
    int32 velIter = 8;
    int32 posIter = 3;

    sf::Clock clock;
    sf::Clock deltaClock;
    float accumulator = 0.0f;
    float globalTime = 0.0f;

    float victoryTimer = 0.0f;       // Acumulador de tiempo post-victoria
    bool victorySequenceStarted = false; // Flag para imprimir una sola vez
    const float VICTORY_DELAY = 0.5f; // Medio segundo de gracia

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

    sf::Texture gridTexture = createGridTexture(WIDTH, HEIGHT);
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
                sf::Vector2f p(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
                trails[i].points.push_front(p);
                float speed = bodies[i]->GetLinearVelocity().Length();
                size_t maxPoints = (size_t)(speed * 3.0f) + 10; 
                if (trails[i].points.size() > maxPoints) trails[i].points.pop_back();
            }
        }

// --- LÓGICA DE VICTORIA CON DELAY ---
        if (physics.gameOver) {
            // 1. Iniciar secuencia si es la primera vez
            if (!victorySequenceStarted) {
                std::cout << ">>> VICTORY DETECTED: Racer " << physics.winnerIndex << ". Finishing recording..." << std::endl;
                victorySequenceStarted = true;
                // Opcional: Podrías frenar el tiempo (physics.isPaused = true) 
                // pero si querés ver como entra suavemente, dejalo correr.
            }

            // 2. Acumular tiempo
            victoryTimer += dtSec;

            // 3. Chequear si pasó el tiempo
            if (victoryTimer >= VICTORY_DELAY) {
                std::cout << ">>> CLOSING SIMULATION." << std::endl;
                recorder.stop(); // <--- FUSIÓN CRÍTICA AQUÍ
                window.close();
            }
        }

        // --- IMGUI DIRECTOR ---
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 750), ImGuiCond_FirstUseEver);
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
        if (ImGui::Button("RESET RACE", ImVec2(-1, 20))) {
            physics.resetRacers();
            for(auto& t : trails) t.points.clear();
            victoryTimer = 0.0f; // <--- RESETEAR
            victorySequenceStarted = false; // <--- RESETEAR
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
        
        // Selector de Sonido para la próxima pared
        static int nextWallSoundID = 1;
        // Mapeo simple de nombres de notas para ayudar
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

// ... Dentro del loop de ImGui, sección Custom Walls List ...
for (int i = 0; i < walls.size(); ++i) {
            ImGui::PushID(i);
            std::string label = "Wall " + std::to_string(i);
            if (walls[i].soundID > 0) label += " [Note " + std::to_string(walls[i].soundID) + "]";
            // Indicador visual si es expandible
            if (walls[i].isExpandable) label += " [EXP]"; 

            if (ImGui::CollapsingHeader(label.c_str())) {
                CustomWall& w = physics.getCustomWalls()[i]; // Referencia para editar
                
                float pos[2] = { w.body->GetPosition().x, w.body->GetPosition().y };
                float size[2] = { w.width, w.height };
                int snd = w.soundID;
                
                // --- PHYSICS PROPS ---
                bool changed = false;
                changed |= ImGui::DragFloat2("Position", pos, 0.1f);
                changed |= ImGui::DragFloat2("Size", size, 0.1f, 0.5f, 30.0f);
                changed |= ImGui::SliderInt("Sound ID", &snd, 0, 8);
                
                if (changed) physics.updateCustomWall(i, pos[0], pos[1], size[0], size[1], snd);

                // --- APPEARANCE (NUEVO & CORREGIDO) ---
                ImGui::Separator();
                ImGui::Text("Appearance");

                // 1. Conversión de sf::Color (0-255) a ImVec4 (0.0-1.0) para ImGui
                sf::Color c = w.neonColor;
                ImVec4 imColor = ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);

                // 2. Botón de Preview (No hace nada al clickear, solo muestra el color)
                ImGui::ColorButton("##preview", imColor, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
                ImGui::SameLine();

                // 3. Selector de Tema
                int currentColorIdx = w.colorIndex;
                const char* colorNames[] = { 
                    "Cyan (Tron)", "Magenta (Synth)", "Toxic Lime", "Electric Orange", 
                    "Plasma Purple", "Hot Red", "Gold", "Deep Blue", "Hot Pink" 
                };
                
                // Asegurate que el array coincida con el tamaño de tu paleta en PhysicsWorld
                if (ImGui::Combo("Neon Color", &currentColorIdx, colorNames, IM_ARRAYSIZE(colorNames))) {
                    physics.updateWallColor(i, currentColorIdx);
                }

                // --- EXPANSION PROPERTIES ---
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

        // --- IMGUI RACER INSPECTOR ---
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

        // --- DRAW ---
        window.clear();
        window.draw(background);

const auto& customWalls = physics.getCustomWalls();
        for (const auto& wall : customWalls) {
            b2Vec2 pos = wall.body->GetPosition();
            float wPx = wall.width * PhysicsWorld::SCALE;
            float hPx = wall.height * PhysicsWorld::SCALE;
            
            sf::RectangleShape r;
            r.setSize(sf::Vector2f(wPx, hPx));
            r.setOrigin(wPx / 2.0f, hPx / 2.0f);
            r.setPosition(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
            
            // Calculamos el color actual basado en el Flash Timer
            // Si flashTimer es 1.0 (golpe reciente), es flashColor. Si es 0.0, es baseFillColor.
            sf::Color currentFill = lerpColor(wall.baseFillColor, wall.flashColor, wall.flashTimer);
            
            // El borde también "palpita" un poco con el golpe
            sf::Color currentOutline = lerpColor(wall.neonColor, sf::Color::White, wall.flashTimer * 0.5f);

            r.setFillColor(currentFill); 
            r.setOutlineColor(currentOutline);
            
            // Grosor del borde: Hacemos que se engrose un poquito al golpear
            float thickness = 2.0f + (wall.flashTimer * 2.0f);
            r.setOutlineThickness(-thickness); // Negativo para que crezca hacia adentro y no cambie el tamaño físico visual

            window.draw(r);
        }

        b2Body* zone = physics.getWinZoneBody();
        if (zone) {
            b2Vec2 pos = zone->GetPosition();
            sf::RectangleShape zoneRect;
            float w = physics.winZoneSize[0] * PhysicsWorld::SCALE;
            float h = physics.winZoneSize[1] * PhysicsWorld::SCALE;
            float pulse = (std::sin(globalTime * 3.0f) + 1.0f) * 0.5f; 
            float alpha = 50.0f + pulse * 100.0f;
            zoneRect.setSize(sf::Vector2f(w, h));
            zoneRect.setOrigin(w/2.0f, h/2.0f);
            zoneRect.setPosition(pos.x * PhysicsWorld::SCALE, pos.y * PhysicsWorld::SCALE);
            zoneRect.setFillColor(sf::Color(255, 215, 0, (sf::Uint8)alpha)); 
            zoneRect.setOutlineColor(sf::Color::Yellow);
            zoneRect.setOutlineThickness(3.0f);
            window.draw(zoneRect);
        }

// --- RENDERIZADO DE TUMBAS (CEMENTERIO) ---
        // Usamos SFML directo para asegurar que salga en el video
        const auto& statuses = physics.getRacerStatus();
        float tombSize = 25.0f; 

        for (const auto& status : statuses) {
            if (!status.isAlive) {
                float px = status.deathPos.x * PhysicsWorld::SCALE;
                float py = status.deathPos.y * PhysicsWorld::SCALE;

                // 1. Base de la tumba (Rectángulo SFML)
                sf::RectangleShape grave;
                grave.setSize(sf::Vector2f(tombSize, tombSize));
                grave.setOrigin(tombSize / 2.0f, tombSize / 2.0f);
                grave.setPosition(px, py);
                grave.setFillColor(sf::Color(40, 40, 40, 200));
                grave.setOutlineColor(sf::Color::Black);
                grave.setOutlineThickness(2.0f);
                window.draw(grave);

                // 2. Cruz Roja (VertexArray de líneas)
                sf::VertexArray cross(sf::Lines, 4);
                float half = tombSize / 2.0f;
                float m = 5.0f; // Margen

                // Diagonal 1 (Izquierda-Arriba a Derecha-Abajo)
                cross[0].position = sf::Vector2f(px - half + m, py - half + m);
                cross[1].position = sf::Vector2f(px + half - m, py + half - m);
                
                // Diagonal 2 (Derecha-Arriba a Izquierda-Abajo)
                cross[2].position = sf::Vector2f(px + half - m, py - half + m);
                cross[3].position = sf::Vector2f(px - half + m, py + half - m);

                // Color rojo sangre para todas las puntas
                for(int k=0; k<4; ++k) cross[k].color = sf::Color(220, 0, 0);

                window.draw(cross);
            }
        }

        for (size_t i = 0; i < trails.size(); ++i) {
            const auto& pts = trails[i].points;
            if (pts.empty()) continue;
            sf::VertexArray va(sf::TriangleStrip, pts.size() * 2);
            float width = 8.0f;
            for (size_t j = 0; j < pts.size(); ++j) {
                sf::Vector2f normal(1, 0);
                if (j + 1 < pts.size()) {
                    sf::Vector2f dir = pts[j+1] - pts[j];
                    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
                    if(len > 0.001f) normal = sf::Vector2f(-dir.y/len, dir.x/len);
                }
                float alphaPct = 1.0f - (float)j / (float)pts.size();
                sf::Color c = trails[i].color;
                c.a = (sf::Uint8)(alphaPct * 150.0f);
                va[j*2].position = pts[j] + normal * (width * alphaPct * 0.5f);
                va[j*2].color = c;
                va[j*2+1].position = pts[j] - normal * (width * alphaPct * 0.5f);
                va[j*2+1].color = c;
            }
            window.draw(va);
        }

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
            if (i < 4) rect.setOutlineColor(racerColors[i]);
            else rect.setOutlineColor(sf::Color::White);
            rect.setFillColor(sf::Color::White);
            rect.setOutlineThickness(-3.0f);
            window.draw(rect);
        }

        recorder.addFrame(window);
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}