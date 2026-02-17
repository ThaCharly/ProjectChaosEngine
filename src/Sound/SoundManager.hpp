#pragma once

#include <SFML/Audio.hpp>
#include <vector>
#include <map>
#include <cmath>
#include <iostream>
#include <random>

// Forward declaration
class Recorder;

class SoundManager {
public:
    SoundManager() {
        // Inicializar RNG
        rng.seed(std::random_device{}());

        // Frecuencias de Do Mayor (Octava 4/5)
        float frequencies[] = {
            261.63f, 293.66f, 329.63f, 349.23f, 
            392.00f, 440.00f, 493.88f, 523.25f 
        };

        for (int i = 0; i < 8; ++i) {
            generateTone(i + 1, frequencies[i]);
        }

        // Pool de 32 voces
        for(int i=0; i<32; ++i) soundPool.emplace_back();
    }

    void setRecorder(Recorder* rec) {
        recorder = rec;
    }

    void generateTone(int id, float frequency) {
        const unsigned SAMPLE_RATE = 44100;
        // Bajamos un poco más la amplitud para evitar saturación interna
        const int AMPLITUDE = 18000; 

        std::vector<sf::Int16> rawSamples;
        float duration = 0.4f; // Duración total
        int numSamples = (int)(SAMPLE_RATE * duration);

        // Suavizado inicial (Attack) más largo para evitar el "golpe"
        float attackTime = 0.02f; // 20ms

        for (int i = 0; i < numSamples; i++) {
            float t = (float)i / SAMPLE_RATE;
            
            // ONDA SENOIDAL PURA (8-bit style limpio)
            float wave = std::sin(2 * 3.14159f * frequency * t);
            
            // --- ENVELOPE MEJORADO ---
            float envelope = 0.0f;

            if (t < attackTime) {
                // FADE IN (Attack): Subida lineal suave
                envelope = t / attackTime;
            } else {
                // FADE OUT (Decay): Caída exponencial más agresiva para que no quede "cola"
                envelope = std::exp(-8.0f * (t - attackTime)); 
            }

            // --- SAFETY RELEASE (CRÍTICO) ---
            // Forzamos que el sonido muera a CERO en los últimos 50ms.
            // Esto elimina el "Pop" o "Crack" final si la exponencial no llegó a 0.
            float fadeOutStart = duration - 0.05f;
            if (t > fadeOutStart) {
                float fade = 1.0f - ((t - fadeOutStart) / 0.05f);
                if (fade < 0.0f) fade = 0.0f;
                envelope *= fade;
            }

            rawSamples.push_back((sf::Int16)(wave * envelope * AMPLITUDE));
        }

        sf::SoundBuffer buffer;
        if (buffer.loadFromSamples(&rawSamples[0], rawSamples.size(), 1, SAMPLE_RATE)) {
            soundBuffers[id] = buffer;
        }
    }

    void playSound(int id, float xPosition, float worldWidth) {
        if (id <= 0 || soundBuffers.find(id) == soundBuffers.end()) return;

        // 1. REPRODUCIR EN VIVO
        sf::Sound* sound = getFreeSound();
        if (sound) {
            sound->setBuffer(soundBuffers[id]);
            sound->setVolume(80.0f); 
            sound->setPitch(1.0f); // Tono puro
            
            float normalizedX = (xPosition / worldWidth) * 2.0f - 1.0f; 
            sound->setPosition(normalizedX * 10.0f, 0.0f, 0.0f); 
            sound->setMinDistance(5.0f);
            sound->setAttenuation(1.0f);

            sound->play();
        }

        // 2. ENVIAR A LA GRABADORA
        if (recorder) {
             const sf::SoundBuffer& buf = soundBuffers[id];
             sendToRecorder(buf.getSamples(), buf.getSampleCount(), 80.0f);
        }
    }

private:
    sf::Sound* getFreeSound() {
        for (auto& s : soundPool) {
            if (s.getStatus() == sf::Sound::Stopped) return &s;
        }
        return &soundPool[0]; 
    }

    void sendToRecorder(const sf::Int16* samples, std::size_t count, float vol);

    std::map<int, sf::SoundBuffer> soundBuffers;
    std::vector<sf::Sound> soundPool;
    Recorder* recorder = nullptr;
    std::mt19937 rng;
};