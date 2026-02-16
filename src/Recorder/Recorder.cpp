#include "Recorder.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm> // para std::max
#include <cmath>     // para std::abs

Recorder::Recorder(int width, int height, int fps, const std::string& outputFilename) 
    : width(width), height(height), fps(fps), finalFilename(outputFilename) 
{
    if (!captureTexture.create(width, height)) {
        throw std::runtime_error("No se pudo crear la textura de captura");
    }

    // Nombres temporales
    tempVideoFilename = outputFilename + ".temp_video.mp4";
    tempAudioFilename = outputFilename + ".temp_audio.wav";

    // 1. Iniciamos FFmpeg solo para el VIDEO (Mudo)
    std::string cmd = "ffmpeg -y "
                      "-f rawvideo -vcodec rawvideo "
                      "-s " + std::to_string(width) + "x" + std::to_string(height) + " "
                      "-pix_fmt rgba "
                      "-r " + std::to_string(fps) + " "
                      "-i - "
                      "-c:v libx264 -preset ultrafast "
                      "-pix_fmt yuv420p "
                      "\"" + tempVideoFilename + "\""; // Guardamos en temporal

    ffmpegPipe = popen(cmd.c_str(), "w");

    if (!ffmpegPipe) {
        throw std::runtime_error("No se pudo iniciar FFmpeg.");
    }

    // Reservamos algo de memoria para el audio (ej: 10 minutos) para evitar reallocs constantes
    audioMixBuffer.reserve(44100 * 60 * 10); 

    std::cout << "[REC] Grabando video temporal: " << tempVideoFilename << std::endl;
}

Recorder::~Recorder() {
    if (ffmpegPipe) {
        std::cout << "[REC] Finalizando captura de video..." << std::endl;
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;
    }

    // 2. GUARDAR EL AUDIO MIXER A UN WAV
    if (!audioMixBuffer.empty()) {
        std::cout << "[REC] Exportando audio temporal..." << std::endl;
        
        // Convertimos de float (mezcla) a Int16 (WAV estándar) con clamping
        std::vector<sf::Int16> finalSamples;
        finalSamples.reserve(audioMixBuffer.size());

        for (float sample : audioMixBuffer) {
            // Clamping para evitar distorsión digital si el volumen se pasa
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            finalSamples.push_back(static_cast<sf::Int16>(sample));
        }

        sf::OutputSoundFile audioFile;
        if (audioFile.openFromFile(tempAudioFilename, 44100, 1)) { // Mono 44100
            audioFile.write(finalSamples.data(), finalSamples.size());
        }
    } else {
        // Crear un audio mudo mínimo si no hubo sonido para que FFmpeg no falle
        sf::OutputSoundFile audioFile;
        sf::Int16 silence[44100] = {0};
        if (audioFile.openFromFile(tempAudioFilename, 44100, 1)) {
            audioFile.write(silence, 44100); // 1 seg de silencio
        }
    }

    // 3. MERGE FINAL (VIDEO + AUDIO) USANDO FFMPEG
    std::cout << "[REC] Fusionando Audio y Video..." << std::endl;
    
    // Comando: input video + input audio -> copy video stream -> aac audio stream -> output final
    std::string mergeCmd = "ffmpeg -y -i \"" + tempVideoFilename + "\" -i \"" + tempAudioFilename + "\" "
                           "-c:v copy -c:a aac -shortest \"" + finalFilename + "\"";
    
    // Ejecutamos y esperamos
    int result = system(mergeCmd.c_str());

    // 4. LIMPIEZA
    if (result == 0) {
        std::cout << "[REC] VIDEO FINAL LISTO: " << finalFilename << std::endl;
        remove(tempVideoFilename.c_str()); // Borrar temporales
        remove(tempAudioFilename.c_str());
    } else {
        std::cerr << "[REC] Error al fusionar archivos." << std::endl;
    }
}

void Recorder::addFrame(const sf::Window& window) {
    if (!ffmpegPipe || !isRecording) return;

    // Actualizamos el contador de frames para saber el tiempo del audio
    currentFrame++;

    captureTexture.update(window);
    sf::Image img = captureTexture.copyToImage();
    const sf::Uint8* pixels = img.getPixelsPtr();
    fwrite(pixels, 1, width * height * 4, ffmpegPipe);
}

void Recorder::addAudioEvent(const sf::Int16* samples, std::size_t sampleCount, float volume) {
    if (!isRecording) return;

    // Calculamos en qué índice del buffer de audio empieza este sonido
    // Tiempo actual = frames / fps.  Sample actual = Tiempo * SampleRate
    size_t startIndex = (size_t)((double)currentFrame / fps * sampleRate);

    // Aseguramos que el buffer sea lo suficientemente grande
    size_t requiredSize = startIndex + sampleCount;
    if (audioMixBuffer.size() < requiredSize) {
        audioMixBuffer.resize(requiredSize, 0.0f); // Rellenar con silencio si falta
    }

    // MEZCLA ADITIVA (Sumamos las ondas)
    // Convertimos volumen de 0-100 a 0.0-1.0
    float volFactor = volume / 100.0f;

    for (size_t i = 0; i < sampleCount; ++i) {
        audioMixBuffer[startIndex + i] += (float)samples[i] * volFactor;
    }
}