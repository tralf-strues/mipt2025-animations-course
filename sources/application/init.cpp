#include "scene.h"


static glm::mat4 get_projective_matrix()
{
  const float fovY = 90.f * DegToRad;
  const float zNear = 0.01f;
  const float zFar = 500.f;
  return glm::perspective(fovY, engine::get_aspect_ratio(), zNear, zFar);
}

void application_init(Scene &scene)
{
  scene.light.lightDirection = glm::normalize(glm::vec3(-1, -1, 0));
  scene.light.lightColor = glm::vec3(1.f);
  scene.light.ambient = glm::vec3(0.2f);
  scene.userCamera.projection = get_projective_matrix();

  engine::onWindowResizedEvent += [&](const std::pair<int, int> &) { scene.userCamera.projection = get_projective_matrix(); };

  ArcballCamera &cam = scene.userCamera.arcballCamera;
  cam.curZoom = cam.targetZoom = 0.5f;
  cam.maxdistance = 5.f;
  cam.distance = cam.curZoom * cam.maxdistance;
  cam.lerpStrength = 10.f;
  cam.mouseSensitivity = 0.5f;
  cam.wheelSensitivity = 0.05f;
  cam.targetPosition = glm::vec3(0.f, 1.f, 0.f);
  cam.targetRotation = cam.curRotation = glm::vec2(DegToRad * -90.f, DegToRad * -30.f);
  cam.rotationEnable = false;

  scene.userCamera.transform = calculate_transform(scene.userCamera.arcballCamera);

  engine::onMouseButtonEvent += [&](const SDL_MouseButtonEvent &e) {
    if (scene.activeControllerIdx && e.button == SDL_BUTTON_RIGHT && e.state == SDL_RELEASED)
    {
      scene.activeControllerIdx.reset();
      // SDL_ShowCursor(SDL_ENABLE);
      SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    else if (!scene.activeControllerIdx)
    {
      arccam_mouse_click_handler(e, scene.userCamera.arcballCamera);
    }
  };

  engine::onMouseMotionEvent += [&](const SDL_MouseMotionEvent &e) {
    if (scene.activeControllerIdx)
      scene.controllers[scene.activeControllerIdx.value()]->onMouseMotionEvent(e);
    else
      arccam_mouse_move_handler(e, scene.userCamera.arcballCamera, scene.userCamera.transform);
  };

  engine::onMouseWheelEvent += [&](const SDL_MouseWheelEvent &e) {
    if (!scene.activeControllerIdx)
      arccam_mouse_wheel_handler(e, scene.userCamera.arcballCamera);
  };

  engine::onKeyboardEvent += [](const SDL_KeyboardEvent &e) {
    if (e.keysym.sym == SDLK_F5 && e.state == SDL_RELEASED) recompile_all_shaders();
  };


  auto material = make_material("character", "sources/shaders/character_vs.glsl", "sources/shaders/character_ps.glsl");

  material->set_property("mainTex", create_texture2d("resources/MotusMan_v55/MCG_diff.jpg"));

  ModelAsset MOB1_Stand_Relaxed_Fgt_v1_IPC = load_model("resources/Animations/IPC/MOB1_Stand_Relaxed_Fgt_v1_IPC.fbx");
  ModelAsset MOB1_Walk_F_Loop_IPC = load_model("resources/Animations/IPC/MOB1_Walk_F_Loop_IPC.fbx");
  ModelAsset MOB1_Run_F_Loop_IPC = load_model("resources/Animations/IPC/MOB1_Run_F_Loop_IPC.fbx");

  ModelAsset motusMan = load_model("resources/MotusMan_v55/MotusMan_v55.fbx");
  ModelAsset ruby = load_model("resources/sketchfab/ruby.fbx");

  scene.characters.reserve(16);

  {
    Character character;
    character.name = "MotusMan_v55";
    character.transform = glm::identity<glm::mat4>();
    character.meshes = motusMan.meshes;
    character.material = std::move(material);
    character.skeleton = motusMan.skeleton.get();
    character.animationContext = AnimationContext(*motusMan.skeleton);
    character.animationContext.setAnimation(MOB1_Walk_F_Loop_IPC.animations[0].get());
    scene.characters.emplace_back(std::move(character));

    scene.controllers.push_back(std::make_unique<ThirdPersonController>(scene.characters.back(), ThirdPersonController::Info {
      .speed = 1.0f,
      .idleAnimation = *MOB1_Stand_Relaxed_Fgt_v1_IPC.animations[0],
      .walkAnimation = *MOB1_Walk_F_Loop_IPC.animations[0],
      .runAnimation = *MOB1_Run_F_Loop_IPC.animations[0],
    }));
  }

  auto whiteMaterial = make_material("character", "sources/shaders/character_vs.glsl", "sources/shaders/character_ps.glsl");

  const uint8_t whiteColor[4] = {255, 255, 255, 255};
  whiteMaterial->set_property("mainTex", create_texture2d(whiteColor, 1, 1, 4));

  scene.characters.emplace_back(Character{
   "Ruby",
    glm::translate(glm::identity<glm::mat4>(), glm::vec3(2.f, 0.f, 0.f)),
    ruby.meshes,
    std::move(whiteMaterial),
    ruby.skeleton.get(),
    AnimationContext(*ruby.skeleton.get())
  });

  scene.characters.back().animationContext.setAnimation(ruby.animations[0].get());

  scene.models.push_back(std::move(motusMan));
  scene.models.push_back(std::move(ruby));
  scene.models.push_back(std::move(MOB1_Stand_Relaxed_Fgt_v1_IPC));
  scene.models.push_back(std::move(MOB1_Walk_F_Loop_IPC));
  scene.models.push_back(std::move(MOB1_Run_F_Loop_IPC));

  std::fflush(stdout);
}