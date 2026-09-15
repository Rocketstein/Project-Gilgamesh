#pragma once

template <typename T>
struct ECSEntity
{
	ECSEntity(size_t id) : id_(id) {}
	size_t id_;
	size_t generation_;
};