#pragma once

// MediaDecoder — decodifica vídeo MP4/BIK via Android MediaCodec NDK.
// Entrega frames RGBA8888 prontos para upload em textura OpenGL ES.
// Requer API >= 24 (NdkMediaExtractor + NdkMediaCodec).

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <memory>
#include <atomic>

namespace aom {
namespace video {

struct VideoFrame {
    std::vector<uint8_t> rgba;   ///< pixels RGBA8888, largura * altura * 4 bytes
    uint32_t width  = 0;
    uint32_t height = 0;
    int64_t  pts_us = 0;         ///< presentation timestamp em microsegundos
};

/// Callback chamado quando um novo frame decodificado está pronto.
/// É chamado na thread do decoder — faça upload de textura na thread GL.
using FrameCallback = std::function<void(const VideoFrame&)>;
using EosCallback   = std::function<void()>;   ///< fim de stream

class MediaDecoder {
public:
    MediaDecoder();
    ~MediaDecoder();

    /// Abre e inicia a decodificação do arquivo em `path`.
    /// Retorna true se o arquivo foi aberto e o codec iniciado com sucesso.
    bool open(const std::string& path);

    /// Abre um vídeo empacotado dentro do APK (assets/).
    /// Tenta fd direto (asset não comprimido) e, se falhar,
    /// copia o asset para `cache_dir` e abre por caminho.
    /// asset_path: ex. "avi/introcg.mp4" ou "assets/avi/introcg.mp4".
    /// cache_dir:  diretório gravável (internalDataPath / filesDir).
    bool open_asset(void* asset_manager,
                    const std::string& asset_path,
                    const std::string& cache_dir);

    /// Avança a decodificação em `dt` segundos.
    /// Quando um frame está pronto, chama `on_frame`.
    /// Quando o stream termina, chama `on_eos`.
    /// Deve ser chamado a cada frame da game loop (thread GL).
    void update(float dt);

    void pause();
    void resume();
    void stop();

    bool is_playing() const;
    bool is_eos()     const;
    float progress()  const;   ///< 0.0 – 1.0
    float duration()  const;   ///< duração total em segundos (0 se desconhecida)

    uint32_t video_width()  const { return width_;  }
    uint32_t video_height() const { return height_; }

    FrameCallback on_frame;
    EosCallback   on_eos;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /// Seleciona a faixa de vídeo, cria e inicia o codec.
    /// O extractor deve estar configurado antes da chamada.
    bool finish_open();

    uint32_t width_  = 0;
    uint32_t height_ = 0;
    float    duration_s_ = 0.0f;
    float    current_s_  = 0.0f;
    bool     playing_    = false;
    bool     eos_        = false;
};

} // namespace video
} // namespace aom
