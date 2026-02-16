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

    // Nota: Agregué -loglevel warning para limpiar la consola
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
    std::cout << "[REC] Grabando video temporal..." << std::endl;
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

    // DEBUG AUDIO
    std::cout << "[REC] Buffer de audio final: " << audioMixBuffer.size() << " samples." << std::endl;

    if (!audioMixBuffer.empty()) {
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
            std::cout << "[REC] Archivo de audio temporal guardado." << std::endl;
        } else {
            std::cerr << "[REC] ERROR al guardar archivo de audio." << std::endl;
        }
    } else {
        std::cout << "[REC] ADVERTENCIA: Buffer de audio vacio. Generando silencio." << std::endl;
        sf::OutputSoundFile audioFile;
        sf::Int16 silence[44100] = {0};
        if (audioFile.openFromFile(tempAudioFilename, 44100, 1)) audioFile.write(silence, 44100); 
    }

    std::cout << "[REC] Fusionando..." << std::endl;
    std::string mergeCmd = "ffmpeg -y -loglevel error -i \"" + tempVideoFilename + "\" -i \"" + tempAudioFilename + "\" "
                           "-c:v copy -c:a aac -shortest \"" + finalFilename + "\"";
    
    int result = system(mergeCmd.c_str());

    if (result == 0) {
        std::cout << "[REC] EXITO: " << finalFilename << std::endl;
        remove(tempVideoFilename.c_str());
        remove(tempAudioFilename.c_str());
    } else {
        std::cerr << "[REC] Error en fusion." << std::endl;
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
    
    // DEBUG: Ver si llega algo
    // static int eventCount = 0;
    // if (eventCount++ % 10 == 0) std::cout << "." << std::flush;

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