#include "scene.h"

void application_update(Scene &scene)
{
  if (scene.activeControllerIdx)
  {
    scene.controllers[scene.activeControllerIdx.value()]->onUpdate(engine::get_delta_time());
  }
  else
  {
    arcball_camera_update(
      scene.userCamera.arcballCamera,
      scene.userCamera.transform,
      engine::get_delta_time());
  }

  for (Character &character : scene.characters)
  {
    character.animationContext.advance(
      character.transform,
      engine::get_delta_time());
  }
}