#pragma once
#include "render/mesh.h"
#include <vector>

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/base/memory/unique_ptr.h>

struct ModelAsset
{
  std::string path;
  std::vector<MeshPtr> meshes;
  ozz::unique_ptr<ozz::animation::Skeleton> skeleton;
  std::vector<ozz::unique_ptr<ozz::animation::Animation>> animations;
};

ModelAsset load_model(const char *path);
