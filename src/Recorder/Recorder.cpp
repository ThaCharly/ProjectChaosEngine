#include "Recorder.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm> 
#include <cmath>     
#include <fstream> 

Recorder::Recorder(int width, int height, int fps, const std::string& outputFilename) 
    : width(width), height(height), fps(fps), finalFilename(outputFilename) 
{
    if (!captureTexture.create(width, height)) {
        throw std::runtime_error("No se pudo crear la textura de captura");
    }

    tempVideoFilename = "temp_video_render.mp4";
    tempAudioFilename = "temp_audio_render.wav";

    std::string cmd = "ffmpeg -y -loglevel warning "
                      "-f rawvideo -vcodec rawvideo "
                      "-s " + std::to_string(width) + "x" + std::to_string(height) + " "
                      "-pix_fmt rgba "
                      "-r " + std::to_string(fps) + " "
                      "-i - "
                      "-c:v libx264 -preset ultrafast "
                      "-pix_fmt yuv420p "
                      "\"" + tempVideoFilename + "\""; 

    ffmpegPipe = popen(cmd.c_str(), "w");
    if (!ffmpegPipe) throw std::runtime_error("No se pudo iniciar FFmpeg.");

    audioMixBuffer.reserve(44100 * 60 * 5); 
    std::cout << "[REC] Grabando video temporal en: " << tempVideoFilename << std::endl;
}

Recorder::~Recorder() {
    stop(); 
}

void Recorder::stop() {
    if (isFinished) return;
    isFinished = true;
    isRecording = false;

    if (ffmpegPipe) {
        std::cout << "[REC] Cerrando pipe de video..." << std::endl;
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;
    }

    if (!audioMixBuffer.empty()) {
        std::cout << "[REC] Procesando audio (Normalizando)..." << std::endl;

        // 1. ENCONTRAR PICO MÁXIMO (Para evitar clipping/clogging)
        float maxPeak = 0.0f;
        for (float s : audioMixBuffer) {
            if (std::abs(s) > maxPeak) maxPeak = std::abs(s);
        }

        // 2. CALCULAR GANANCIA
        // El límite de Int16 es 32767. Dejamos un margen (32000) por seguridad.
        float gain = 1.0f;
        if (maxPeak > 32000.0f) {
            gain = 32000.0f / maxPeak;
            std::cout << "[REC] Audio saturado detectado (Pico: " << maxPeak << "). Reduciendo volumen general por factor: " << gain << std::endl;
        } else if (maxPeak > 0.0f && maxPeak < 10000.0f) {
            // Opcional: Si quedó muy bajito, lo subimos un poco también
            gain = 25000.0f / maxPeak;
            std::cout << "[REC] Audio bajo detectado. Boost: " << gain << std::endl;
        }

        std::vector<sf::Int16> finalSamples;
        finalSamples.reserve(audioMixBuffer.size() * 2);

        // 3. APLICAR GANANCIA Y EXPORTAR
        for (float sample : audioMixBuffer) {
            // Aplicamos la normalización
            float normalizedSample = sample * gain;

            // Clamping final de seguridad (por si acaso)
            if (normalizedSample > 32767.0f) normalizedSample = 32767.0f;
            if (normalizedSample < -32768.0f) normalizedSample = -32768.0f;
            
            sf::Int16 s = static_cast<sf::Int16>(normalizedSample);
            finalSamples.push_back(s); 
            finalSamples.push_back(s); 
        }

        sf::OutputSoundFile audioFile;
        if (audioFile.openFromFile(tempAudioFilename, 44100, 2)) { 
            audioFile.write(finalSamples.data(), finalSamples.size());
            audioFile.close(); 
        }
    }

    std::cout << "[REC] Iniciando fusion final..." << std::endl;
    
    std::string mergeCmd = "ffmpeg -y -loglevel error -i " + tempVideoFilename + " -i " + tempAudioFilename + 
                           " -c:v copy -c:a aac -b:a 192k -shortest " + finalFilename;
    
    int result = system(mergeCmd.c_str());

    if (result == 0) {
        std::cout << "[REC] EXITO TOTAL: " << finalFilename << std::endl;
        remove(tempVideoFilename.c_str());
        remove(tempAudioFilename.c_str());
    } else {
        std::cerr << "[REC] Error en la fusion de FFmpeg." << std::endl;
    }
}

void Recorder::addFrame(const sf::Window& window) {
    if (!ffmpegPipe || !isRecording) return;
    currentFrame++;
    captureTexture.update(window);
    sf::Image img = captureTexture.copyToImage();
    const sf::Uint8* pixels = img.getPixelsPtr();
    fwrite(pixels, 1, width * height * 4, ffmpegPipe);
}

void Recorder::addAudioEvent(const sf::Int16* samples, std::size_t sampleCount, float volume) {
    if (!isRecording) return;

    size_t startIndex = (size_t)((double)currentFrame / fps * sampleRate);
    size_t requiredSize = startIndex + sampleCount;
    
    if (audioMixBuffer.size() < requiredSize) {
        audioMixBuffer.resize(requiredSize, 0.0f); 
    }
    
    float volFactor = volume / 100.0f;
    for (size_t i = 0; i < sampleCount; ++i) {
        // Suma directa (el clipping se arregla al final en stop())
        audioMixBuffer[startIndex + i] += (float)samples[i] * volFactor;
    }
}