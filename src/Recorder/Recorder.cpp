#include "Recorder.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm> 
#include <cmath>     

Recorder::Recorder(int width, int height, int fps, const std::string& outputFilename) 
    : width(width), height(height), fps(fps), finalFilename(outputFilename) 
{
    if (!captureTexture.create(width, height)) {
        throw std::runtime_error("No se pudo crear la textura de captura");
    }

    tempVideoFilename = outputFilename + ".temp_video.mp4";
    tempAudioFilename = outputFilename + ".temp_audio.wav";

    std::string cmd = "ffmpeg -y "
                      "-f rawvideo -vcodec rawvideo "
                      "-s " + std::to_string(width) + "x" + std::to_string(height) + " "
                      "-pix_fmt rgba "
                      "-r " + std::to_string(fps) + " "
                      "-i - "
                      "-c:v libx264 -preset ultrafast "
                      "-pix_fmt yuv420p "
                      "\"" + tempVideoFilename + "\""; 

    ffmpegPipe = popen(cmd.c_str(), "w");

    if (!ffmpegPipe) {
        throw std::runtime_error("No se pudo iniciar FFmpeg.");
    }

    audioMixBuffer.reserve(44100 * 60 * 10); 
    std::cout << "[REC] Grabando video temporal..." << std::endl;
}

Recorder::~Recorder() {
    stop(); // Aseguramos cierre si el objeto se destruye
}

void Recorder::stop() {
    if (isFinished) return; // Ya se cerró, no hacemos nada
    isFinished = true;
    isRecording = false;

    // 1. CERRAR VIDEO PIPE
    if (ffmpegPipe) {
        std::cout << "[REC] Finalizando captura de video..." << std::endl;
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;
    }

    // 2. EXPORTAR AUDIO WAV
    if (!audioMixBuffer.empty()) {
        std::cout << "[REC] Exportando audio temporal..." << std::endl;
        
        std::vector<sf::Int16> finalSamples;
        finalSamples.reserve(audioMixBuffer.size());

        for (float sample : audioMixBuffer) {
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            finalSamples.push_back(static_cast<sf::Int16>(sample));
        }

        sf::OutputSoundFile audioFile;
        if (audioFile.openFromFile(tempAudioFilename, 44100, 1)) { 
            audioFile.write(finalSamples.data(), finalSamples.size());
        }
    } else {
        // Silencio de seguridad
        sf::OutputSoundFile audioFile;
        sf::Int16 silence[44100] = {0};
        if (audioFile.openFromFile(tempAudioFilename, 44100, 1)) {
            audioFile.write(silence, 44100); 
        }
    }

    // 3. FUSIÓN FINAL (FFMPEG MERGE)
    std::cout << "[REC] Fusionando Audio y Video (Espere)..." << std::endl;
    
    // Agregamos -shortest para que corte cuando termine el video
    std::string mergeCmd = "ffmpeg -y -i \"" + tempVideoFilename + "\" -i \"" + tempAudioFilename + "\" "
                           "-c:v copy -c:a aac -shortest \"" + finalFilename + "\"";
    
    int result = system(mergeCmd.c_str());

    if (result == 0) {
        std::cout << "[REC] EXITO: Video guardado en " << finalFilename << std::endl;
        remove(tempVideoFilename.c_str());
        remove(tempAudioFilename.c_str());
    } else {
        std::cerr << "[REC] Error al fusionar archivos." << std::endl;
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
        audioMixBuffer[startIndex + i] += (float)samples[i] * volFactor;
    }
}