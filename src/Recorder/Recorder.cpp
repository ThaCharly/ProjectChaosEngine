#include "Recorder.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm> 
#include <cmath>     
#include <fstream> 
#include <SFML/Window/Context.hpp> // Para enganchar funciones de OpenGL

// --- DEFINICIONES DE OPENGL ---
#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_STREAM_READ 0x88E1
#define GL_READ_ONLY 0x88B8
#endif

typedef void (*glGenBuffersFunc)(GLsizei, GLuint*);
typedef void (*glBindBufferFunc)(GLenum, GLuint);
typedef void (*glBufferDataFunc)(GLenum, GLsizeiptr, const GLvoid*, GLenum);
typedef void* (*glMapBufferFunc)(GLenum, GLenum);
typedef GLboolean (*glUnmapBufferFunc)(GLenum);
typedef void (*glDeleteBuffersFunc)(GLsizei, const GLuint*);

// Punteros globales para este archivo
glGenBuffersFunc my_glGenBuffers = nullptr;
glBindBufferFunc my_glBindBuffer = nullptr;
glBufferDataFunc my_glBufferData = nullptr;
glMapBufferFunc my_glMapBuffer = nullptr;
glUnmapBufferFunc my_glUnmapBuffer = nullptr;
glDeleteBuffersFunc my_glDeleteBuffers = nullptr;

Recorder::Recorder(int width, int height, int fps, const std::string& outputFilename) 
    : width(width), height(height), fps(fps), finalFilename(outputFilename) 
{
    this->width = width + (width % 2);
    this->height = height + (height % 2);

    tempVideoFilename = "temp_video_render.mp4";
    tempAudioFilename = "temp_audio_render.wav";

    // --- MAGIA VERDE DE NVIDIA (NVENC HEVC) ---
    // -vf "vflip,format=yuv420p": vflip corrige el eje Y de OpenGL, format asegura compatibilidad.
    // -c:v hevc_nvenc: Códec H.265 por hardware NVIDIA.
    // -preset p7 -tune hq: Calidad absolutamente máxima del encoder.
    // -rc vbr -cq 18 -b:v 0: Calidad Constante (18 es calidad visualmente sin pérdida).
    std::string cmd = "ffmpeg -y -loglevel warning "
                      "-f rawvideo -vcodec rawvideo "
                      "-s " + std::to_string(width) + "x" + std::to_string(height) + " "
                      "-pix_fmt rgba "
                      "-r " + std::to_string(fps) + " "
                      "-i - "
                      "-vf \"vflip,format=yuv420p\" " 
                      "-c:v hevc_nvenc -preset p4 -tune hq -rc vbr -cq 18 -b:v 0 " 
                      "\"" + tempVideoFilename + "\""; 

    ffmpegPipe = popen(cmd.c_str(), "w");
    if (!ffmpegPipe) throw std::runtime_error("No se pudo iniciar FFmpeg.");

    audioMixBuffer.reserve(44100 * 60 * 5);

    // --- 1. CARGAMOS LAS FUNCIONES EXTENDIDAS DE OPENGL ---
    my_glGenBuffers = (glGenBuffersFunc)sf::Context::getFunction("glGenBuffers");
    my_glBindBuffer = (glBindBufferFunc)sf::Context::getFunction("glBindBuffer");
    my_glBufferData = (glBufferDataFunc)sf::Context::getFunction("glBufferData");
    my_glMapBuffer = (glMapBufferFunc)sf::Context::getFunction("glMapBuffer");
    my_glUnmapBuffer = (glUnmapBufferFunc)sf::Context::getFunction("glUnmapBuffer");
    my_glDeleteBuffers = (glDeleteBuffersFunc)sf::Context::getFunction("glDeleteBuffers");

    if (!my_glGenBuffers || !my_glBindBuffer || !my_glBufferData || !my_glMapBuffer || !my_glUnmapBuffer) {
        throw std::runtime_error("Pah, la gráfica no soporta PBOs o falló la carga de OpenGL.");
    }

    // --- 2. INICIALIZAMOS EL TRIPLE BUFFER (EL POOL DE RAM AHORA ES DINÁMICO) ---
    size_t dataSize = this->width * this->height * 4;

    my_glGenBuffers(3, pbo);
    
    for (int i = 0; i < 3; ++i) {
        my_glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[i]);
        my_glBufferData(GL_PIXEL_PACK_BUFFER, dataSize, nullptr, GL_STREAM_READ);
    }
    
    my_glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    
    isWorkerRunning = true;
    workerThread = std::thread(&Recorder::workerLoop, this);

    std::cout << "[REC] Grabando video 4K ASÍNCRONO por hardware (NVIDIA HEVC/H.265) en: " << tempVideoFilename << std::endl;
}

