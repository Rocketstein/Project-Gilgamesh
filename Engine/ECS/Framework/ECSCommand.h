#pragma once

#include "Engine/ECS/Entity/Entity.h"

template <typename Component>
struct ECSAddEntityCommand
{


};

template <typename Component>
struct ECSRemoveEntityCommand
{


};

template <typename Component>
struct ECSAddComponentCommand
{
	Entity entity;
};

template <typename Component>
struct ECSRemoveComponentCommand
{


};

template <typename Component>
struct ECSSetComponentValueCommand
{


};

