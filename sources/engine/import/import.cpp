#include "render/mesh.h"
#include <vector>
#include <optional>
#include <3dmath.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include "engine/api.h"
#include "glad/glad.h"
#include <glm/gtc/type_ptr.hpp>

#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/runtime/skeleton_utils.h>

#include "import/model.h"

using RawSkeleton = ozz::animation::offline::RawSkeleton;
using RawJoint = RawSkeleton::Joint;

MeshPtr create_mesh(const aiMesh *mesh)
{
  std::vector<uint32_t> indices;
  std::vector<vec3> vertices;
  std::vector<vec3> normals;
  std::vector<vec2> uv;
  std::vector<uvec4> boneIndices;
  std::vector<vec4> boneWeights;
  std::vector<mat4> inverseBindPose;
  std::vector<std::string> bonesNames;

  int numVert = mesh->mNumVertices;
  int numFaces = mesh->mNumFaces;

  if (mesh->HasFaces())
  {
    indices.resize(numFaces * 3);
    for (int i = 0; i < numFaces; i++)
    {
      assert(mesh->mFaces[i].mNumIndices == 3);
      for (int j = 0; j < 3; j++)
        indices[i * 3 + j] = mesh->mFaces[i].mIndices[j];
    }
  }

  if (mesh->HasPositions())
  {
    vertices.resize(numVert);
    for (int i = 0; i < numVert; i++)
      vertices[i] = to_vec3(mesh->mVertices[i]);
  }

  if (mesh->HasNormals())
  {
    normals.resize(numVert);
    for (int i = 0; i < numVert; i++)
      normals[i] = to_vec3(mesh->mNormals[i]);
  }

  if (mesh->HasTextureCoords(0))
  {
    uv.resize(numVert);
    for (int i = 0; i < numVert; i++)
      uv[i] = to_vec2(mesh->mTextureCoords[0][i]);
  }

  if (mesh->HasBones())
  {
    boneWeights.resize(numVert, vec4(0.f));
    boneIndices.resize(numVert);
    inverseBindPose.resize(mesh->mNumBones);
    bonesNames.resize(mesh->mNumBones);

    int numBones = mesh->mNumBones;
    std::vector<int> weightsOffset(numVert, 0);
    for (int i = 0; i < numBones; i++)
    {
      const aiBone *bone = mesh->mBones[i];

      glm::mat4 mOffsetMatrix;
      memcpy(&mOffsetMatrix, &bone->mOffsetMatrix, sizeof(mOffsetMatrix));
      mOffsetMatrix = glm::transpose(mOffsetMatrix);
      inverseBindPose[i] = mOffsetMatrix;
      bonesNames[i] = bone->mName.C_Str();

      for (unsigned j = 0; j < bone->mNumWeights; j++)
      {
        int vertex = bone->mWeights[j].mVertexId;
        int offset = weightsOffset[vertex]++;
        assert(offset < 4);
        boneWeights[vertex][offset] = bone->mWeights[j].mWeight;
        boneIndices[vertex][offset] = i;
      }

    }
    // the sum of weights not 1
    for (int i = 0; i < numVert; i++)
    {
      vec4 w = boneWeights[i];
      float s = w.x + w.y + w.z + w.w;
      boneWeights[i] *= 1.f / s;
    }
  }

  return create_mesh(mesh->mName.C_Str(), indices, vertices, normals, uv, boneWeights, boneIndices, std::move(inverseBindPose), std::move(bonesNames));
}

void load_skeleton_joint(RawSkeleton& rawSkeleton, RawJoint& joint, const aiNode &node)
{
  joint.name = node.mName.C_Str();

  aiVector3D scale;
  aiQuaternion rotation;
  aiVector3D position;
  node.mTransformation.Decompose(scale, rotation, position);

  joint.transform.translation = ozz::math::Float3(position.x, position.y, position.z);
  joint.transform.rotation = ozz::math::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
  joint.transform.scale = ozz::math::Float3(scale.x, scale.y, scale.z);

  joint.children.resize(node.mNumChildren);
  for (int32_t i = 0; i < node.mNumChildren; ++i)
  {
    load_skeleton_joint(rawSkeleton, joint.children[i], *node.mChildren[i]);
  }
}

