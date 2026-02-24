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

const char* brightnessFrag = R"(
    uniform sampler2D source;
    uniform float threshold;
    void main() {
        vec4 color = texture2D(source, gl_TexCoord[0].xy);
        
        // NORMALIZACIÓN DE NEÓN:
        // Usamos el canal más alto del pixel en lugar de la luminancia del ojo humano.
        // Así un Azul puro (0,0,1) y un Verde puro (0,1,0) tienen un brillo = 1.0.
        float maxBrightness = max(color.r, max(color.g, color.b));
        
        if (maxBrightness > threshold) {
            gl_FragColor = color;
        } else {
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        }
    }
)";

// Desenfoque Gaussiano Separable (Muchísimo más rápido que el bidimensional)
const char* blurFrag = R"(
    uniform sampler2D source;
    uniform vec2 dir; // Dirección del desenfoque (horizontal o vertical)
    void main() {
        vec2 uv = gl_TexCoord[0].xy;
        vec4 color = vec4(0.0);
        vec2 off1 = vec2(1.3846153846) * dir;
        vec2 off2 = vec2(3.2307692308) * dir;
        
        color += texture2D(source, uv) * 0.2270270270;
        color += texture2D(source, uv + off1) * 0.3162162162;
        color += texture2D(source, uv - off1) * 0.3162162162;
        color += texture2D(source, uv + off2) * 0.0702702703;
        color += texture2D(source, uv - off2) * 0.0702702703;
        
        gl_FragColor = color;
    }
)";

const char* blendFrag = R"(
    uniform sampler2D baseTexture;
    uniform sampler2D bloomTexture;
    uniform float multiplier;
    void main() {
        vec4 base = texture2D(baseTexture, gl_TexCoord[0].xy);
        vec4 bloom = texture2D(bloomTexture, gl_TexCoord[0].xy);
        // Fusión Aditiva: Luz + Luz
        gl_FragColor = base + (bloom * multiplier);
    }
)";

