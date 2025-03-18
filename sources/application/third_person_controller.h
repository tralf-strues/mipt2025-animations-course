#pragma once

#include "character.h"

class ThirdPersonController
{
public:
  struct Info
  {
    float speed = 1.0f;

    const ozz::animation::Animation &idleAnimation;
    const ozz::animation::Animation &walkAnimation;
    const ozz::animation::Animation &runAnimation;
  };

  ThirdPersonController(Character &character, const Info &info)
      : character(character), info(info)
  {
  }

  void onUpdate(float dt)
  {
    forward = glm::vec3(sin(yaw), 0.0f, cos(yaw));

    State newState = state;
    if (engine::get_key(SDLK_w))
    {
      newState = engine::get_key(SDLK_LSHIFT) ? State::Run : State::Walk;
    } else {
      newState = State::Idle;
    }

    if (newState != state)
    {
      state = newState;
      switch (state)
      {
      case State::Idle:
        character.animationContext.setAnimation(&info.idleAnimation);
        break;
      case State::Walk:
        character.animationContext.setAnimation(&info.walkAnimation);
        break;
      case State::Run:
        character.animationContext.setAnimation(&info.runAnimation);
        break;
      }
    }

    if (state != State::Idle)
    {
      float speed = (state == State::Run) ? 2.0f : 1.0f;
      position += forward * speed * info.speed * dt;
    }

    cameraPosition = position - forward * 3.f + vec3(0, 2, 0);
    view = glm::lookAt(getCameraPosition(), position + vec3(0, 1, 0), vec3(0, 1, 0));

    character.transform = glm::translate(glm::mat4(1.0f), position) * glm::yawPitchRoll(yaw, 0.0f, 0.0f);
  }

  void onMouseMotionEvent(const SDL_MouseMotionEvent &e)
  {
    yaw += 0.004f * e.xrel;
  }

  Character& getPawn() { return character; }

  glm::vec3 getPosition() const
  {
    return position;
  }

  glm::vec3 getForward() const
  {
    return forward;
  }

  glm::vec3 getCameraPosition() const
  {
    return cameraPosition;
  }

  glm::mat4 getView() const
  {
    return view;
  }

private:
  enum class State
  {
    Initial,
    Idle,
    Walk,
    Run
  };

  Character &character;
  Info info;

  glm::vec3 position{0.0f};
  glm::vec3 forward{0.0f};

  glm::vec3 cameraPosition{0.0f};
  glm::mat4 view{0.0f};

  State state = State::Initial;
  float yaw = 0.0f;
};
