
A single-include Entity Component System (ECS) written in modern C++.

## Example
```c++
auto registry = ecs::Registry{};

for (int i = 0; i < 10; ++i)
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
An extended, compiling example can be found in ```example.cpp```.

## Implementation overview
The maximum number of entities and component data types is static, but can be specified at compile time by defining the macros ```ECS_MAX_ENTITY_COUNT``` and ```ECS_MAX_COMPONENT_COUNT```.
### Entities
Entities are stored as 32-bit unsigned integers in a ```std::array``` with skip values enabling fast iteration when updating views.

### Components
Data types that are registered as components have an ```std::array``` the size of the entity pool pre-allocated, allowing elements to be accessed in constant time. The array entry associated with any specific entity is inaccessible until the corresponding component has been assigned to that entity. Accessing the component of an entity after it or its component has been removed throws a runtime error to avoid unintended behaviour.

### Views
Views are represented by 32-bit unsigned integers. Each view stores its entities by identifier in an ```std::unordered_set```. The set is kept up-to-date in regard to assignments and removals of components on entities, meaning the identifier never decays.

## Requirements
Compiles in C++14 or later. No external dependencies.

## License
Released under the MIT License. See LICENSE for more information.