ozz::unique_ptr<ozz::animation::Animation> create_animation(const aiAnimation &animation, const ozz::animation::Skeleton &skeleton)
{
  ozz::animation::offline::RawAnimation rawAnimation;

  rawAnimation.name = animation.mName.C_Str();
  rawAnimation.duration = animation.mDuration / animation.mTicksPerSecond;

  rawAnimation.tracks.resize(skeleton.num_joints());
  for (int jointIdx = 0; jointIdx < skeleton.num_joints(); ++jointIdx)
  {
    auto& jointTrack = rawAnimation.tracks[jointIdx];
    std::string_view jointName = skeleton.joint_names()[jointIdx];

    /* Find the src assimp channel for this joint */
    std::optional<uint32_t> srcChannelIdx = std::nullopt;
    for (uint32_t channelIdx = 0; channelIdx < animation.mNumChannels; ++channelIdx)
    {
      if (jointName == animation.mChannels[channelIdx]->mNodeName.C_Str())
      {
        srcChannelIdx = channelIdx;
        break;
      }
    }

    /* If there is no channel for this joint, then simply add a single keyframe with rest pose local transform */
    if (!srcChannelIdx)
    {
      ozz::math::Transform transform = ozz::animation::GetJointLocalRestPose(skeleton, jointIdx);

      jointTrack.translations.resize(1);
      jointTrack.translations[0].time = 0.f;
      jointTrack.translations[0].value = transform.translation;

      jointTrack.rotations.resize(1);
      jointTrack.rotations[0].time = 0.f;
      jointTrack.rotations[0].value = transform.rotation;

      jointTrack.scales.resize(1);
      jointTrack.scales[0].time = 0.f;
      jointTrack.scales[0].value = transform.scale;

      continue;
    }

    const aiNodeAnim &srcChannel = *animation.mChannels[srcChannelIdx.value()];

    /* Load translations */
    jointTrack.translations.resize(srcChannel.mNumPositionKeys);
    for (uint32_t keyIdx = 0; keyIdx < srcChannel.mNumPositionKeys; ++keyIdx)
    {
      const aiVectorKey &key = srcChannel.mPositionKeys[keyIdx];

      jointTrack.translations[keyIdx].time = key.mTime / animation.mTicksPerSecond;
      jointTrack.translations[keyIdx].value = ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z);
    }

    /* Load rotations */
    jointTrack.rotations.resize(srcChannel.mNumRotationKeys);
    for (uint32_t keyIdx = 0; keyIdx < srcChannel.mNumRotationKeys; ++keyIdx)
    {
      const aiQuatKey &key = srcChannel.mRotationKeys[keyIdx];

      jointTrack.rotations[keyIdx].time = key.mTime / animation.mTicksPerSecond;
      jointTrack.rotations[keyIdx].value = ozz::math::Quaternion(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w);
    }

    /* Load scales */
    jointTrack.scales.resize(srcChannel.mNumScalingKeys);
    for (uint32_t keyIdx = 0; keyIdx < srcChannel.mNumScalingKeys; ++keyIdx)
    {
      const aiVectorKey &key = srcChannel.mScalingKeys[keyIdx];

      jointTrack.scales[keyIdx].time = key.mTime / animation.mTicksPerSecond;
      jointTrack.scales[keyIdx].value = ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z);
    }
  }

  if (!rawAnimation.Validate())
  {
    engine::error("Invalid raw animation data in animation \"%s\"", rawAnimation.name.c_str());
    return nullptr;
  }

  engine::log("Animation \"%s\" loaded", rawAnimation.name.c_str());

  ozz::animation::offline::AnimationBuilder builder;
  return builder(rawAnimation);
}

ModelAsset load_model(const char *path)
{
  Assimp::Importer importer;
  importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
  importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.f);

  importer.ReadFile(path,
    aiPostProcessSteps::aiProcess_Triangulate |
    aiPostProcessSteps::aiProcess_LimitBoneWeights |
    aiPostProcessSteps::aiProcess_GenNormals |
    aiPostProcessSteps::aiProcess_GlobalScale |
    aiPostProcessSteps::aiProcess_FlipWindingOrder);

  const aiScene *scene = importer.GetScene();
  ModelAsset model;
  model.path = path;
  if (!scene)
  {
    engine::error("Failed to read model file \"%s\"", path);
    return model;
  }

  RawSkeleton rawSkeleton;
  rawSkeleton.roots.resize(1);
  load_skeleton_joint(rawSkeleton, rawSkeleton.roots[0], *scene->mRootNode);
  if (!rawSkeleton.Validate())
  {
    engine::error("Invalid raw skeleton data in model file \"%s\"", path);
    return model;
  }

  ozz::animation::offline::SkeletonBuilder skeletonBuilder;
  model.skeleton = skeletonBuilder(rawSkeleton);

  model.meshes.resize(scene->mNumMeshes);
  for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
  {
    model.meshes[i] = create_mesh(scene->mMeshes[i]);
  }

  model.animations.resize(scene->mNumAnimations);
  for (uint32_t i = 0; i < scene->mNumAnimations; ++i)
  {
    model.animations[i] = create_animation(*scene->mAnimations[i], *model.skeleton);
  }

  engine::log("Model \"%s\" loaded", path);
  return model;
}