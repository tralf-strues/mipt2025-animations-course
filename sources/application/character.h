#pragma once
#include "engine/3dmath.h"
#include "engine/render/material.h"
#include "engine/render/mesh.h"

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/base/maths/soa_transform.h>

class AnimationContext
{
public:
  AnimationContext() = default;

  AnimationContext(const ozz::animation::Skeleton &skeleton)
      : skeleton(&skeleton),
        samplingContext(std::make_unique<ozz::animation::SamplingJob::Context>(skeleton.num_joints())),
        localTransforms(skeleton.num_joints()),
        worldTransforms(skeleton.num_joints())
  {
    localTransforms.assign(skeleton.joint_rest_poses().begin(), skeleton.joint_rest_poses().end());
  }

  void setLoop(bool loop)
  {
    this->loop = loop;
  }

  bool getLoop() const
  {
    return loop;
  }

  std::span<glm::mat4> getWorldTransforms()
  {
    return {reinterpret_cast<glm::mat4 *>(worldTransforms.data()), worldTransforms.size()};
  }

  std::span<const glm::mat4> getWorldTransforms() const
  {
    return {reinterpret_cast<const glm::mat4 *>(worldTransforms.data()), worldTransforms.size()};
  }

  void setAnimation(const ozz::animation::Animation *animation)
  {
    samplingContext->Invalidate();
    this->animation = animation;

    if (!animation)
    {
      localTransforms.assign(skeleton->joint_rest_poses().begin(), skeleton->joint_rest_poses().end());
    }

    progress = 0.0f;
  }

  void advance(const glm::mat4& rootTransform, float dt)
  {
    if (animation)
    {
      progress = loop
        ? std::fmod(progress + dt / animation->duration(), 1.0f)
        : std::min(progress + dt / animation->duration(), 1.0f);

      ozz::animation::SamplingJob samplingJob;
      samplingJob.animation = animation;
      samplingJob.ratio = progress;
      samplingJob.context = samplingContext.get();
      samplingJob.output = ozz::make_span(localTransforms);

      assert(animation->num_tracks() == skeleton->num_joints());

      if (!samplingJob.Run())
      {
        engine::error("AnimationContext::advance: SamplingJob validation failed");
        setAnimation(nullptr);
      }
    }

    ozz::math::Float4x4 root;
    memcpy(&root, &rootTransform, sizeof(root));

    ozz::animation::LocalToModelJob localToModelJob;
    localToModelJob.skeleton = skeleton;
    localToModelJob.input = ozz::make_span(localTransforms);
    localToModelJob.output = ozz::make_span(worldTransforms);
    localToModelJob.root = &root;

    if (!localToModelJob.Run())
    {
      engine::error("AnimationContext::advance: LocalToModelJob validation failed");
      setAnimation(nullptr);
    }
  }

private:
  const ozz::animation::Skeleton *skeleton;
  std::unique_ptr<ozz::animation::SamplingJob::Context> samplingContext;

  std::vector<ozz::math::SoaTransform> localTransforms;
  std::vector<ozz::math::Float4x4> worldTransforms;

  const ozz::animation::Animation* animation = nullptr;
  float progress = 0.0f;
  bool loop = true;
};

struct Character
{
  std::string name;
  glm::mat4 transform;
  std::vector<MeshPtr> meshes;
  MaterialPtr material;
  const ozz::animation::Skeleton* skeleton;
  AnimationContext animationContext;
};
