#include <array>
#include "imgui/imgui.h"
#include "imgui/ImGuizmo.h"

#include <ozz/animation/runtime/skeleton_utils.h>

#include "scene.h"

static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);

static void show_info()
{
  if (ImGui::Begin("Info"))
  {
    ImGui::Text("ESC - exit");
    ImGui::Text("F5 - recompile shaders");
    ImGui::Text("Left Mouse Button and Wheel - controll camera");
  }
  ImGui::End();
}

static ImVec2 world_to_screen(const UserCamera &camera, glm::vec3 world_position)
{
  glm::vec4 clipSpace = camera.projection * inverse(camera.transform) * glm::vec4(world_position, 1.f);
  glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
  glm::vec2 screen = (glm::vec2(ndc) + 1.f) / 2.f;
  ImGuiIO &io = ImGui::GetIO();
  return ImVec2(screen.x * io.DisplaySize.x, io.DisplaySize.y - screen.y * io.DisplaySize.y);
}

static void draw_bone(const UserCamera &camera, glm::vec3 from, glm::vec3 to)
{
  constexpr float X_SPLIT = 0.1f;
  constexpr float Y_SPLIT = 0.2f;

  constexpr std::array POSITIONS = {
    glm::vec3(0.0f, 0.0f, 0.0f),
    glm::vec3(X_SPLIT, Y_SPLIT, 0.0f),
    glm::vec3(0.0f, Y_SPLIT, X_SPLIT),

    glm::vec3(0.0f, 0.0f, 0.0f),
    glm::vec3(X_SPLIT, Y_SPLIT, 0.0f),
    glm::vec3(0.0f, Y_SPLIT, -X_SPLIT),

    glm::vec3(0.0f, 0.0f, 0.0f),
    glm::vec3(-X_SPLIT, Y_SPLIT, 0.0f),
    glm::vec3(0.0f, Y_SPLIT, X_SPLIT),

    glm::vec3(0.0f, 0.0f, 0.0f),
    glm::vec3(-X_SPLIT, Y_SPLIT, 0.0f),
    glm::vec3(0.0f, Y_SPLIT, -X_SPLIT),

    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(X_SPLIT, Y_SPLIT, 0.0f),
    glm::vec3(0.0f, Y_SPLIT, X_SPLIT),

    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(X_SPLIT, Y_SPLIT, 0.0f),
    glm::vec3(0.0f, Y_SPLIT, -X_SPLIT),

    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(-X_SPLIT, Y_SPLIT, 0.0f),
    glm::vec3(0.0f, Y_SPLIT, X_SPLIT),

    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(-X_SPLIT, Y_SPLIT, 0.0f),
    glm::vec3(0.0f, Y_SPLIT, -X_SPLIT),
  };

  constexpr auto BONE_COLOR = IM_COL32(255, 215, 0, 128);

  ImDrawList *drawList = ImGui::GetWindowDrawList();

  glm::vec3 dir = to - from;
  float len = glm::length(dir);

  for (uint32_t triangleIdx = 0; triangleIdx < POSITIONS.size() / 3; ++triangleIdx)
  {
    std::array<ImVec2, 3> triangleScreen;

    for (uint32_t i = 0; i < triangleScreen.size(); ++i)
    {
      glm::vec3 position = POSITIONS[3 * triangleIdx + i];

      position *= len;
      position = glm::quat(glm::vec3(0.0f, 1.0f, 0.0f), glm::normalize(dir)) * position;

      position += from;

      triangleScreen[i] = world_to_screen(camera, position);
    }

    drawList->AddTriangle(triangleScreen[0], triangleScreen[1], triangleScreen[2], BONE_COLOR, 2.0f);
  }
}

static void draw_line(const UserCamera &camera, const glm::mat4 &transform, glm::vec3 to, ImColor color, float thickness)
{
  auto fromScreen = world_to_screen(camera, transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
  auto toScreen = world_to_screen(camera, transform * glm::vec4(to, 1.0f));

  ImGui::GetWindowDrawList()->AddLine(fromScreen, toScreen, color, thickness);
}

static void manipulate_transform(glm::mat4 &transform, const UserCamera &camera)
{
  ImGuizmo::BeginFrame();
  const glm::mat4 &projection = camera.projection;
  mat4 cameraView = inverse(camera.transform);
  ImGuiIO &io = ImGui::GetIO();
  ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

  glm::mat4 globNodeTm = transform;

  ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(projection), mCurrentGizmoOperation, mCurrentGizmoMode,
                       glm::value_ptr(globNodeTm));

  transform = globNodeTm;
}

