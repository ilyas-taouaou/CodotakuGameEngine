#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <stdexcept>
#include <fstream>
#include <SDL3_image/SDL_image.h>

class SDLException final : public std::runtime_error {
public:
	explicit SDLException(const std::string &message) : std::runtime_error(message + '\n' + SDL_GetError()) {
	}
};

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
