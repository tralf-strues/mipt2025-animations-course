
#include "scene.h"

#include <ozz/animation/runtime/skeleton_utils.h>

void render_character(const Character &character, const mat4 &cameraProjView, vec3 cameraPosition, const DirectionLight &light)
{
  const Material &material = *character.material;
  const Shader &shader = material.get_shader();

  shader.use();
  material.bind_uniforms_to_shader();
  shader.set_mat4x4("ViewProjection", cameraProjView);
  shader.set_vec3("CameraPosition", cameraPosition);
  shader.set_vec3("LightDirection", glm::normalize(light.lightDirection));
  shader.set_vec3("AmbientLight", light.ambient);
  shader.set_vec3("SunLight", light.lightColor);

  std::span<const mat4> bindPose = character.animationContext.getWorldTransforms();

  for (const MeshPtr &mesh : character.meshes)
  {
    static std::vector<glm::mat4> skinningMatricesWS;
    skinningMatricesWS.resize(mesh->inverseBindPose.size());

    for (size_t i = 0; i < mesh->inverseBindPose.size(); ++i)
    {
      auto jointIdx = ozz::animation::FindJoint(*character.skeleton, mesh->bonesNames[i].c_str());
      if (jointIdx == -1)
        engine::error("Bone \"%s\" from Mesh \"%s\" not found in skeleton", mesh->bonesNames[i].c_str(), mesh->name.c_str());

      skinningMatricesWS[i] = (jointIdx != -1)
        ? bindPose[jointIdx] * mesh->inverseBindPose[i]
        : glm::identity<glm::mat4>();
    }

    shader.set_mat4x4("SkinningMatricesWS", skinningMatricesWS);
    render(mesh);
  }
}

void application_render(Scene &scene)
{
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  const float grayColor = 0.3f;
  glClearColor(grayColor, grayColor, grayColor, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  const mat4 &projection = scene.userCamera.projection;
  mat4 projView;
  glm::vec3 cameraPosition;

  if (!scene.activeControllerIdx)
  {
    const glm::mat4 &transform = scene.userCamera.transform;
    cameraPosition = glm::vec3(transform[3]);
    projView = projection * inverse(transform);
  }
  else
  {
    const auto &controller = scene.controllers[scene.activeControllerIdx.value()];
    cameraPosition = controller->getCameraPosition();
    projView = projection * controller->getView();
  }


  for (const Character &character : scene.characters)
    render_character(character, projView, cameraPosition, scene.light);
}
