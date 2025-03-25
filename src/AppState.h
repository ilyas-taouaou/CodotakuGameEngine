#pragma once

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_video.h>
#include "Model.h"

class AppState {
public:
  int GetWindowWidth();
  int GetWindowHeight();
  float GetWindowAspectRatio();

  SDL_Window *window{nullptr};
  SDL_GPUDevice *device{nullptr};

  SDL_GPUGraphicsPipeline *pipeline{nullptr};
  SDL_GPUSampleCount sampleCount{SDL_GPU_SAMPLECOUNT_4};
  SDL_GPUTexture *texture{nullptr};
  SDL_GPUTexture *msaaTexture{nullptr};
  SDL_GPUTextureFormat depthStencilFormat{};
  SDL_GPUTexture *depthStencilTexture{nullptr};
  SDL_GPUBuffer *vertexBuffer{nullptr};
  SDL_GPUBuffer *indexBuffer{nullptr};
  SDL_GPUSampler *sampler{nullptr};
  Model model;

private:
  void ReadWindowSize();

  int m_windowWidth{0};
  int m_windowHeight{0};
};
