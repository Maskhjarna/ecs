# Overview
This repository hosts a single-include Entity Component System written in modern C++.

# Example usage
```c++
auto registry = ecs::Registry{};

for (auto i = 0u; i < 10u; ++i)
{
    auto entity = registry.create_entity();
    registry.assign<Position>(entity, {0, 0});
    registry.assign<Velocity>(entity, {1, i});
}

auto view = registry.create_view<Position, Velocity>();
for (auto entity : registry.get(view))
{
    auto& pos = registry.get<Position>(entity);
    const auto vel = registry.get<Velocity>(entity);
    pos.x += vel.dx;
    pos.y += vel.dy;
}
```
An extended, compiling example is found in ```example.cpp```.

# Implementation overview
The maximum number of entities and component data types is static, but can be specified at compile time by setting ```ECS_MAX_ENTITY_COUNT``` and ```ECS_MAX_COMPONENT_COUNT``` before including the library.
## Entities
Entities are stored as 32-bit unsigned integers in an ```std::array``` with skip values for faster iteration.

## Components
Data types registered as components have an ```std::array``` the size of the entity pool pre-allocated for constant time access. The array entry associated with any specific entity is inaccessible until the corresponding component has been assigned to that entity. Accessing the component of an entity after it or its component has been removed throws a runtime error.

## Views
Views are represented by 32-bit unsigned integers. Each view stores the identifiers of its entities in an ```std::unordered_set```. The set is kept up-to-date in regard to assignments and removals of components on entities, meaning an identifier never decays.

# Requirements
Compiles in C++14 or later. No external dependencies.

# License
Released under the MIT License. See LICENSE for more information.
