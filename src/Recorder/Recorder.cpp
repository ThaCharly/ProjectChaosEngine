#include "Recorder.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm> 
#include <cmath>     
#include <fstream> 

Recorder::Recorder(int width, int height, int fps, const std::string& outputFilename) 
    : width(width), height(height), fps(fps), finalFilename(outputFilename) 
{
    this->width = width + (width % 2);
    this->height = height + (height % 2);

    tempVideoFilename = "temp_video_render.mp4";
    tempAudioFilename = "temp_audio_render.wav";

    // Usamos la fuerza bruta del Ryzen: 14 hilos para FFmpeg, dejamos 2 libres para el motor físico.
    // -crf 18 te da calidad casi lossless sin generar archivos de 40GB.
    std::string cmd = "ffmpeg -y -loglevel warning "
                      "-f rawvideo -vcodec rawvideo "
                      "-s " + std::to_string(width) + "x" + std::to_string(height) + " "
                      "-pix_fmt rgba "
                      "-r " + std::to_string(fps) + " "
                      "-i - "
                      "-c:v libx264 -preset ultrafast -crf 18 -threads 14 " 
                      "-pix_fmt yuv420p "
                      "\"" + tempVideoFilename + "\""; 

    ffmpegPipe = popen(cmd.c_str(), "w");
    if (!ffmpegPipe) throw std::runtime_error("No se pudo iniciar FFmpeg.");

    audioMixBuffer.reserve(44100 * 60 * 5); 
    
    isWorkerRunning = true;
    workerThread = std::thread(&Recorder::workerLoop, this);

    std::cout << "[REC] Grabando video 4K (CPU Multihilo) en: " << tempVideoFilename << std::endl;
}

Recorder::~Recorder() {
    stop(); 
}

void Recorder::workerLoop() {
    while (true) {
        std::vector<sf::Uint8> frameData;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            // El hilo se duerme si la cola está vacía, no gasta CPU
            queueCV.wait(lock, [this] { return !frameQueue.empty() || !isWorkerRunning; });
            
            if (frameQueue.empty() && !isWorkerRunning) break; // Salida limpia

            frameData = std::move(frameQueue.front());
            frameQueue.pop();
        }

        // Acá está el cuello de botella que trancaba el juego, ahora corre aislado
        if (ffmpegPipe) {
            fwrite(frameData.data(), 1, width * height * 4, ffmpegPipe);
        }
    }
}

void Recorder::stop() {
    if (isFinished) return;
    isFinished = true;
    isRecording = false;

    // --- FRENAR EL HILO LIMPIAMENTE ---
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        isWorkerRunning = false;
    }
    queueCV.notify_one();
    if (workerThread.joinable()) {
        std::cout << "[REC] Esperando a que FFmpeg termine de digerir la cola de frames..." << std::endl;
        workerThread.join();
    }

    if (ffmpegPipe) {
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;
    }

    // (El resto del método stop() del Audio Mix y Fusión dejalo igualito a como lo tenés)
    if (!audioMixBuffer.empty()) {
        std::cout << "[REC] Procesando audio (Normalizando)..." << std::endl;
        float maxPeak = 0.0f;
        for (float s : audioMixBuffer) {
            if (std::abs(s) > maxPeak) maxPeak = std::abs(s);
        }

        float gain = 1.0f;
        if (maxPeak > 32000.0f) {
            gain = 32000.0f / maxPeak;
        } else if (maxPeak > 0.0f && maxPeak < 10000.0f) {
            gain = 25000.0f / maxPeak;
        }

        std::vector<sf::Int16> finalSamples;
        finalSamples.reserve(audioMixBuffer.size() * 2);

        for (float sample : audioMixBuffer) {
            float normalizedSample = sample * gain;
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

void Recorder::addFrame(const sf::Texture& texture) {
    if (!ffmpegPipe || !isRecording) return;
    currentFrame++;
    
    // SFML te frena acá para pasar VRAM a RAM, pero es el mínimo mal posible.
    sf::Image img = texture.copyToImage();
    const sf::Uint8* pixels = img.getPixelsPtr();
    size_t dataSize = width * height * 4;
    
    // Armamos el buffer crudo
    std::vector<sf::Uint8> buffer(pixels, pixels + dataSize);

    // Lo empujamos a la cola con candado para que el hilo trabajador lo mastique
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        frameQueue.push(std::move(buffer));
    }
    queueCV.notify_one();
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