#include "Model.h"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include <glm/glm.hpp>

Model::Model() {
  Assimp::Importer importer;
  const auto *scene{importer.ReadFile("Content/Models/viking_room.obj",
                                      aiProcess_Triangulate)};
  if (!scene)
    throw std::runtime_error{"Couldn't load model"};

  for (size_t i{}; i < scene->mNumMeshes; ++i) {
    const auto *mesh{scene->mMeshes[i]};
    for (size_t j{}; j < mesh->mNumVertices; ++j) {
      const auto &vertex{mesh->mVertices[j]};
      vertices.push_back({
          .position = glm::vec3{vertex.x, vertex.y, vertex.z},
          .uv = mesh->mTextureCoords[0]
                    ? glm::vec2{mesh->mTextureCoords[0][j].x,
                                mesh->mTextureCoords[0][j].y}
                    : glm::vec2{0.0f, 0.0f},
      });
    }
    for (size_t j{}; j < mesh->mNumFaces; ++j) {
      const auto &face{mesh->mFaces[j]};
      for (size_t k{}; k < face.mNumIndices; ++k)
        indices.push_back(face.mIndices[k]);
    }
  }
}
