/*
 * Rudimentary entity component system written by Gustav Andersson
 * (https://www.github.com/maskhjarna). Last edited: Apr 29 2022
 */

#ifndef ECS_H
#define ECS_H

#include <array>
#include <bitset>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ecs {

#ifndef ECS_MAX_ENTITY_COUNT
constexpr auto ECS_MAX_ENTITY_COUNT = 65535;
#endif

#ifndef ECS_MAX_COMPONENT_COUNT
constexpr auto ECS_MAX_COMPONENT_COUNT = 64;
#endif

struct Entity {
    uint32_t m_id_or_next_empty = 0;
    uint16_t m_next_valid_entity = ECS_MAX_ENTITY_COUNT;
    uint16_t m_prev_valid_entity = 0;
};

struct IComponentArray {
    virtual ~IComponentArray() = default;
    virtual auto copy() -> std::unique_ptr<IComponentArray> = 0;
};

template <typename T> struct ComponentArray : public IComponentArray {
    ComponentArray() : m_component_seq(ECS_MAX_ENTITY_COUNT){};
    ComponentArray(std::vector<T> component_seq) : m_component_seq{std::move(component_seq)} {}
    auto copy() -> std::unique_ptr<IComponentArray> {
        return std::move(std::make_unique<ComponentArray<T>>(m_component_seq));
    }
    std::vector<T> m_component_seq;
};

class Registry {
  public:
    Registry() {
        for (auto i = 0; i < ECS_MAX_ENTITY_COUNT; ++i)
            m_entities.at(i).m_id_or_next_empty = i + 1;
    }

    ~Registry() = default; // unique pointers clear memory

    Registry(const Registry& other) {
        m_entities = other.m_entities;
        m_first_available_entity_slot = other.m_first_available_entity_slot;
        m_view_count = other.m_view_count;
        m_entity_signatures = other.m_entity_signatures;
        m_view_entities = other.m_view_entities;
        m_view_signatures = other.m_view_signatures;
        m_bit_to_component = other.m_bit_to_component;
        for (auto i = 0; i < ECS_MAX_COMPONENT_COUNT; ++i) {
            const auto raw_ptr = other.m_component_arrays.at(i).get();
            if (raw_ptr != nullptr)
                m_component_arrays.at(i) = raw_ptr->copy();
        }
    }

    Registry(Registry&& other) { *this = other; }

    auto operator=(const Registry& other) -> Registry& { return *this = Registry(other); }

    auto operator=(Registry&& other) -> Registry& {
        std::swap(m_entities, other.m_entities);
        std::swap(m_first_available_entity_slot, other.m_first_available_entity_slot);
        std::swap(m_view_count, other.m_view_count);
        std::swap(m_entity_signatures, other.m_entity_signatures);
        std::swap(m_view_entities, other.m_view_entities);
        std::swap(m_view_signatures, other.m_view_signatures);
        std::swap(m_bit_to_component, other.m_bit_to_component);
        return *this;
    }

    /**
     * @brief
     * Creates a new entity.
     *
     * @return An unsigned integer representing the entity's id. This id is used for assigning and
     * accessing components of the entity.
     **/
    [[nodiscard]] auto create_entity() -> uint32_t {
        const auto temp = m_first_available_entity_slot;

        Entity new_entity = m_entities.at(temp);

        m_first_available_entity_slot = new_entity.m_id_or_next_empty;
        new_entity.m_id_or_next_empty = temp;

        if (temp > 0) {
            new_entity.m_next_valid_entity = m_entities.at(temp - 1).m_next_valid_entity;
            new_entity.m_prev_valid_entity = m_entities.at(temp - 1).m_id_or_next_empty;
            m_entities.at(temp - 1).m_next_valid_entity = temp;
        }
        return new_entity.m_id_or_next_empty;
    }

    /**
     * @brief Destroys an entity, rendering its component data inaccessible.
     *
     * @param entity: ID of the entity to destroy.
     **/
    auto destroy_entity(uint32_t entity) -> void {
        auto& destroyed_entity = m_entities.at(entity);
        if (entity < m_first_available_entity_slot) {
            destroyed_entity.m_id_or_next_empty = m_first_available_entity_slot;
            m_first_available_entity_slot = entity;
        } else {
            auto i = m_first_available_entity_slot;
            auto last_empty_slot_before = Entity{};
            do {
                last_empty_slot_before = m_entities.at(i);
                i = last_empty_slot_before.m_id_or_next_empty;
            } while (i < entity);

            destroyed_entity.m_id_or_next_empty = m_entities.at(i).m_id_or_next_empty;
            last_empty_slot_before.m_id_or_next_empty = entity;
        }
        m_entities.at(entity + 1).m_prev_valid_entity = destroyed_entity.m_prev_valid_entity;
        m_entities.at(entity - 1).m_next_valid_entity = destroyed_entity.m_next_valid_entity;

        m_entity_signatures.at(entity).reset();
    }

    /**
     * @brief Creates a view of all entities which have all of the given components assigned.
     *
     * @tparam Ts: The components to view.
     **/
    template <typename... Ts> [[nodiscard]] auto create_view() -> uint32_t {
        auto signature = std::bitset<ECS_MAX_COMPONENT_COUNT>{};
        set_component_bits<Ts...>(signature);

        for (auto view_and_signature : m_view_signatures)
            if (signature == view_and_signature.second)
                return view_and_signature.first;

        m_view_signatures.insert({m_view_count, signature});
        m_view_entities.insert({m_view_count, {}});

        for (auto entity = 0; entity < ECS_MAX_ENTITY_COUNT;
             entity = m_entities.at(entity).m_next_valid_entity)
            if ((m_entity_signatures.at(entity) & signature) == signature)
                m_view_entities.at(m_view_count).insert(entity);

        return m_view_count++;
    }