Recorder::~Recorder() {
    stop(); 
    if (my_glDeleteBuffers) {
        my_glDeleteBuffers(3, pbo);
    }
}

void Recorder::addFrame(const sf::Texture& texture) {
    if (!ffmpegPipe || !isRecording) return;
    currentFrame++;
    
    size_t dataSize = width * height * 4;

    // 1. Forzamos a SFML a vincular su textura en la máquina de estados de OpenGL
    glBindTexture(GL_TEXTURE_2D, texture.getNativeHandle());

    // 2. TRANSFERENCIA ASÍNCRONA (VRAM -> PBO)
    my_glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[pboIndex]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    // 3. LEER EL FRAME ANTERIOR (PBO -> RAM)
    if (!firstFrame) {
        my_glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[nextPboIndex]);
        
        GLubyte* ptr = (GLubyte*)my_glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

        if (ptr) {
            std::vector<std::uint8_t> buffer;
            bool needsAllocation = false;
            
            // Tomamos un buffer reciclado, o creamos uno nuevo si no llegamos al límite
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                if (!freeQueue.empty()) {
                    buffer = std::move(freeQueue.front());
                    freeQueue.pop();
                } else if (totalAllocatedBuffers < MAX_QUEUE_SIZE) {
                    totalAllocatedBuffers++;
                    needsAllocation = true;
                } else {
                    // Si ya llegamos a los 3.3GB, toca esperar a que el encoder libere uno
                    queueSpaceCV.wait(lock, [this] { return !freeQueue.empty(); });
                    buffer = std::move(freeQueue.front());
                    freeQueue.pop();
                }
            }

            // Si tuvimos que crear uno nuevo, lo hacemos AFERA del bloqueo para que no tranque nada
            if (needsAllocation) {
                buffer.resize(dataSize);
            }

            // Copia de muy bajo nivel ultra rápida
            std::memcpy(buffer.data(), ptr, dataSize);
            my_glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

            {
                std::unique_lock<std::mutex> lock(queueMutex);
                frameQueue.push(std::move(buffer));
            }
            queueCV.notify_one();
        }
    } else {
        firstFrame = false; 
    }

    // 4. LIMPIEZA
    my_glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 5. CAMBIO DE ROLES (Triple Buffering)
    pboIndex = (pboIndex + 1) % 3;
    nextPboIndex = (pboIndex + 1) % 3;
}

// ... EL RESTO QUEDA IGUAL (workerLoop, stop, addAudioEvent) ...

void Recorder::workerLoop() {
    while (true) {
        std::vector<std::uint8_t> currentFrameData; 
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [this] { return !frameQueue.empty() || !isWorkerRunning; });
            
            if (frameQueue.empty() && !isWorkerRunning) break;

            currentFrameData = std::move(frameQueue.front());
            frameQueue.pop();
        }
        
        // Leemos directo de la memoria contigua del vector para escupirlo a FFmpeg
        if (ffmpegPipe) {
            fwrite(currentFrameData.data(), 1, width * height * 4, ffmpegPipe); 
        }

        // Devolvemos el buffer reciclado a la cola libre y avisamos que hay lugar
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            freeQueue.push(std::move(currentFrameData));
        }
        queueSpaceCV.notify_one(); 
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
        if (maxPeak > 38000.0f) {
            gain = 38000.0f / maxPeak;
        } else if (maxPeak > 0.0f && maxPeak < 32000.0f) {
            gain = 32000.0f / maxPeak;
        }

        std::vector<std::int16_t> finalSamples;
        finalSamples.reserve(audioMixBuffer.size() * 2);

        for (float sample : audioMixBuffer) {
            float normalizedSample = sample * gain;
            if (normalizedSample > 32767.0f) normalizedSample = 32767.0f;
            if (normalizedSample < -32768.0f) normalizedSample = -32768.0f;
            std::int16_t s = static_cast<std::int16_t>(normalizedSample);
            finalSamples.push_back(s); 
            finalSamples.push_back(s); 
        }

        sf::OutputSoundFile audioFile;
        // SFML 3: Pide explícitamente el ChannelMap para crear el archivo
        if (audioFile.openFromFile(tempAudioFilename, 44100, 2, {sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight})) { 
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

void Recorder::addAudioEvent(const std::int16_t* samples, std::size_t sampleCount, float volume) {
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