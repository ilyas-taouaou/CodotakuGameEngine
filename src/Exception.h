#pragma once

#include <string>
#include <stdexcept>
#include <SDL3/SDL.h>

class SDLException final : public std::runtime_error {
public:
  explicit SDLException(const std::string &message)
      : std::runtime_error(message + '\n' + SDL_GetError()) {}
};