    /**
     * @brief Destroys a view, freeing its metadata.
     *
     * @param view: The view to destroy.
     */
    auto destroy_view(uint32_t view) -> void {
        if (m_view_signatures.find(view) == m_view_signatures.end())
            throw std::runtime_error("Attempted to remove non-existent view.");

        m_view_signatures.erase(view);
        m_view_entities.erase(view);
    }

    /**
     * @brief Assigns a component to an entity and initializes it.
     *
     * @tparam T: Struct to assign as a component of the entity.
     * @param entity: ID of the entity to assign the component to.
     * @param data: Data to initialize component with.
     **/
    template <typename T> auto assign(uint32_t entity, const T& data) -> void {
        const auto index =
            has_component_bit<T>() ? get_component_bit<T>() : create_component_bit<T>();
        ++m_components_assigned.at(index);

        const auto raw_ptr = m_component_arrays.at(index).get();
        static_cast<ComponentArray<T>*>(raw_ptr)->m_component_seq.at(entity) = data;

        m_entity_signatures.at(entity).set(index);
        on_entity_signature_change(entity);
    }

    /**
     * @brief Removes a component from an entity.
     * component.
     *
     * @tparam T: The component to remove.
     * @param entity: ID of the entity to remove the component from.
     **/
    template <typename T> auto remove(uint32_t entity) -> void {
        if (!has_component_bit<T>())
            throw std::runtime_error("Attempted to remove unassigned component.");
        auto index = get_component_bit<T>();
        if (!m_entity_signatures.at(entity)[index])
            throw std::runtime_error("Attempted to remove unassigned component.");
        if (--m_components_assigned.at(index) == 0) {
            m_bit_to_component.at(index) = 0;
            m_component_arrays.at(index).reset();
        }
        m_entity_signatures.at(entity).reset(index);
        on_entity_signature_change(entity);
    }

    /**
     * @brief Get the component data for an entity.
     *
     * @tparam T: Type of component to retrieve.
     * @param entity: ID of the entity.
     * @return A mutable reference to the entity's component data.
     **/
    template <typename T> [[nodiscard]] auto get(uint32_t entity) const -> T& {
        const auto raw_ptr = m_component_arrays.at(get_component_bit<T>()).get();
        return static_cast<ComponentArray<T>*>(raw_ptr)->m_component_seq.at(entity);
    }

    /**
     * @brief Returns all entities belonging to a view.
     *
     * @param view: A view returned by ecs::view(...).
     * @return An unordered set of all entities belonging to the view.
     **/
    [[nodiscard]] auto get_entities(uint32_t view) const -> const std::unordered_set<uint32_t>& {
        return m_view_entities.at(view);
    }

  private:
    uint32_t m_first_available_entity_slot = 0;
    uint32_t m_view_count = 0;
    std::array<Entity, ECS_MAX_ENTITY_COUNT> m_entities{};
    std::array<std::bitset<ECS_MAX_COMPONENT_COUNT>, ECS_MAX_ENTITY_COUNT> m_entity_signatures{};
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> m_view_entities{};
    std::unordered_map<uint32_t, std::bitset<ECS_MAX_COMPONENT_COUNT>> m_view_signatures{};
    std::array<size_t, ECS_MAX_COMPONENT_COUNT> m_bit_to_component{};
    std::array<uint32_t, ECS_MAX_COMPONENT_COUNT> m_components_assigned{0};
    std::array<std::unique_ptr<IComponentArray>, ECS_MAX_COMPONENT_COUNT> m_component_arrays{};

    /* helper function called from (add/remove)_component.
    updates system entity containers accordingly */
    auto on_entity_signature_change(uint32_t entity) -> void {
        const auto entity_signature = m_entity_signatures.at(entity);
        for (auto& pair : m_view_entities) {
            const auto view = pair.first;
            const auto& view_signature = m_view_signatures.at(view);
            auto& entities = pair.second;
            if ((entity_signature & view_signature) == view_signature)
                entities.insert(entity);
            else
                entities.erase(entity);
        }
    }

    template <typename T> [[nodiscard]] auto has_component_bit() const -> bool {
        const auto type_hash = typeid(T).hash_code();
        for (uint32_t bit = 0; bit < ECS_MAX_COMPONENT_COUNT; ++bit)
            if (m_bit_to_component.at(bit) == type_hash)
                return true;

        return false;
    }

    template <typename T> [[nodiscard]] auto create_component_bit() -> uint32_t {
        auto bit = 0;
        while (m_bit_to_component.at(bit))
            ++bit;
        m_bit_to_component.at(bit) = typeid(T).hash_code();
        m_component_arrays.at(bit) = std::make_unique<ComponentArray<T>>();
        return bit;
    }

    // returns a unique index for a given component
    template <typename T> [[nodiscard]] auto get_component_bit() const -> uint32_t {
        const auto type_hash = typeid(T).hash_code();
        for (uint32_t bit = 0; bit < ECS_MAX_COMPONENT_COUNT; ++bit)
            if (m_bit_to_component.at(bit) == type_hash)
                return bit;

        throw std::runtime_error("Component bit queried on unregistered component.");
    }

    // called from view when generating signature
    template <typename T, typename U, typename... Vs>
    auto set_component_bits(std::bitset<ECS_MAX_COMPONENT_COUNT>& signature) const -> void {
        set_component_bits<T>(signature);
        if (sizeof...(Vs) > 0)
            set_component_bits<U, Vs...>(signature);
        else
            set_component_bits<U>(signature);
    }

    template <typename T>
    auto set_component_bits(std::bitset<ECS_MAX_COMPONENT_COUNT>& signature) const -> void {
        signature.set(get_component_bit<T>());
    }
};

} // namespace ecs

#endif // ECS_H