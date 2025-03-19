#pragma once

#include <optional>

#include "engine/render/direction_light.h"
#include "engine/import/model.h"
#include "user_camera.h"
#include "character.h"
#include "static_object.h"
#include "third_person_controller.h"

struct Scene
{
  std::vector<ModelAsset> models;
  DirectionLight light;

  UserCamera userCamera;

  std::optional<std::size_t> activeControllerIdx = std::nullopt;
  std::vector<std::unique_ptr<ThirdPersonController>> controllers;

  std::vector<Character> characters;
  std::vector<StaticObject> staticObjects;
};