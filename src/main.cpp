#include <array>
#include <filesystem>
#include <vector>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <print>
#include <span>
#include <glm/glm.hpp>
#include <string>
#include <fstream>

#include "assimp/scene.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

#include "AppState.h"
#include "Exception.h"
#include "Model.h"
#include "Vertex.h"

static std::filesystem::path BasePath;

SDL_GPUShader *LoadShader(
	SDL_GPUDevice *device,
	const std::string &shaderFilename,
	const Uint32 samplerCount,
	const Uint32 uniformBufferCount,
	const Uint32 storageBufferCount,
	const Uint32 storageTextureCount
) {
	SDL_GPUShaderStage stage;
	if (shaderFilename.contains(".vert"))
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	else if (shaderFilename.contains(".frag"))
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	else
		throw std::runtime_error{"Unrecognized shader stage!"};

	std::filesystem::path fullPath;
	const SDL_GPUShaderFormat backendFormats{SDL_GetGPUShaderFormats(device)};
	SDL_GPUShaderFormat format{};
	std::string entrypoint{"main"};

	if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
		fullPath = BasePath / "Content/Shaders/Compiled/SPIRV" / (shaderFilename + ".spv");
		format = SDL_GPU_SHADERFORMAT_SPIRV;
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
		fullPath = BasePath / "Content/Shaders/Compiled/MSL" / (shaderFilename + ".msl");
		format = SDL_GPU_SHADERFORMAT_MSL;
		entrypoint = "main0";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
		fullPath = BasePath / "Content/Shaders/Compiled/DXIL" / (shaderFilename + ".dxil");
		format = SDL_GPU_SHADERFORMAT_DXIL;
	} else throw std::runtime_error{"No supported shader formats available"};

	std::ifstream file{fullPath, std::ios::binary};
	if (!file)
		throw std::runtime_error{"Couldn't open shader file"};
	std::vector<Uint8> code{std::istreambuf_iterator(file), {}};

	const SDL_GPUShaderCreateInfo shaderInfo{
		.code_size = code.size(),
		.code = code.data(),
		.entrypoint = entrypoint.c_str(),
		.format = format,
		.stage = stage,
		.num_samplers = samplerCount,
		.num_storage_textures = storageTextureCount,
		.num_storage_buffers = storageBufferCount,
		.num_uniform_buffers = uniformBufferCount
	};

	return SDL_CreateGPUShader(device, &shaderInfo);
}

SDL_Surface *LoadImage(const std::string_view imageFilename, const int desiredChannels) {
	const auto fullPath{BasePath / "Content/Images" / imageFilename};
	SDL_PixelFormat format;

	auto result{IMG_Load(fullPath.string().c_str())};
	if (!result)
		throw SDLException{"Couldn't load image"};

	if (desiredChannels == 4)
		format = SDL_PIXELFORMAT_ABGR8888;
	else {
		SDL_DestroySurface(result);
		throw std::runtime_error{"Unsupported number of channels"};
	}
	if (result->format != format) {
		SDL_Surface *next = SDL_ConvertSurface(result, format);
		SDL_DestroySurface(result);
		result = next;
	}

	return result;
}

void CreateMsaaTexture(AppState *const context) {
	const SDL_GPUTextureCreateInfo msaaTextureCreateInfo{
		.format =
		    SDL_GetGPUSwapchainTextureFormat(context->device, context->window),
		.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
		.width = static_cast<Uint32>(context->GetWindowWidth()),
		.height = static_cast<Uint32>(context->GetWindowHeight()),
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.sample_count = context->sampleCount,
	    };
	context->msaaTexture =
	    SDL_CreateGPUTexture(context->device, &msaaTextureCreateInfo);
	if (!context->msaaTexture)
		throw SDLException{"Couldn't create GPU texture"};
	SDL_SetGPUTextureName(context->device, context->msaaTexture, "MSAA Texture");
}