// 1. Arriba de todo, cambiá la grilla para que responda a la resolución
sf::Texture createGridTexture(int width, int height) {
    sf::RenderTexture rt;
    rt.create(width, height);
    rt.clear(sf::Color(30, 30, 30)); 
    sf::RectangleShape line;
    line.setFillColor(sf::Color(10, 10, 10)); 
    
    // Si subimos resolución, subimos el grosor y el espacio entre líneas
    float lineThick = (width / 1080.0f) * 2.0f;
    int stepSize = width / 18; // 18 cortes en total, da igual si es 1080 o 2160
    
    line.setSize(sf::Vector2f(lineThick, (float)height));
    for (int x = 0; x < width; x += stepSize) { 
        line.setPosition((float)x, 0.0f); rt.draw(line);
    }
    line.setSize(sf::Vector2f((float)width, lineThick));
    for (int y = 0; y < height; y += stepSize) {
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

// --- MÁQUINA DE ESTADOS PARA LA UI ESTILO UNITY ---
enum class EntityType { None, Global, WinZone, Racers, Wall };

int main()
{
    const unsigned int RENDER_WIDTH = 2160;
    const unsigned int RENDER_HEIGHT = 2160;
    const float DISPLAY_SIZE = 900.0f;
    const unsigned int FPS = 60;
    const std::string VIDEO_DIRECTORY = "../output/video.mp4";

    sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    sf::RenderWindow window(desktopMode, "ChaosEngine - Neon Lab", sf::Style::Fullscreen);
    window.setFramerateLimit(FPS);

    if (!ImGui::SFML::Init(window)) return -1;

    // --- ESTILO IMGUI TIPO MOTOR GRÁFICO ---
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.95f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

    sf::RenderTexture gameBuffer;
    if (!gameBuffer.create(RENDER_WIDTH, RENDER_HEIGHT)) {
        std::cerr << "Pah, te quedaste sin VRAM bo. Falló el RenderTexture." << std::endl;
        return -1;
    }

    // --- SETUP DE BLOOM ---
    if (!sf::Shader::isAvailable()) {
        std::cerr << "Pah, tu GPU no banca shaders. Olvidate del neón." << std::endl;
        return -1;
    }

    sf::Shader brightnessShader, blurShader, blendShader;
    brightnessShader.loadFromMemory(brightnessFrag, sf::Shader::Fragment);
    blurShader.loadFromMemory(blurFrag, sf::Shader::Fragment);
    blendShader.loadFromMemory(blendFrag, sf::Shader::Fragment);

    // Achicamos a la mitad para el cálculo del brillo. ¡Magia negra para optimizar!
    unsigned int BLOOM_W = RENDER_WIDTH / 2;
    unsigned int BLOOM_H = RENDER_HEIGHT / 2;
    
    sf::RenderTexture brightnessBuffer, blurBuffer1, blurBuffer2, finalBuffer;
    brightnessBuffer.create(BLOOM_W, BLOOM_H);
    blurBuffer1.create(BLOOM_W, BLOOM_H);
    blurBuffer2.create(BLOOM_W, BLOOM_H);
    finalBuffer.create(RENDER_WIDTH, RENDER_HEIGHT); // Este es el 4K final que grabamos

    // Variables de control para ImGui
    bool enableBloom = true;
    float bloomThreshold = 0.4f; // A partir de qué brillo empieza a generar glow
    float bloomMultiplier = 1.5f; // Intensidad del neón
    int blurIterations = 3; // Cuántas pasadas de blur (más = glow más grande)

    SoundManager soundManager; 
    PhysicsWorld physics(RENDER_WIDTH, RENDER_HEIGHT, &soundManager);
    physics.isPaused = true; 
    const auto& bodies = physics.getDynamicBodies();

    fs::path videoPath(VIDEO_DIRECTORY);
    fs::path outputDir = videoPath.parent_path();
    if (!fs::exists(outputDir)) fs::create_directories(outputDir);

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

    sf::Texture gridTexture = createGridTexture(RENDER_WIDTH, RENDER_HEIGHT);
    sf::Sprite background(gridTexture);

    std::vector<Trail> trails(4);
    for(int i=0; i<4; ++i) trails[i].color = racerColors[i];

    static char mapFilename[128] = "../levels/level_01.txt";
    static char songFile[128] = "song.txt";

    // Variables de estado de la Interfaz
    EntityType selectedType = EntityType::None;
    int selectedIndex = -1;

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

        // --- FIX DE SINCRONIZACIÓN PARA 4K PERFECTO ---
        if (recorder.isRecording) {
            // Le clavamos el tiempo exacto. No importa si la GPU demora,
            // para el motor y el video SIEMPRE pasaron 16.6ms por ciclo.
            dtSec = timeStep;
        }
        
        physics.updateWallVisuals(dtSec);
        physics.updateParticles(dtSec);
        globalTime += dtSec;

        if (!physics.isPaused) {
            if (recorder.isRecording) {
                // MODO GRABACIÓN: 1 Frame de Video = 1 Step de Física. (Chau acumulador)
                physics.step(timeStep, velIter, posIter);
                physics.updateWallExpansion(timeStep);
                physics.updateMovingPlatforms(timeStep);
            } else {
                // MODO TIEMPO REAL: Usamos el acumulador para compensar tironcitos normales
                accumulator += dtSec;
                while (accumulator >= timeStep) {
                    physics.step(timeStep, velIter, posIter);
                    physics.updateWallExpansion(timeStep);
                    physics.updateMovingPlatforms(timeStep);
                    accumulator -= timeStep;
                }
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

        // ==============================================
        // --- UI ESTILO UNITY/UE5 ---
        // ==============================================

        // 1. TOOLBAR (Panel Superior)
    // 1. TOOLBAR (Panel Superior, anclado arriba y ocupando el ancho del centro)
        float panelWidth = 320.0f;
        ImGui::SetNextWindowPos(ImVec2(panelWidth, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(desktopMode.width - (panelWidth * 2), 65), ImGuiCond_Always);
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

        if (recorder.isRecording) {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
            if (ImGui::Button("STOP REC", ImVec2(100, 40))) recorder.isRecording = false;
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.33f, 0.6f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.33f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.33f, 0.8f, 0.8f));
            if (ImGui::Button("START REC", ImVec2(100, 40))) recorder.isRecording = true;
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();
        ImGui::SetCursorPosY(15);
        if (physics.isPaused) {
            if (ImGui::Button("RESUME PHYS", ImVec2(100, 30))) physics.isPaused = false;
        } else {
            if (ImGui::Button("PAUSE PHYS", ImVec2(100, 30))) physics.isPaused = true;
        }

        ImGui::SameLine();
        ImGui::SetCursorPosY(15);
        if (ImGui::Button("RESET RACE", ImVec2(100, 30))) {
            physics.resetRacers();
            for(auto& t : trails) t.points.clear();
            victoryTimer = 0.0f; 
            victorySequenceStarted = false; 
        }

        ImGui::SameLine();
        ImGui::SetCursorPosY(15);
        ImGui::SetNextItemWidth(120);
        ImGui::InputText("##SongFile", songFile, 128);
        ImGui::SameLine();
        if (ImGui::Button("LOAD SONG", ImVec2(90, 30))) {
            physics.loadSong(songFile);
        }

        ImGui::End();

        // 2. HIERARCHY (Panel Izquierdo)
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth, desktopMode.height), ImGuiCond_Always);
        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1), "SCENE");
        if (ImGui::Selectable("Global Settings", selectedType == EntityType::Global)) selectedType = EntityType::Global;
        if (ImGui::Selectable("Win Zone", selectedType == EntityType::WinZone)) selectedType = EntityType::WinZone;
        if (ImGui::Selectable("Racers Fleet", selectedType == EntityType::Racers)) selectedType = EntityType::Racers;

        ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0.0f, 1, 1), "POST-PROCESSING");
            ImGui::Checkbox("Enable Neon Bloom", &enableBloom);
            if (enableBloom) {
                ImGui::Indent();
                ImGui::DragFloat("Threshold", &bloomThreshold, 0.05f, 0.0f, 1.0f);
                ImGui::DragFloat("Intensity", &bloomMultiplier, 0.05f, 0.0f, 5.0f);
                ImGui::SliderInt("Glow Spread", &blurIterations, 1, 8);
                ImGui::Unindent();
            }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "WALLS");
        ImGui::SameLine(ImGui::GetWindowWidth() - 35);
        if (ImGui::Button("+", ImVec2(25, 20))) {
            physics.addCustomWall(12.0f, 20.0f, 10.0f, 1.0f, 1);
            selectedType = EntityType::Wall;
            selectedIndex = (int)physics.getCustomWalls().size() - 1;
        }

        const auto& walls = physics.getCustomWalls();
        for (int i = 0; i < (int)walls.size(); ++i) {
            std::string label = "Wall " + std::to_string(i);
            if (walls[i].soundID > 0) label += " [♪]";
            if (walls[i].isExpandable) label += " [E]";
            if (walls[i].isMoving) label += " [M]";
            if (walls[i].shapeType == 1) label += " [Spike]";

            if (ImGui::Selectable(label.c_str(), selectedType == EntityType::Wall && selectedIndex == i)) {
                selectedType = EntityType::Wall;
                selectedIndex = i;
            }
        }
        ImGui::End();

        // 3. INSPECTOR (Panel Derecho, de arriba a abajo)
        ImGui::SetNextWindowPos(ImVec2(desktopMode.width - panelWidth, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth, desktopMode.height), ImGuiCond_Always);
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        int wallToDelete = -1;
        int wallToDuplicate = -1;

        if (selectedType == EntityType::None) {
            ImGui::TextDisabled("Select an object\nin the Hierarchy.");
        } 
        else if (selectedType == EntityType::Global) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1), "MAP SYSTEM");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##Filename", mapFilename, IM_ARRAYSIZE(mapFilename));
            if (ImGui::Button("SAVE MAP", ImVec2(-1, 30))) physics.saveMap(mapFilename);
            if (ImGui::Button("LOAD MAP", ImVec2(-1, 30))) {
                physics.loadMap(mapFilename);
                for(auto& t : trails) t.points.clear();
                selectedType = EntityType::None; // Reset selection safety
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "GLOBAL PHYSICS");
            ImGui::Checkbox("Gravity", &physics.enableGravity);
            ImGui::Checkbox("Stop on First Win", &physics.stopOnFirstWin);
            ImGui::DragFloat("Finish Delay (s)", &physics.finishDelay, 0.05f, 0.0f, 2.0f);
            
            ImGui::Checkbox("Chaos Mode", &physics.enableChaos);
            if (physics.enableChaos) {
                ImGui::Indent();
                ImGui::SliderFloat("Chaos %", &physics.chaosChance, 0.0f, 0.5f);
                ImGui::SliderFloat("Boost", &physics.chaosBoost, 1.0f, 3.0f);
                ImGui::Unindent();
            }
        }
        else if (selectedType == EntityType::WinZone) {
            ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "WIN ZONE CONFIG");
            bool updateZone = false;
            updateZone |= ImGui::DragFloat2("Pos", physics.winZonePos, 0.1f);
            updateZone |= ImGui::DragFloat2("Size", physics.winZoneSize, 0.1f);
            if (updateZone) physics.updateWinZone(physics.winZonePos[0], physics.winZonePos[1], physics.winZoneSize[0], physics.winZoneSize[1]);
        }
        else if (selectedType == EntityType::Racers) {
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
            ImGui::Text("INDIVIDUAL RACERS");
            for (size_t i = 0; i < bodies.size(); ++i) {
                b2Body* b = bodies[i];
                ImGui::PushStyleColor(ImGuiCol_Header, guiColors[i % 4]);
                ImGui::PushID((int)i);
                std::string headerName = (i < 4) ? std::string(racerNames[i]) : "Racer " + std::to_string(i);
                if (ImGui::CollapsingHeader(headerName.c_str())) {
                    b2Vec2 pos = b->GetPosition(); float p[2] = { pos.x, pos.y };
                    if (ImGui::DragFloat2("Pos", p, 0.1f)) { b->SetTransform(b2Vec2(p[0], p[1]), b->GetAngle()); b->SetAwake(true); }
                    b2Vec2 vel = b->GetLinearVelocity(); float v[2] = { vel.x, vel.y };
                    if (ImGui::DragFloat2("Vel", v, 0.1f)) { b->SetLinearVelocity(b2Vec2(v[0], v[1])); b->SetAwake(true); }
                }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }
        else if (selectedType == EntityType::Wall) {
            if (selectedIndex >= 0 && selectedIndex < physics.getCustomWalls().size()) {
                CustomWall& w = physics.getCustomWalls()[selectedIndex];
                
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "WALL %d", selectedIndex);
                
                float pos[2] = { w.body->GetPosition().x, w.body->GetPosition().y };
                float size[2] = { w.width, w.height };
                int snd = w.soundID;
                
                bool changed = false;
                changed |= ImGui::DragFloat2("Position", pos, 0.1f);
                changed |= ImGui::DragFloat2("Size", size, 0.1f, 0.5f, 30.0f);
                changed |= ImGui::SliderInt("Sound ID", &snd, 0, 8);
                
                if (changed) physics.updateCustomWall(selectedIndex, pos[0], pos[1], size[0], size[1], snd, w.shapeType, w.rotation);

                ImGui::Separator();
                ImGui::Text("Geometry");
                int sType = w.shapeType;
                float rotDeg = w.rotation * 180.0f / 3.14159f; 
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
                    physics.updateCustomWall(selectedIndex, pos[0], pos[1], size[0], size[1], snd, sType, rotRad);
                }

                ImGui::Separator();
                ImGui::Text("Appearance");
                sf::Color c = w.neonColor;
                ImVec4 imColor = ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);

                ImGui::ColorButton("##preview", imColor, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
                ImGui::SameLine();

                int currentColorIdx = w.colorIndex;
                const char* colorNames[] = { "Cyan", "Magenta", "Lime", "Orange", "Purple", "Red", "Gold", "Blue", "Pink" };
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##Color", &currentColorIdx, colorNames, IM_ARRAYSIZE(colorNames))) {
                    physics.updateWallColor(selectedIndex, currentColorIdx);
                }

                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "DANGER ZONE"); 
                if (ImGui::Checkbox("IS DEADLY (Spike)", &w.isDeadly)) {
                    if (w.isDeadly) physics.updateWallColor(selectedIndex, 5); 
                }

                ImGui::Separator();
                if (ImGui::CollapsingHeader("Expansion Properties")) {
                    ImGui::Checkbox("Is Expandable", &w.isExpandable);
                    if (w.isExpandable) {
                        ImGui::Indent();
                        ImGui::DragFloat("Start Delay", &w.expansionDelay, 0.1f, 0.0f, 60.0f);
                        ImGui::DragFloat("Speed", &w.expansionSpeed, 0.05f, 0.01f, 10.0f);
                        ImGui::RadioButton("X", &w.expansionAxis, 0); ImGui::SameLine();
                        ImGui::RadioButton("Y", &w.expansionAxis, 1); ImGui::SameLine();
                        ImGui::RadioButton("XY", &w.expansionAxis, 2);
                        ImGui::Checkbox("Stop on Contact", &w.stopOnContact);
                        if (w.stopOnContact) ImGui::InputInt("Target Wall", &w.stopTargetIdx);
                        ImGui::DragFloat("Max Size", &w.maxSize, 0.5f, 0.0f, 100.0f);
                        ImGui::Unindent();
                    }
                }

                if (ImGui::CollapsingHeader("Kinematic Movement")) {
                    if (ImGui::Checkbox("Is Moving Platform", &w.isMoving)) {
                        if (w.isMoving) {
                            w.pointA = w.body->GetPosition();
                            w.pointB = w.pointA + b2Vec2(5.0f, 0.0f);
                            w.body->SetType(b2_kinematicBody);
                        } else {
                            w.body->SetType(b2_staticBody);
                            w.body->SetLinearVelocity(b2Vec2(0.0f, 0.0f)); 
                        }
                    }

                    if (w.isMoving) {
                        ImGui::Indent();
                        float pA[2] = { w.pointA.x, w.pointA.y };
                        if (ImGui::DragFloat2("Point A", pA, 0.1f)) w.pointA.Set(pA[0], pA[1]);
                        
                        float pB[2] = { w.pointB.x, w.pointB.y };
                        if (ImGui::DragFloat2("Point B", pB, 0.1f)) w.pointB.Set(pB[0], pB[1]);
                        
                        ImGui::DragFloat("Speed", &w.moveSpeed, 0.1f, 0.1f, 50.0f);
                        
                        if (ImGui::Button("Set A = Current", ImVec2(-1, 0))) w.pointA = w.body->GetPosition();
                        if (ImGui::Button("Set B = Current", ImVec2(-1, 0))) w.pointB = w.body->GetPosition();

                        ImGui::Checkbox("Reverse on Wall", &w.reverseOnContact);
                        if (w.reverseOnContact) {
                            ImGui::Checkbox("Free Bounce", &w.freeBounce);
                            if (w.isFreeBouncing && ImGui::Button("Reset Route")) {
                                w.isFreeBouncing = false;
                                w.body->SetLinearVelocity(b2Vec2(0.0f, 0.0f)); 
                            }
                        }
                        ImGui::Unindent();
                    }
                }

                ImGui::Separator();
                ImGui::Spacing();
                if (ImGui::Button("DUPLICATE", ImVec2(-1, 30))) wallToDuplicate = selectedIndex;
                ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
                if (ImGui::Button("DELETE", ImVec2(-1, 30))) wallToDelete = selectedIndex;
                ImGui::PopStyleColor(3);

            } else {
                selectedType = EntityType::None; // Safe fallback si se borró y quedó desfasado el índice
            }
        }

        ImGui::End();

        // Procesamiento de comandos de la UI (Borrar / Duplicar)
        if (wallToDelete != -1) {
            physics.removeCustomWall(wallToDelete);
            selectedType = EntityType::None; // Deseleccionamos por seguridad
        }
        if (wallToDuplicate != -1) {
            physics.duplicateCustomWall(wallToDuplicate);
            selectedType = EntityType::Wall;
            selectedIndex = (int)physics.getCustomWalls().size() - 1; // Seleccionamos el clon nuevo
        }

        // ==============================================
        // --- DRAW: RENDERIZADO AL BUFFER GIGANTE ---
        // ==============================================
        gameBuffer.clear(sf::Color(10, 10, 10)); 
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
            
            float baseThickness = 0.08f * physics.SCALE; 
            float thickness = baseThickness + (wall.flashTimer * baseThickness);
            shapeToDraw->setOutlineThickness(-thickness);

            gameBuffer.draw(*shapeToDraw); 

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
                zoneRect.setOutlineThickness(0.1f * physics.SCALE); // <--- Relativo
                gameBuffer.draw(zoneRect);
            }
        }

        const auto& statuses = physics.getRacerStatus();
        float tombSize = 0.8f * physics.SCALE;  // 1.0 metros en escala visual
        float crossThick = 0.15f * physics.SCALE; // Grosor de la cruz
        float outlineThick = 0.08f * physics.SCALE;

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
                grave.setOutlineThickness(outlineThick);
                gameBuffer.draw(grave); 

            
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

                gameBuffer.draw(bar1); 
                gameBuffer.draw(bar2);
            }
        }

        for (size_t i = 0; i < trails.size(); ++i) {
            const auto& pts = trails[i].points;
            if (pts.size() < 2) continue; 

            sf::VertexArray glowVA(sf::TriangleStrip, pts.size() * 2);
            sf::VertexArray coreVA(sf::TriangleStrip, pts.size() * 2);
            
            float baseWidth = physics.currentRacerSize * physics.SCALE; 

            for (size_t j = 0; j < pts.size(); ++j) {
                sf::Vector2f normal(1, 0);
                
                if (j == 0) {
                    sf::Vector2f dir = pts[1] - pts[0];
                    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
                    if (len > 0.001f) normal = sf::Vector2f(-dir.y/len, dir.x/len);
                } else if (j == pts.size() - 1) {
                    sf::Vector2f dir = pts[j] - pts[j-1];
                    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
                    if (len > 0.001f) normal = sf::Vector2f(-dir.y/len, dir.x/len);
                } else {
                    sf::Vector2f d1 = pts[j] - pts[j-1];
                    sf::Vector2f d2 = pts[j+1] - pts[j];
                    
                    float l1 = std::sqrt(d1.x*d1.x + d1.y*d1.y);
                    float l2 = std::sqrt(d2.x*d2.x + d2.y*d2.y);
                    
                    sf::Vector2f n1(1, 0), n2(1, 0);
                    if (l1 > 0.001f) n1 = sf::Vector2f(-d1.y/l1, d1.x/l1);
                    if (l2 > 0.001f) n2 = sf::Vector2f(-d2.y/l2, d2.x/l2);
                    
                    normal = n1 + n2;
                    float lenN = std::sqrt(normal.x*normal.x + normal.y*normal.y);
                    
                    if (lenN > 0.001f) {
                        normal.x /= lenN;
                        normal.y /= lenN;
                        float miter = normal.x * n1.x + normal.y * n1.y;
                        if (miter > 0.2f) { 
                            normal.x /= miter;
                            normal.y /= miter;
                        }
                    } else {
                        normal = n1; 
                    }
                }

                float lifePct = 1.0f - ((float)j / (float)pts.size());
                float widthPct = std::pow(lifePct, 0.6f); 
                float glowWidth = baseWidth * widthPct * 1.4f;
                float coreWidth = baseWidth * widthPct * 0.5f;
                
                sf::Color baseColor = trails[i].color;
                
                sf::Color glowColor = baseColor;
                glowColor.a = (sf::Uint8)(std::pow(lifePct, 1.5f) * 160.0f); 
                
                glowVA[j*2].position = pts[j] + normal * (glowWidth * 0.5f);
                glowVA[j*2].color = glowColor;
                glowVA[j*2+1].position = pts[j] - normal * (glowWidth * 0.5f);
                glowVA[j*2+1].color = glowColor;

                sf::Color coreColor = sf::Color::White;
                coreColor.r = (baseColor.r + 255 * 2) / 3;
                coreColor.g = (baseColor.g + 255 * 2) / 3;
                coreColor.b = (baseColor.b + 255 * 2) / 3;
                coreColor.a = (sf::Uint8)(lifePct * 240.0f);
                
                coreVA[j*2].position = pts[j] + normal * (coreWidth * 0.5f);
                coreVA[j*2].color = coreColor;
                coreVA[j*2+1].position = pts[j] - normal * (coreWidth * 0.5f);
                coreVA[j*2+1].color = coreColor;
            }
            
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
            rect.setOutlineThickness(-0.1f * physics.SCALE);
            gameBuffer.draw(rect); 
        }

        // --- DRAW PARTÍCULAS ---
        const auto& particles = physics.getParticles();
        if (!particles.empty()) {
            // Usamos Quads, necesitamos 4 vértices por partícula
            sf::VertexArray va(sf::Quads, particles.size() * 4);
            
            // Tamaño de la partícula escalado a la resolución bruta (2160p)
            float pSize = (RENDER_WIDTH / 1080.0f) * 4.0f; 
            
            for (size_t i = 0; i < particles.size(); ++i) {
                const auto& p = particles[i];
                sf::Color c = p.color;
                
                // Hacemos que se desvanezcan en el canal Alpha según su vida
                c.a = (sf::Uint8)(255.0f * (p.life / p.maxLife));
                
                // Construimos el cuadradito
                va[i*4 + 0].position = p.position + sf::Vector2f(-pSize, -pSize);
                va[i*4 + 1].position = p.position + sf::Vector2f(pSize, -pSize);
                va[i*4 + 2].position = p.position + sf::Vector2f(pSize, pSize);
                va[i*4 + 3].position = p.position + sf::Vector2f(-pSize, pSize);
                
                va[i*4 + 0].color = c;
                va[i*4 + 1].color = c;
                va[i*4 + 2].color = c;
                va[i*4 + 3].color = c;
            }
            gameBuffer.draw(va);
        }

        gameBuffer.display();

        // 1. Declaramos el sprite acá arriba para que exista en todo este bloque
        sf::Sprite renderSprite;

        if (enableBloom) {
            // 1. EXTRAER BRILLO 
            brightnessShader.setUniform("source", sf::Shader::CurrentTexture);
            brightnessShader.setUniform("threshold", bloomThreshold);
            brightnessBuffer.clear(sf::Color::Black);
            sf::Sprite brightSprite(gameBuffer.getTexture());
            brightSprite.setScale(0.5f, 0.5f); 
            brightnessBuffer.draw(brightSprite, &brightnessShader);
            brightnessBuffer.display();

            // 2. DESENFOQUE GAUSSIANO 
            // FIX: Puntero a constante porque getTexture() devuelve const
            const sf::Texture* currentSource = &brightnessBuffer.getTexture();
            
            for (int i = 0; i < blurIterations; ++i) {
                // Pasada Horizontal
                blurShader.setUniform("source", sf::Shader::CurrentTexture);
                blurShader.setUniform("dir", sf::Vector2f(1.0f / BLOOM_W, 0.0f));
                blurBuffer1.clear(sf::Color::Transparent);
                blurBuffer1.draw(sf::Sprite(*currentSource), &blurShader);
                blurBuffer1.display();

                // Pasada Vertical
                blurShader.setUniform("source", sf::Shader::CurrentTexture);
                blurShader.setUniform("dir", sf::Vector2f(0.0f, 1.0f / BLOOM_H));
                blurBuffer2.clear(sf::Color::Transparent);
                blurBuffer2.draw(sf::Sprite(blurBuffer1.getTexture()), &blurShader);
                blurBuffer2.display();
                
                // FIX: La reasignación ahora funciona perfecto porque el puntero es const
                currentSource = &blurBuffer2.getTexture();
            }

            // 3. FUSIÓN ADITIVA 
            blendShader.setUniform("baseTexture", sf::Shader::CurrentTexture);
            blendShader.setUniform("bloomTexture", *currentSource);
            blendShader.setUniform("multiplier", bloomMultiplier);
            
            finalBuffer.clear();
            sf::Sprite finalBaseSprite(gameBuffer.getTexture());
            finalBuffer.draw(finalBaseSprite, &blendShader);
            finalBuffer.display();

            recorder.addFrame(finalBuffer.getTexture());
            renderSprite.setTexture(finalBuffer.getTexture()); // Asignamos textura
        } else {
            recorder.addFrame(gameBuffer.getTexture());
            renderSprite.setTexture(gameBuffer.getTexture()); // Asignamos textura
        }

        window.clear(sf::Color(20, 20, 20)); 

        // 2. Ahora el resto del código que ya tenías para centrar el viewport
        // se aplica sobre el renderSprite que ya tiene su textura correcta.
        float scale = DISPLAY_SIZE / (float)RENDER_WIDTH; 
        renderSprite.setScale(scale, scale);
        
        float offsetX = (desktopMode.width - DISPLAY_SIZE) / 2.0f;
        float offsetY = (desktopMode.height - DISPLAY_SIZE) / 2.0f;
        
        renderSprite.setPosition(offsetX, offsetY);

        // Opcional: Le metemos un marquito sutil al viewport para que se despegue del fondo
   /*   sf::RectangleShape viewportBorder(sf::Vector2f(DISPLAY_SIZE + 2, DISPLAY_SIZE + 2));
        viewportBorder.setPosition(offsetX - 1, offsetY - 1);
        viewportBorder.setFillColor(sf::Color::Transparent);
        viewportBorder.setOutlineColor(sf::Color(60, 60, 60));
        viewportBorder.setOutlineThickness(1.0f);
        
        window.draw(viewportBorder); */
        window.draw(renderSprite);

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}