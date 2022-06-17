#include "ecs.hpp"
#include <iostream>

struct Position {
    int x;
    int y;
};

struct Velocity {
    int dx;
    int dy;
};

auto system_display(const ecs::Registry& registry, const uint32_t view) -> void {
    // system diplay
    for (const auto entity : registry.get_entities(view)) {
        const auto pos = registry.get<Position>(entity);
        std::cout << "Position " + std::to_string(entity) + ": (" + std::to_string(pos.x) + "," +
                         std::to_string(pos.y) + ")"
                  << std::endl;
    }
}

auto system_update_movement(ecs::Registry& registry, const uint32_t view) -> void {
    for (const auto entity : registry.get_entities(view)) {
        auto& pos = registry.get<Position>(entity);
        const auto vel = registry.get<Velocity>(entity);
        pos.x += vel.dx;
        pos.y += vel.dy;
    }
}

auto main() -> int32_t {
    auto registry = ecs::Registry{};

    // create entities with varying velocity vectors
    for (auto i = 0; i < 10; ++i) {
        auto entity = registry.create_entity();
        registry.assign<Position>(entity, {0, 0});
        registry.assign<Velocity>(entity, {1, i});
    }

    // create single still entity
    {
        auto entity = registry.create_entity();
        registry.assign<Position>(entity, {2, -2});
    }

    // create view of all moving entities
    auto view_movement = registry.create_view<Position, Velocity>();

    // create view of all entities with a position
    auto view_display = registry.create_view<Position>();

    std::cout << "Before movement ticks:\n";
    system_display(registry, view_display);

    // iterate movement many times
    auto ticks = 1000u;
    for (auto c = 0u; c < ticks; ++c) {
        system_update_movement(registry, view_movement);
    }

    std::cout << "\nAfter movement ticks:\n";
    system_display(registry, view_display);

    // destroy a view
    registry.destroy_view(view_movement);

    // remove some components
    registry.remove<Position>(3);
    registry.remove<Position>(4);
    registry.remove<Position>(5);
    std::cout << "\nAfter deleting entity 3, 4, and 5:\n";
    system_display(registry, view_display);

    // duplicate registry
    {
        auto second_registry = registry;
        auto entities = second_registry.get_entities(view_display);
        for (auto entity : entities) {
            auto& pos = second_registry.get<Position>(entity);
            pos.x = 13;
            pos.y = 37;
        }
        for (auto i = 0u; i < 5; i++) {
            auto entity = second_registry.create_entity();
            second_registry.assign<Position>(entity, {10, 10});
        }
        std::cout << "\nDulicated and modified registry:\n";
        system_display(second_registry, view_display);
        std::cout << "\n...compared to original, unmodified:\n";
        system_display(registry, view_display);
    }

    return EXIT_SUCCESS;
}