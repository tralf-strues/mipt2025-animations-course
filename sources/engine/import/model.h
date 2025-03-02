#pragma once
#include "render/mesh.h"
#include <vector>

struct SkeletonAsset
{
  static const int32_t NULL_PARENT = -1;

  std::vector<std::string> names;
  std::vector<glm::mat4> localTransforms;
  std::vector<int32_t> parentIndices;

  std::vector<int32_t> hierarchyDepths; // only for ui
};

struct ModelAsset
{
  std::string path;
  std::vector<MeshPtr> meshes;
  SkeletonAsset skeletonAsset;
};

ModelAsset load_model(const char *path);
