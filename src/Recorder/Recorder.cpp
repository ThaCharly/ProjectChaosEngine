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

    // --- MAGIA ROJA DE AMD (VA-API) ---
    // Usamos el bloque VCN del Ryzen. 
    // -vaapi_device apunta al nodo de renderizado de la iGPU.
    // hwupload pasa el frame por DMA directo al codificador de hardware.
    std::string cmd = "ffmpeg -y -loglevel warning "
                      "-vaapi_device /dev/dri/renderD128 " 
                      "-f rawvideo -vcodec rawvideo "
                      "-s " + std::to_string(width) + "x" + std::to_string(height) + " "
                      "-pix_fmt rgba "
                      "-r " + std::to_string(fps) + " "
                      "-i - "
                      "-vf \"format=nv12,hwupload\" " 
                      "-c:v h264_vaapi -qp 20 -b:v 50M " 
                      "\"" + tempVideoFilename + "\""; 

    ffmpegPipe = popen(cmd.c_str(), "w");
    if (!ffmpegPipe) throw std::runtime_error("No se pudo iniciar FFmpeg.");

    audioMixBuffer.reserve(44100 * 60 * 5); 
    
    isWorkerRunning = true;
    workerThread = std::thread(&Recorder::workerLoop, this);

    std::cout << "[REC] Grabando video 4K por hardware (AMD VCN) en: " << tempVideoFilename << std::endl;
}

Recorder::~Recorder() {
    stop(); 
}

void Recorder::addFrame(const sf::Texture& texture) {
    if (!ffmpegPipe || !isRecording) return;
    currentFrame++;
    
    // Extracción de VRAM a RAM. (Aún frena el hilo, pero es la única copia)
    sf::Image img = texture.copyToImage();

    // Movemos la imagen a la cola, no copiamos los píxeles
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        frameQueue.push(std::move(img)); 
    }
    queueCV.notify_one();
}

void Recorder::workerLoop() {
    while (true) {
        sf::Image currentImage;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [this] { return !frameQueue.empty() || !isWorkerRunning; });
            
            if (frameQueue.empty() && !isWorkerRunning) break;

            currentImage = std::move(frameQueue.front());
            frameQueue.pop();
        }

        // Leemos directo del buffer interno de sf::Image en el hilo de FFmpeg
        if (ffmpegPipe) {
            fwrite(currentImage.getPixelsPtr(), 1, width * height * 4, ffmpegPipe);
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