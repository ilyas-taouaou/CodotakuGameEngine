#include "AppState.h"

#include "Exception.h"

int AppState::GetWindowWidth() {
  if (!SDL_GetWindowSize(window, &m_windowWidth, &m_windowHeight))
    throw SDLException{"Couldn't get window size"};

  return m_windowWidth;
}

int AppState::GetWindowHeight() {
  if (!SDL_GetWindowSize(window, &m_windowWidth, &m_windowHeight))
    throw SDLException{"Couldn't get window size"};

  return m_windowHeight;
}

float AppState::GetWindowAspectRatio() {
  if (!SDL_GetWindowSize(window, &m_windowWidth, &m_windowHeight))
    throw SDLException{"Couldn't get window size"};

  return static_cast<float>(GetWindowWidth()) / static_cast<float>(GetWindowHeight());
}