static void show_characters(Scene &scene)
{
  // implement showing characters when only one character can be selected
  static uint32_t selectedCharacter = -1u;
  static uint32_t selectedNode = -1u;
  static std::vector<int> skeletonJointParentsStack;

  if (ImGui::Begin("Scene"))
  {
    for (size_t i = 0; i < scene.characters.size(); i++)
    {
      Character &character = scene.characters[i];
      ImGui::PushID(i);

      if (ImGui::Selectable(character.name.c_str(), selectedCharacter == i, ImGuiSelectableFlags_AllowDoubleClick))
      {
        selectedCharacter = i;
        if (ImGui::IsMouseDoubleClicked(0))
        {
          scene.userCamera.arcballCamera.targetPosition = vec3(character.transform[3]) + vec3(0, 1, 0);
        }
      }

      if (selectedCharacter == i)
      {
        const float INDENT = 10.0f;
        ImGui::Indent(INDENT);
        ImGui::Text("Meshes: %zu", character.meshes.size());

        // show skeleton
        const auto &skeleton = scene.models[i].skeleton;

        ImGui::Text("Skeleton Nodes: %zu", skeleton->num_joints());

        // show  skeleton node hierarchy
        skeletonJointParentsStack.clear();
        ozz::animation::IterateJointsDF(*skeleton, [&](int jointIndex, int parentIndex) {
          while (!skeletonJointParentsStack.empty() && skeletonJointParentsStack.back() != parentIndex)
          {
            skeletonJointParentsStack.pop_back();
          }

          ImGui::Indent(INDENT * skeletonJointParentsStack.size());
          if (ImGui::Selectable(skeleton->joint_names()[jointIndex], selectedNode == jointIndex))
          {
            selectedNode = jointIndex;
          }
          ImGui::Unindent(INDENT * skeletonJointParentsStack.size());

          skeletonJointParentsStack.push_back(jointIndex);
        });
      }

      ImGui::PopID();
    }

    if (selectedCharacter < scene.characters.size())
    {
      Character &character = scene.characters[selectedCharacter];
      if (selectedNode < character.skeleton->num_joints())
      {
        glm::mat4 &worldTransform = character.animationContext.getWorldTransforms()[selectedNode];
        glm::mat4 transform = character.transform * worldTransform;
        manipulate_transform(transform, scene.userCamera);
        worldTransform = inverse(character.transform) * transform;
      }
      else
      {
        manipulate_transform(character.transform, scene.userCamera);
      }
    }
  }
  ImGui::End();

  if (selectedCharacter < scene.characters.size())
  {
    const Character &character = scene.characters[selectedCharacter];

    const ImU32 flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    ImGui::PushStyleColor(ImGuiCol_Border, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("gizmo", NULL, flags);

    // Visualize skeleton bones
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    for (size_t j = 0; j < character.skeleton->num_joints(); ++j)
    {
      glm::mat4 nodeTransform = character.animationContext.getWorldTransforms()[j];
      glm::vec3 nodePosition = nodeTransform[3];

      int32_t parentIndex = character.skeleton->joint_parents()[j];
      if (parentIndex != ozz::animation::Skeleton::kNoParent)
      {
        glm::mat4 parentTransform = character.animationContext.getWorldTransforms()[parentIndex];
        glm::vec3 parentPosition = parentTransform[3];

        draw_bone(scene.userCamera, parentPosition, nodePosition);

        float lineLength = (glm::length(nodePosition - parentPosition) + 0.1f) * 0.1f;
        draw_line(scene.userCamera, nodeTransform, glm::vec3(lineLength, 0.0f, 0.0f), IM_COL32(255, 0, 0, 255), 1.5f);
        draw_line(scene.userCamera, nodeTransform, glm::vec3(0.0f, lineLength, 0.0f), IM_COL32(0, 255, 0, 255), 1.5f);
        draw_line(scene.userCamera, nodeTransform, glm::vec3(0.0f, 0.0f, lineLength), IM_COL32(0, 0, 255, 255), 1.5f);
      }

      drawList->AddCircleFilled(world_to_screen(scene.userCamera, nodePosition), 4.0f, IM_COL32(255, 215, 0, 215));
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
  }
}

void render_imguizmo(ImGuizmo::OPERATION &mCurrentGizmoOperation, ImGuizmo::MODE &mCurrentGizmoMode)
{
  if (ImGui::Begin("gizmo window"))
  {
    if (ImGui::IsKeyPressed('Z'))
      mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed('E'))
      mCurrentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed('R')) // r Key
      mCurrentGizmoOperation = ImGuizmo::SCALE;
    if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
      mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
      mCurrentGizmoOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
      mCurrentGizmoOperation = ImGuizmo::SCALE;

    if (mCurrentGizmoOperation != ImGuizmo::SCALE)
    {
      if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
        mCurrentGizmoMode = ImGuizmo::LOCAL;
      ImGui::SameLine();
      if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
        mCurrentGizmoMode = ImGuizmo::WORLD;
    }
  }
  ImGui::End();
}

static void show_models(Scene &scene)
{
  if (ImGui::Begin("Models"))
  {
    static uint32_t selectedModel = -1u;
    for (size_t i = 0; i < scene.models.size(); i++)
    {
      const ModelAsset &model = scene.models[i];
      if (ImGui::Selectable(model.path.c_str(), selectedModel == i))
      {
        selectedModel = i;
      }
      if (selectedModel == i)
      {
        ImGui::Indent(15.0f);
        ImGui::Text("Path: %s", model.path.c_str());
        ImGui::Text("Meshes: %zu", model.meshes.size());
        for (size_t j = 0; j < model.meshes.size(); j++)
        {
          const MeshPtr &mesh = model.meshes[j];
          ImGui::Text("%s", mesh->name.c_str());
        }
        ImGui::Unindent(15.0f);
      }
    }
  }
  ImGui::End();
}

void application_imgui_render(Scene &scene)
{
  render_imguizmo(mCurrentGizmoOperation, mCurrentGizmoMode);

  show_info();
  show_characters(scene);
  show_models(scene);
}