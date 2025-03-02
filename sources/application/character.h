#pragma once
#include "engine/3dmath.h"
#include "engine/render/material.h"
#include "engine/render/mesh.h"

struct SkeletonRuntime
{
  static const int32_t NULL_PARENT = -1;

  std::vector<glm::mat4> localTransforms;
  std::vector<glm::mat4> modelTransforms;
  std::vector<int32_t> parentIndices;

  SkeletonRuntime(const SkeletonAsset &asset)
    : localTransforms(asset.localTransforms),
      modelTransforms(localTransforms.size(), glm::mat4(1.0f)),
      parentIndices(asset.parentIndices)
  {
    forward_kinematics(asset);
  }

  void forward_kinematics(const SkeletonAsset &asset)
  {
    for (size_t i = 0; i < asset.parentIndices.size(); i++)
    {
      int32_t parentIndex = asset.parentIndices[i];

      if (parentIndex != NULL_PARENT)
        modelTransforms[i] = modelTransforms[parentIndex] * localTransforms[i];
      else
        modelTransforms[i] = localTransforms[i];
    }
  }
};

struct Character
{
  std::string name;
  glm::mat4 transform;
  std::vector<MeshPtr> meshes;
  MaterialPtr material;
  SkeletonRuntime skeleton;
};
