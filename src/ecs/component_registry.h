#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace acheron::ecs
{
    using ComponentTypeID = std::uint16_t;

    static constexpr ComponentTypeID INVALID_COMPONENT_TYPE = 0xFFFFu;
    static constexpr std::uint32_t MAX_COMPONENT_TYPES = 64u;
    static constexpr std::uint32_t MAX_COMPONENT_NAME_BYTES = 48u;

    struct ComponentSignature
    {
        std::uint64_t bits = 0;
    };

    constexpr bool operator==(ComponentSignature a, ComponentSignature b) noexcept
    {
        return a.bits == b.bits;
    }

    constexpr bool operator!=(ComponentSignature a, ComponentSignature b) noexcept
    {
        return !(a == b);
    }

    constexpr ComponentSignature empty_signature() noexcept
    {
        return {};
    }

    constexpr bool component_id_valid(ComponentTypeID id) noexcept
    {
        return id < MAX_COMPONENT_TYPES;
    }

    constexpr ComponentSignature component_bit(ComponentTypeID id) noexcept
    {
        return component_id_valid(id) ? ComponentSignature{ 1ull << id } : ComponentSignature{};
    }

    constexpr ComponentSignature signature_add(ComponentSignature signature, ComponentTypeID id) noexcept
    {
        signature.bits |= component_bit(id).bits;
        return signature;
    }

    constexpr ComponentSignature signature_remove(ComponentSignature signature, ComponentTypeID id) noexcept
    {
        if (component_id_valid(id))
        {
            signature.bits &= ~(1ull << id);
        }
        return signature;
    }

    constexpr bool signature_contains(ComponentSignature signature, ComponentTypeID id) noexcept
    {
        return (signature.bits & component_bit(id).bits) != 0ull;
    }

    constexpr bool signature_includes(ComponentSignature signature, ComponentSignature required) noexcept
    {
        return (signature.bits & required.bits) == required.bits;
    }

    std::uint32_t signature_count(ComponentSignature signature) noexcept;

    template <typename T>
    struct ComponentTraits;

    template <typename T>
    concept Component =
        std::is_trivially_copyable_v<T> &&
        std::is_standard_layout_v<T> &&
        requires
        {
            { ComponentTraits<T>::id } -> std::convertible_to<ComponentTypeID>;
            { ComponentTraits<T>::name } -> std::convertible_to<const char*>;
        };

    struct ComponentDescriptor
    {
        ComponentTypeID id = INVALID_COMPONENT_TYPE;
        std::uint16_t size = 0;
        std::uint16_t alignment = 0;
        const char* name = nullptr;
        std::uint32_t stable_hash = 0;
    };

    class ComponentRegistry final
    {
    public:
        ComponentRegistry() noexcept = default;

        ComponentRegistry(const ComponentRegistry&) = delete;
        ComponentRegistry& operator=(const ComponentRegistry&) = delete;

        void reset() noexcept;

        bool register_descriptor(ComponentDescriptor descriptor) noexcept;

        template <Component T>
        bool register_component() noexcept
        {
            ComponentDescriptor descriptor{};
            descriptor.id = ComponentTraits<T>::id;
            descriptor.size = static_cast<std::uint16_t>(sizeof(T));
            descriptor.alignment = static_cast<std::uint16_t>(alignof(T));
            descriptor.name = ComponentTraits<T>::name;
            descriptor.stable_hash = stable_name_hash(ComponentTraits<T>::name);
            return register_descriptor(descriptor);
        }

        bool is_registered(ComponentTypeID id) const noexcept;
        const ComponentDescriptor* descriptor(ComponentTypeID id) const noexcept;

        ComponentSignature registered_signature() const noexcept;
        std::uint32_t registered_count() const noexcept { return m_registered_count; }

        static std::uint32_t stable_name_hash(const char* name) noexcept;

    private:
        ComponentDescriptor m_descriptors[MAX_COMPONENT_TYPES]{};
        ComponentSignature m_registered{};
        std::uint32_t m_registered_count = 0;
    };
}

#define ACHERON_ECS_COMPONENT(TYPE, ID_VALUE) \
    template <> \
    struct acheron::ecs::ComponentTraits<TYPE> \
    { \
        static constexpr ::acheron::ecs::ComponentTypeID id = static_cast<::acheron::ecs::ComponentTypeID>(ID_VALUE); \
        static constexpr const char* name = #TYPE; \
    }
