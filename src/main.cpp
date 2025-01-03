#include <fstream>
#include <stdexcept>
#include <vector>
#include <SDL3/SDL.h>
#include <print>
#include <span>
#include <glm/glm.hpp>
#include <SDL3_image/SDL_image.h>

class SDLException final : public std::runtime_error {
public:
	explicit SDLException(const std::string &message) : std::runtime_error(message + '\n' + SDL_GetError()) {
	}
};

static std::string BasePath{};

void InitializeAssetLoader() {
	BasePath = std::string{SDL_GetBasePath()};
}

SDL_GPUShader *LoadShader(
	SDL_GPUDevice *device,
	const std::string &shaderFilename,
	const Uint32 samplerCount,
	const Uint32 uniformBufferCount,
	const Uint32 storageBufferCount,
	const Uint32 storageTextureCount
) {
	// Auto-detect the shader stage from the file name for convenience
	SDL_GPUShaderStage stage;
	if (shaderFilename.contains(".vert"))
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	else if (shaderFilename.contains(".frag"))
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	else
		throw std::runtime_error{"Unrecognized shader stage!"};

	std::string fullPath;
	const SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
	SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
	const char *entrypoint;

	if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
		fullPath = std::format("{}/Content/Shaders/Compiled/SPIRV/{}.spv", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_SPIRV;
		entrypoint = "main";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
		fullPath = std::format("{}/Content/Shaders/Compiled/MSL/{}.msl", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_MSL;
		entrypoint = "main0";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
		fullPath = std::format("{}/Content/Shaders/Compiled/DXIL/{}.dxil", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_DXIL;
		entrypoint = "main";
	} else throw std::runtime_error{"No supported shader formats available"};

	std::ifstream file{fullPath, std::ios::binary};
	if (!file)
		throw std::runtime_error{"Couldn't open shader file"};
	std::vector<Uint8> code{std::istreambuf_iterator(file), {}};

	SDL_GPUShaderCreateInfo shaderInfo{};
	shaderInfo.code = code.data();
	shaderInfo.code_size = code.size();
	shaderInfo.entrypoint = entrypoint;
	shaderInfo.format = format;
	shaderInfo.stage = stage;
	shaderInfo.num_samplers = samplerCount;
	shaderInfo.num_uniform_buffers = uniformBufferCount;
	shaderInfo.num_storage_buffers = storageBufferCount;
	shaderInfo.num_storage_textures = storageTextureCount;

	SDL_GPUShader *shader = SDL_CreateGPUShader(device, &shaderInfo);
	if (!shader)
		throw SDLException{"Couldn't create GPU shader"};

	return shader;
}

SDL_Surface *LoadImage(const char *imageFilename, const int desiredChannels) {
	const std::string fullPath = std::format("{}/Content/Images/{}", BasePath, imageFilename);
	SDL_PixelFormat format;

	SDL_Surface *result = IMG_Load(fullPath.c_str());
	if (!result)
		throw SDLException{"Couldn't load image"};

	if (desiredChannels == 4) {
		format = SDL_PIXELFORMAT_ABGR8888;
	} else {
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

struct Vertex {
	glm::vec3 position;
	glm::vec2 uv;
};


int main() {
	// Initialize SDL
	if (!SDL_Init(SDL_INIT_VIDEO))
		throw SDLException{"Couldn't initialize SDL"};

	InitializeAssetLoader();

	// Create window
	SDL_Window *window{SDL_CreateWindow("Codotaku Game Engine", 800, 600, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE)};
	if (!window)
		throw SDLException{"Couldn't create window"};

	// Create GPU logical device
	SDL_GPUDevice *device{
		SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXIL, true,
		                    nullptr)
	};
	if (!device)
		throw SDLException{"Couldn't create GPU device"};

	std::println("Using GPU device driver: {}", SDL_GetGPUDeviceDriver(device));

	// Claim window for GPU device
	if (!SDL_ClaimWindowForGPUDevice(device, window))
		throw SDLException{"Couldn't claim window for GPU device"};

	SDL_GPUShader *vertexShader{LoadShader(device, "TexturedQuad.vert", 0, 0, 0, 0)};
	if (!vertexShader)
		throw SDLException{"Couldn't load vertex shader"};

	SDL_GPUShader *fragmentShader{LoadShader(device, "TexturedQuad.frag", 1, 0, 0, 0)};
	if (!fragmentShader)
		throw SDLException{"Couldn't load fragment shader"};

	SDL_Surface *image_data{
		LoadImage("screenshot.png", 4)
	};

	SDL_GPUColorTargetDescription colorTargetDescription{};
	colorTargetDescription.format = SDL_GetGPUSwapchainTextureFormat(device, window);
	std::vector colorTargetDescriptions{colorTargetDescription};

	SDL_GPUGraphicsPipelineTargetInfo targetInfo{};
	targetInfo.color_target_descriptions = colorTargetDescriptions.data();
	targetInfo.num_color_targets = colorTargetDescriptions.size();

	std::vector<SDL_GPUVertexAttribute> vertexAttributes{};
	std::vector<SDL_GPUVertexBufferDescription> vertexBufferDescriptions{};

	vertexAttributes.emplace_back(0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, position));
	vertexAttributes.emplace_back(1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, uv));

	vertexBufferDescriptions.emplace_back(0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0);

	SDL_GPUVertexInputState vertexInputState{};
	vertexInputState.vertex_attributes = vertexAttributes.data();
	vertexInputState.num_vertex_attributes = vertexAttributes.size();
	vertexInputState.vertex_buffer_descriptions = vertexBufferDescriptions.data();
	vertexInputState.num_vertex_buffers = vertexBufferDescriptions.size();

	SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.vertex_shader = vertexShader;
	pipelineCreateInfo.fragment_shader = fragmentShader;
	pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipelineCreateInfo.target_info = targetInfo;
	pipelineCreateInfo.vertex_input_state = vertexInputState;

	SDL_GPUGraphicsPipeline *pipeline{SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo)};
	if (!pipeline)
		throw SDLException{"Couldn't create GPU graphics pipeline"};

	SDL_ReleaseGPUShader(device, vertexShader);
	SDL_ReleaseGPUShader(device, fragmentShader);

	SDL_GPUSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.min_filter = SDL_GPU_FILTER_LINEAR;
	samplerCreateInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
	samplerCreateInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
	samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

	auto sampler{
		SDL_CreateGPUSampler(device, &samplerCreateInfo)
	};

	SDL_GPUTextureCreateInfo textureCreateInfo{};
	textureCreateInfo.type = SDL_GPU_TEXTURETYPE_2D;
	textureCreateInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	textureCreateInfo.width = image_data->w;
	textureCreateInfo.height = image_data->h;
	textureCreateInfo.layer_count_or_depth = 1;
	textureCreateInfo.num_levels = 1;
	textureCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	auto texture{SDL_CreateGPUTexture(device, &textureCreateInfo)};
	if (!texture)
		throw SDLException{"Couldn't create GPU texture"};
	SDL_SetGPUTextureName(device, texture, "screenshot.png");

	// Rect
	std::vector<Vertex> vertices{
		{{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}},
		{{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}},
		{{0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}},
		{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}},
	};

	std::vector<Uint32> indices{
		0, 1, 2,
		0, 2, 3,
	};

	SDL_GPUBufferCreateInfo vertexBufferCreateInfo{};
	vertexBufferCreateInfo.size = vertices.size() * sizeof(Vertex);
	vertexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
	auto vertexBuffer{SDL_CreateGPUBuffer(device, &vertexBufferCreateInfo)};
	if (!vertexBuffer)
		throw SDLException{"Couldn't create GPU buffer"};
	SDL_SetGPUBufferName(device, vertexBuffer, "Vertex Buffer");

	SDL_GPUBufferCreateInfo indexBufferCreateInfo{};
	indexBufferCreateInfo.size = indices.size() * sizeof(Uint32);
	indexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
	auto indexBuffer{SDL_CreateGPUBuffer(device, &indexBufferCreateInfo)};
	if (!indexBuffer)
		throw SDLException{"Couldn't create GPU buffer"};
	SDL_SetGPUBufferName(device, indexBuffer, "Index Buffer");

	SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo{};
	transferBufferCreateInfo.size = vertexBufferCreateInfo.size + indexBufferCreateInfo.size;
	transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	auto transferBuffer{SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo)};
	if (!transferBuffer)
		throw SDLException{"Couldn't create transfer buffer"};

	auto transferBufferDataPtr{static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(device, transferBuffer, false))};
	if (!transferBufferDataPtr)
		throw SDLException{"Couldn't map transfer buffer"};

	// Vertex buffer data span
	std::span vertexBufferData{reinterpret_cast<Vertex *>(transferBufferDataPtr), vertices.size()};
	std::ranges::copy(vertices, vertexBufferData.begin());

	// Index buffer data span
	std::span indexBufferData{
		reinterpret_cast<Uint32 *>(transferBufferDataPtr + vertexBufferCreateInfo.size), indices.size()
	};
	std::ranges::copy(indices, indexBufferData.begin());

	SDL_UnmapGPUTransferBuffer(device, transferBuffer);

	SDL_GPUTransferBufferCreateInfo textureTransferBufferCreateInfo{};
	textureTransferBufferCreateInfo.size = image_data->pitch * image_data->h;
	textureTransferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	auto textureTransferBuffer{SDL_CreateGPUTransferBuffer(device, &textureTransferBufferCreateInfo)};
	if (!textureTransferBuffer)
		throw SDLException{"Couldn't create transfer buffer"};

	auto textureTransferBufferDataPtr{
		static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(device, textureTransferBuffer, false))
	};
	if (!textureTransferBufferDataPtr)
		throw SDLException{"Couldn't map transfer buffer"};

	std::memcpy(textureTransferBufferDataPtr, image_data->pixels, textureTransferBufferCreateInfo.size);

	SDL_UnmapGPUTransferBuffer(device, textureTransferBuffer);

	SDL_GPUCommandBuffer *transferCommandBuffer{SDL_AcquireGPUCommandBuffer(device)};
	if (!transferCommandBuffer)
		throw SDLException{"Couldn't acquire GPU command buffer"};
	SDL_GPUCopyPass *copyPass{SDL_BeginGPUCopyPass(transferCommandBuffer)};

	SDL_GPUTransferBufferLocation source{};
	source.transfer_buffer = transferBuffer;
	source.offset = 0;
	SDL_GPUBufferRegion destination{};
	destination.buffer = vertexBuffer;
	destination.size = vertexBufferCreateInfo.size;
	SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

	source.offset = vertexBufferCreateInfo.size;
	destination.buffer = indexBuffer;
	destination.size = indexBufferCreateInfo.size;
	SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

	SDL_GPUTextureTransferInfo textureTransferInfo{};
	textureTransferInfo.transfer_buffer = textureTransferBuffer;

	SDL_GPUTextureRegion textureRegion{};
	textureRegion.texture = texture;
	textureRegion.w = image_data->w;
	textureRegion.h = image_data->h;
	textureRegion.d = 1;

	SDL_UploadToGPUTexture(copyPass, &textureTransferInfo, &textureRegion, false);

	SDL_EndGPUCopyPass(copyPass);
	if (!SDL_SubmitGPUCommandBuffer(transferCommandBuffer))
		throw SDLException{"Couldn't submit GPU command buffer"};
	SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

	// Show window
	SDL_ShowWindow(window);

	bool isRunning{true};
	SDL_Event event;

	// Main loop
	while (isRunning) {
		// Event loop
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_EVENT_QUIT:
					isRunning = false;
				default: break;
			}
		}

		SDL_GPUCommandBuffer *commandBuffer{SDL_AcquireGPUCommandBuffer(device)};
		if (!commandBuffer)
			throw SDLException{"Couldn't acquire GPU command buffer"};

		SDL_GPUTexture *swapchainTexture;
		SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, window, &swapchainTexture, nullptr, nullptr);
		if (swapchainTexture) {
			SDL_GPUColorTargetInfo colorTarget{};
			colorTarget.texture = swapchainTexture;
			colorTarget.store_op = SDL_GPU_STOREOP_STORE;
			colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
			colorTarget.clear_color = SDL_FColor{0.1f, 0.1f, 0.1f, 1.0f};
			std::vector colorTargets{colorTarget};
			SDL_GPURenderPass *renderPass{
				SDL_BeginGPURenderPass(commandBuffer, colorTargets.data(), colorTargets.size(), nullptr)
			};
			SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
			std::vector<SDL_GPUBufferBinding> bindings{{vertexBuffer, 0}};
			SDL_BindGPUVertexBuffers(renderPass, 0, bindings.data(), bindings.size());
			SDL_GPUBufferBinding indexBufferBinding{indexBuffer, 0};
			SDL_BindGPUIndexBuffer(renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

			SDL_GPUTextureSamplerBinding textureSamplerBinding{};
			textureSamplerBinding.texture = texture;
			textureSamplerBinding.sampler = sampler;
			std::vector textureSamplerBindings{textureSamplerBinding};
			SDL_BindGPUFragmentSamplers(renderPass, 0, textureSamplerBindings.data(), textureSamplerBindings.size());

			SDL_DrawGPUIndexedPrimitives(renderPass, indices.size(), 1, 0, 0, 0);
			SDL_EndGPURenderPass(renderPass);
		}

		if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
			throw SDLException{"Couldn't submit GPU command buffer"};
	}

	return EXIT_SUCCESS;
}