void CreateDepthStencilTexture(AppState *const context) {
	const SDL_GPUTextureCreateInfo depthStencilTextureCreateInfo{
		.format = context->depthStencilFormat,
		.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
		.width = static_cast<Uint32>(context->GetWindowWidth()),
		.height = static_cast<Uint32>(context->GetWindowHeight()),
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.sample_count = context->sampleCount,
	    };
	context->depthStencilTexture =
	    SDL_CreateGPUTexture(context->device, &depthStencilTextureCreateInfo);
	if (!context->depthStencilTexture)
		throw SDLException{"Couldn't create GPU texture"};
	SDL_SetGPUTextureName(context->device, context->depthStencilTexture,
			      "Depth Stencil Texture");
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO))
    throw SDLException{"Couldn't initialize SDL"};

  BasePath = SDL_GetBasePath();

  const auto context = new AppState();
  *appstate = context;

  context->window = SDL_CreateWindow("Codotaku Game Engine", 800, 600,
                                     SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
  if (!context->window)
    throw SDLException{"Couldn't create window"};

  context->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV |
                                            SDL_GPU_SHADERFORMAT_MSL |
                                            SDL_GPU_SHADERFORMAT_DXIL,
                                        true, nullptr);
  if (!context->device)
    throw SDLException{"Couldn't create GPU device"};

  std::println("Using GPU device driver: {}",
               SDL_GetGPUDeviceDriver(context->device));

  if (!SDL_ClaimWindowForGPUDevice(context->device, context->window))
    throw SDLException{"Couldn't claim window for GPU device"};

  if (SDL_GPUTextureSupportsFormat(
          context->device, SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
          SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
    context->depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
  else if (SDL_GPUTextureSupportsFormat(
               context->device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
               SDL_GPU_TEXTURETYPE_2D,
               SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
    context->depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
  else
    throw SDLException{"Couldn't find a suitable depth stencil format"};

  auto vertexShader{
      LoadShader(context->device, "TexturedQuadWithMatrix.vert", 0, 1, 0, 0)};
  if (!vertexShader)
    throw SDLException{"Couldn't load vertex shader"};

  auto fragmentShader{
      LoadShader(context->device, "TexturedQuad.frag", 1, 0, 0, 0)};
  if (!fragmentShader)
    throw SDLException{"Couldn't load fragment shader"};

  std::array colorTargetDescriptions{
      SDL_GPUColorTargetDescription{
          .format = SDL_GetGPUSwapchainTextureFormat(context->device,
                                                     context->window),
      },
  };
  std::array<SDL_GPUVertexAttribute, 2> vertexAttributes{
      {
          {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
           offsetof(Vertex, position)},
          {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, uv)},
      },
  };
  std::array<SDL_GPUVertexBufferDescription, 1> vertexBufferDescriptions{
      {
          {0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0},
      },
  };
  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo{
      .vertex_shader = vertexShader,
      .fragment_shader = fragmentShader,
      .vertex_input_state =
          {
              .vertex_buffer_descriptions = vertexBufferDescriptions.data(),
              .num_vertex_buffers = vertexBufferDescriptions.size(),
              .vertex_attributes = vertexAttributes.data(),
              .num_vertex_attributes = vertexAttributes.size(),
          },
      .multisample_state =
          {
              .sample_count = context->sampleCount,
          },
      .depth_stencil_state =
          {
              .compare_op = SDL_GPU_COMPAREOP_LESS,
              .enable_depth_test = true,
              .enable_depth_write = true,
          },
      .target_info =
          {
              .color_target_descriptions = colorTargetDescriptions.data(),
              .num_color_targets = colorTargetDescriptions.size(),
              .depth_stencil_format = context->depthStencilFormat,
              .has_depth_stencil_target = true,
          },
  };
  auto pipeline{
      SDL_CreateGPUGraphicsPipeline(context->device, &pipelineCreateInfo)};
  if (!pipeline)
    throw SDLException{"Couldn't create GPU graphics pipeline"};
  context->pipeline = pipeline;

  CreateMsaaTexture(context);
  CreateDepthStencilTexture(context);

  SDL_ReleaseGPUShader(context->device, vertexShader);
  SDL_ReleaseGPUShader(context->device, fragmentShader);

  SDL_GPUSamplerCreateInfo samplerCreateInfo{
      .min_filter = SDL_GPU_FILTER_LINEAR,
      .mag_filter = SDL_GPU_FILTER_LINEAR,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  };
  context->sampler = SDL_CreateGPUSampler(context->device, &samplerCreateInfo);

  SDL_Surface *imageData{LoadImage("viking_room.png", 4)};

  SDL_GPUTextureCreateInfo textureCreateInfo{
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = static_cast<Uint32>(imageData->w),
      .height = static_cast<Uint32>(imageData->h),
      .layer_count_or_depth = 1,
      .num_levels = 1,
  };
  context->texture = SDL_CreateGPUTexture(context->device, &textureCreateInfo);
  if (!context->texture)
    throw SDLException{"Couldn't create GPU texture"};

  SDL_SetGPUTextureName(context->device, context->texture, "viking_room.png");

  context->model = Model();

  SDL_GPUBufferCreateInfo vertexBufferCreateInfo{
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size =
          static_cast<Uint32>(context->model.vertices.size() * sizeof(Vertex)),
  };
  context->vertexBuffer =
      SDL_CreateGPUBuffer(context->device, &vertexBufferCreateInfo);
  if (!context->vertexBuffer)
    throw SDLException{"Couldn't create GPU buffer"};
  SDL_SetGPUBufferName(context->device, context->vertexBuffer, "Vertex Buffer");

  SDL_GPUBufferCreateInfo indexBufferCreateInfo{
      .usage = SDL_GPU_BUFFERUSAGE_INDEX,
      .size =
          static_cast<Uint32>(context->model.indices.size() * sizeof(Uint32)),
  };
  context->indexBuffer =
      SDL_CreateGPUBuffer(context->device, &indexBufferCreateInfo);
  if (!context->indexBuffer)
    throw SDLException{"Couldn't create GPU buffer"};
  SDL_SetGPUBufferName(context->device, context->indexBuffer, "Index Buffer");

  SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = vertexBufferCreateInfo.size + indexBufferCreateInfo.size,
  };
  auto transferBuffer{
      SDL_CreateGPUTransferBuffer(context->device, &transferBufferCreateInfo)};
  if (!transferBuffer)
    throw SDLException{"Couldn't create transfer buffer"};

  auto transferBufferDataPtr{static_cast<Uint8 *>(
      SDL_MapGPUTransferBuffer(context->device, transferBuffer, false))};
  if (!transferBufferDataPtr)
    throw SDLException{"Couldn't map transfer buffer"};

  std::span vertexBufferData{reinterpret_cast<Vertex *>(transferBufferDataPtr),
                             context->model.vertices.size()};
  std::ranges::copy(context->model.vertices, vertexBufferData.begin());

  std::span indexBufferData{
      reinterpret_cast<Uint32 *>(transferBufferDataPtr +
                                 vertexBufferCreateInfo.size),
      context->model.indices.size()};
  std::ranges::copy(context->model.indices, indexBufferData.begin());

  SDL_UnmapGPUTransferBuffer(context->device, transferBuffer);

  SDL_GPUTransferBufferCreateInfo textureTransferBufferCreateInfo{
      .size = static_cast<Uint32>(imageData->pitch * imageData->h),
  };
  auto textureTransferBuffer{SDL_CreateGPUTransferBuffer(
      context->device, &textureTransferBufferCreateInfo)};
  if (!textureTransferBuffer)
    throw SDLException{"Couldn't create transfer buffer"};

  auto textureTransferBufferDataPtr{static_cast<Uint8 *>(
      SDL_MapGPUTransferBuffer(context->device, textureTransferBuffer, false))};
  if (!textureTransferBufferDataPtr)
    throw SDLException{"Couldn't map transfer buffer"};

  std::span textureDataSpan{textureTransferBufferDataPtr,
                            textureTransferBufferCreateInfo.size};
  std::span imagePixels{static_cast<Uint8 *>(imageData->pixels),
                        static_cast<size_t>(imageData->pitch * imageData->h)};
  std::ranges::copy(imagePixels, textureDataSpan.begin());

  SDL_UnmapGPUTransferBuffer(context->device, textureTransferBuffer);

  auto transferCommandBuffer{SDL_AcquireGPUCommandBuffer(context->device)};
  if (!transferCommandBuffer)
    throw SDLException{"Couldn't acquire GPU command buffer"};

  auto copyPass{SDL_BeginGPUCopyPass(transferCommandBuffer)};

  SDL_GPUTransferBufferLocation source{
      .transfer_buffer = transferBuffer,
  };
  SDL_GPUBufferRegion destination{
      .buffer = context->vertexBuffer,
      .size = vertexBufferCreateInfo.size,
  };
  SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

  source.offset = vertexBufferCreateInfo.size;
  destination.buffer = context->indexBuffer;
  destination.size = indexBufferCreateInfo.size;
  SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

  SDL_GPUTextureTransferInfo textureTransferInfo{
      .transfer_buffer = textureTransferBuffer,
  };

  SDL_GPUTextureRegion textureRegion{.texture = context->texture,
                                     .w = static_cast<Uint32>(imageData->w),
                                     .h = static_cast<Uint32>(imageData->h),
                                     .d = 1};
  SDL_UploadToGPUTexture(copyPass, &textureTransferInfo, &textureRegion, false);

  SDL_EndGPUCopyPass(copyPass);

  if (!SDL_SubmitGPUCommandBuffer(transferCommandBuffer))
    throw SDLException{"Couldn't submit GPU command buffer"};

  SDL_ReleaseGPUTransferBuffer(context->device, transferBuffer);
  SDL_ReleaseGPUTransferBuffer(context->device, textureTransferBuffer);

  SDL_DestroySurface(imageData);

  SDL_ShowWindow(context->window);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
	const auto context = static_cast<AppState*>(appstate);

	switch (event->type) {
	case SDL_EVENT_QUIT:
		return SDL_APP_SUCCESS;

	case SDL_EVENT_WINDOW_RESIZED: {
		SDL_ReleaseGPUTexture(context->device, context->msaaTexture);
		CreateMsaaTexture(context);

		// depth stencil texture
		SDL_ReleaseGPUTexture(context->device, context->depthStencilTexture);
		CreateDepthStencilTexture(context);
	}
		return SDL_APP_CONTINUE;

	default:
		return SDL_APP_CONTINUE;
	}
}

SDL_AppResult SDL_AppIterate(void* appstate) {
	const auto context = static_cast<AppState*>(appstate);

	auto ticks{SDL_GetTicks()};

	auto commandBuffer{SDL_AcquireGPUCommandBuffer(context->device)};
	if (!commandBuffer)
		throw SDLException{"Couldn't acquire GPU command buffer"};

	SDL_GPUTexture *swapchainTexture;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, context->window, &swapchainTexture, nullptr, nullptr))
		throw SDLException{"Couldn't acquire swapchain texture"};

	if (swapchainTexture) {
		const std::array colorTargets{
			SDL_GPUColorTargetInfo{
				.texture = context->msaaTexture,
				.clear_color = SDL_FColor{0.1f, 0.1f, 0.1f, 1.0f},
				.load_op = SDL_GPU_LOADOP_CLEAR,
				.store_op = SDL_GPU_STOREOP_RESOLVE,
				.resolve_texture = swapchainTexture,
			}
		};
		SDL_GPUDepthStencilTargetInfo depthStencilTarget{
			.texture = context->depthStencilTexture,
			.clear_depth = 1.0f,
			.load_op = SDL_GPU_LOADOP_CLEAR,
		};
		auto renderPass{
			SDL_BeginGPURenderPass(commandBuffer, colorTargets.data(), colorTargets.size(), &depthStencilTarget)
		};

		SDL_BindGPUGraphicsPipeline(renderPass, context->pipeline);

                const std::array<SDL_GPUBufferBinding, 1> bindings{{context->vertexBuffer, 0}};
		SDL_BindGPUVertexBuffers(renderPass, 0, bindings.data(), bindings.size());

		const SDL_GPUBufferBinding indexBufferBinding{context->indexBuffer, 0};
		SDL_BindGPUIndexBuffer(renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		std::array<SDL_GPUTextureSamplerBinding, 1> textureSamplerBindings{{context->texture, context->sampler}};
		SDL_BindGPUFragmentSamplers(renderPass, 0, textureSamplerBindings.data(), textureSamplerBindings.size());

		auto projectionMatrix{glm::perspective(glm::radians(45.0f), context->GetWindowAspectRatio(), 0.1f, 100.0f)};
		auto viewMatrix{
			lookAt(glm::vec3{0.0f, 0.0f, 2.0f}, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f})
		};
		auto modelMatrix{glm::mat4{1.0f}};
		modelMatrix = rotate(modelMatrix, glm::radians(static_cast<float>(ticks) * 0.1f),
		                     glm::vec3{0.0f, 1.0f, 1.0f});
		auto projectionViewMatrix{projectionMatrix * viewMatrix};
		auto modelViewProjectionMatrix{projectionViewMatrix * modelMatrix};
		SDL_PushGPUVertexUniformData(commandBuffer, 0, &modelViewProjectionMatrix,
		                             sizeof(modelViewProjectionMatrix));

		SDL_DrawGPUIndexedPrimitives(renderPass, context->model.indices.size(), 1, 0, 0, 0);

		SDL_EndGPURenderPass(renderPass);
	}

	if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
		throw SDLException{"Couldn't submit GPU command buffer"};

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
}
