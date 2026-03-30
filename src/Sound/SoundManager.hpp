#pragma once

#include <SFML/Audio.hpp>
#include <vector>
#include <map>
#include <cmath>
#include <iostream>
#include <random>
#include <cstdint> // ¡Clave para std::int16_t!

// Forward declaration
class Recorder;

class SoundManager {
public:
    SoundManager() {
        rng.seed(std::random_device{}());

        // --- CAMBIO: GENERAR LAS 128 NOTAS MIDI ---
        // Fórmula: f = 440 * 2^((d - 69) / 12)
        // Generamos del 0 al 127.
        for (int i = 0; i < 128; ++i) {
            float freq = 440.0f * std::pow(2.0f, (i - 69) / 12.0f);
            generateTone(i, freq);
        }

        // Pool de voces: SFML 3 exige que le pasemos un buffer al nacer.
        // Como el mapa 'midiBuffers' ya tiene todos los buffers creados (porque 
        // generateTone se ejecutó antes), agarramos el buffer de la nota 0 
        // solo como un 'dummy' para satisfacer al constructor. 
        // Después en playMidiNote() se pisa con el buffer correcto usando setBuffer().
        const sf::SoundBuffer& dummyBuffer = midiBuffers[0];
        
        for(int i = 0; i < 64; ++i) {
            soundPool.emplace_back(dummyBuffer);
        }
    }

    void setRecorder(Recorder* rec) {
        recorder = rec;
    }

    // Generador de ondas (Senoide suave)
    void generateTone(int id, float frequency) {
        const unsigned SAMPLE_RATE = 44100;
        const int AMPLITUDE = 18000; 

        std::vector<std::int16_t> rawSamples; // Adiós sf::Int16
        float duration = 0.3f; // Un poquito más cortas para melodías rápidas
        int numSamples = (int)(SAMPLE_RATE * duration);
        float attackTime = 0.01f; // Ataque rápido

        for (int i = 0; i < numSamples; i++) {
            float t = (float)i / SAMPLE_RATE;
            float wave = std::sin(2 * 3.14159f * frequency * t);
            
            float envelope = 0.0f;
            if (t < attackTime) {
                envelope = t / attackTime;
            } else {
                envelope = std::exp(-10.0f * (t - attackTime)); 
            }

            // Safety release para evitar "pops"
            float fadeOutStart = duration - 0.05f;
            if (t > fadeOutStart) {
                float fade = 1.0f - ((t - fadeOutStart) / 0.05f);
                if (fade < 0.0f) fade = 0.0f;
                envelope *= fade;
            }

            rawSamples.push_back((std::int16_t)(wave * envelope * AMPLITUDE)); // Adiós sf::Int16
        }

        sf::SoundBuffer buffer;
        // SFML 3: Puntero, cantidad, canales (1), sample rate, y el Channel Map al final.
        // Además, es buena práctica en C++11 en adelante usar .data() en vez de &vector[0]
        if (buffer.loadFromSamples(rawSamples.data(), rawSamples.size(), 1, SAMPLE_RATE, {sf::SoundChannel::Mono})) {
            midiBuffers[id] = buffer;
        }
    }

    // ESTA ES LA NUEVA FUNCIÓN CLAVE
    void playMidiNote(int noteNumber, float volume = 98.0f) {
        if (noteNumber < 0 || noteNumber > 127) return;
        if (midiBuffers.find(noteNumber) == midiBuffers.end()) return;

        sf::Sound* sound = getFreeSound();
        if (sound) {
            sound->setBuffer(midiBuffers[noteNumber]);
            sound->setVolume(volume); 
            sound->setPitch(1.0f);
            sound->setPosition({0.0f, 0.0f, 0.0f}); // SFML 3: Pide Vector3 explícito
            sound->setAttenuation(0);    // Que se escuche igual en todos lados
            sound->play();
        }

        if (recorder) {
             const sf::SoundBuffer& buf = midiBuffers[noteNumber];
             sendToRecorder(buf.getSamples(), buf.getSampleCount(), volume);
        }
    }

    // Mantenemos esta por compatibilidad con el código viejo, mapeando IDs viejos a notas MIDI
    void playSound(int id, float xPosition, float worldWidth) {
        // Mapeo trucho: Si piden ID 1 (Do), tocamos MIDI 60 (Do central)
        // Esto es solo para que no crashee si usas el modo viejo.
        int midiMap[] = { 0, 60, 62, 64, 65, 67, 69, 71, 72 }; 
        if (id > 0 && id <= 8) playMidiNote(midiMap[id]);
    }

    void sendToRecorder(const std::int16_t* samples, std::size_t count, float vol);

private:
    sf::Sound* getFreeSound() {
        for (auto& s : soundPool) {
            // SFML 3: Status es un enum class fuertemente tipado
            if (s.getStatus() == sf::Sound::Status::Stopped) return &s; 
        }
        return &soundPool[0]; 
    }

    // Cambiamos el nombre para ser claros
    std::map<int, sf::SoundBuffer> midiBuffers;
    std::vector<sf::Sound> soundPool;
    Recorder* recorder = nullptr;
    std::mt19937 rng;
};