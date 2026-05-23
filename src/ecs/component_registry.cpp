#include "component_registry.h"

#include <cstring>
#include <cassert>

namespace acheron::ecs
{
    std::uint32_t signature_count(ComponentSignature signature) noexcept
    {
        // Count set bits using Hamming weight
        std::uint32_t count = 0;
        std::uint64_t bits = signature.bits;
        while (bits)
        {
            count += static_cast<std::uint32_t>(bits & 1ull);
            bits >>= 1;
        }
        return count;
    }

    void ComponentRegistry::reset() noexcept
    {
        for (std::uint32_t i = 0; i < MAX_COMPONENT_TYPES; ++i)
        {
            m_descriptors[i] = {};
        }
        m_registered = empty_signature();
        m_registered_count = 0;
    }

    bool ComponentRegistry::register_descriptor(ComponentDescriptor descriptor) noexcept
    {
        // Validate descriptor
        if (descriptor.id >= MAX_COMPONENT_TYPES)
        {
            return false;
        }
        if (descriptor.size == 0 || descriptor.alignment == 0)
        {
            return false;
        }
        if (!descriptor.name)
        {
            return false;
        }

        // Check if already registered
        if (is_registered(descriptor.id))
        {
            // Cannot re-register; must be unique
            return false;
        }

        // Store descriptor
        m_descriptors[descriptor.id] = descriptor;
        m_registered.bits |= component_bit(descriptor.id).bits;
        ++m_registered_count;

        return true;
    }

    bool ComponentRegistry::is_registered(ComponentTypeID id) const noexcept
    {
        if (id >= MAX_COMPONENT_TYPES)
        {
            return false;
        }
        return signature_contains(m_registered, id);
    }

    const ComponentDescriptor* ComponentRegistry::descriptor(ComponentTypeID id) const noexcept
    {
        if (!is_registered(id))
        {
            return nullptr;
        }
        return &m_descriptors[id];
    }

    ComponentSignature ComponentRegistry::registered_signature() const noexcept
    {
        return m_registered;
    }

    std::uint32_t ComponentRegistry::stable_name_hash(const char* name) noexcept
    {
        // FNV-1a 32-bit hash
        constexpr std::uint32_t FNV_OFFSET = 0x811c9dc5u;
        constexpr std::uint32_t FNV_PRIME = 0x01000193u;

        std::uint32_t hash = FNV_OFFSET;
        if (name)
        {
            for (const char* p = name; *p; ++p)
            {
                hash ^= static_cast<unsigned char>(*p);
                hash *= FNV_PRIME;
            }
        }
        return hash;
    }
}

#if defined(ACHERON_COMPONENT_REGISTRY_UNIT_TEST)

namespace
{
    // Define test components
    struct TestPositionComponent
    {
        float x, y, z;
    };

    struct TestVelocityComponent
    {
        float vx, vy, vz;
    };

    struct TestTrafficAgentComponent
    {
        std::uint32_t route_id;
        float speed;
    };

    // Register them using the macro
    ACHERON_ECS_COMPONENT(TestPositionComponent, 0);
    ACHERON_ECS_COMPONENT(TestVelocityComponent, 1);
    ACHERON_ECS_COMPONENT(TestTrafficAgentComponent, 2);

    static bool registration_consistency_test() noexcept
    {
        acheron::ecs::ComponentRegistry registry;
        registry.reset();

        // Register the components
        if (!registry.register_component<TestPositionComponent>())
        {
            return false;
        }
        if (!registry.register_component<TestVelocityComponent>())
        {
            return false;
        }
        if (!registry.register_component<TestTrafficAgentComponent>())
        {
            return false;
        }

        // Check that they're registered
        if (!registry.is_registered(0) || !registry.is_registered(1) || !registry.is_registered(2))
        {
            return false;
        }

        // Check descriptor consistency
        auto* desc0 = registry.descriptor(0);
        auto* desc1 = registry.descriptor(1);
        auto* desc2 = registry.descriptor(2);

        if (!desc0 || !desc1 || !desc2)
        {
            return false;
        }

        // Validate sizes
        if (desc0->size != sizeof(TestPositionComponent))
        {
            return false;
        }
        if (desc1->size != sizeof(TestVelocityComponent))
        {
            return false;
        }
        if (desc2->size != sizeof(TestTrafficAgentComponent))
        {
            return false;
        }

        // Validate alignments
        if (desc0->alignment != alignof(TestPositionComponent))
        {
            return false;
        }
        if (desc1->alignment != alignof(TestVelocityComponent))
        {
            return false;
        }
        if (desc2->alignment != alignof(TestTrafficAgentComponent))
        {
            return false;
        }

        // Check registered count
        if (registry.registered_count() != 3)
        {
            return false;
        }

        // Verify signature
        auto sig = registry.registered_signature();
        if (!acheron::ecs::signature_contains(sig, 0) ||
            !acheron::ecs::signature_contains(sig, 1) ||
            !acheron::ecs::signature_contains(sig, 2))
        {
            return false;
        }

        // Check duplicate registration fails
        if (registry.register_component<TestPositionComponent>())
        {
            return false;
        }

        return true;
    }

    static bool signature_operations_test() noexcept
    {
        using namespace acheron::ecs;

        ComponentSignature sig0 = empty_signature();
        ComponentSignature sig1 = component_bit(0);
        ComponentSignature sig2 = component_bit(1);

        // Add components
        sig0 = signature_add(sig0, 0);
        if (!signature_contains(sig0, 0))
        {
            return false;
        }

        sig0 = signature_add(sig0, 1);
        if (!signature_contains(sig0, 1))
        {
            return false;
        }

        // Test signature inclusion
        if (!signature_includes(sig0, sig1) || !signature_includes(sig0, sig2))
        {
            return false;
        }

        // Remove component
        sig0 = signature_remove(sig0, 0);
        if (signature_contains(sig0, 0))
        {
            return false;
        }
        if (!signature_contains(sig0, 1))
        {
            return false;
        }

        return true;
    }

    static bool stable_hash_test() noexcept
    {
        using namespace acheron::ecs;

        // Same name should produce same hash
        std::uint32_t hash1 = ComponentRegistry::stable_name_hash("TestComponent");
        std::uint32_t hash2 = ComponentRegistry::stable_name_hash("TestComponent");

        if (hash1 != hash2)
        {
            return false;
        }

        // Different names should (probably) produce different hashes
        std::uint32_t hash3 = ComponentRegistry::stable_name_hash("AnotherComponent");

        if (hash1 == hash3)
        {
            return false; // Hash collision unlikely but possible
        }

        return true;
    }
}

#include <cstdio>

int main()
{
    if (!registration_consistency_test())
    {
        std::printf("[component_registry] registration consistency: FAIL\n");
        return 1;
    }
    std::printf("[component_registry] registration consistency: OK\n");

    if (!signature_operations_test())
    {
        std::printf("[component_registry] signature operations: FAIL\n");
        return 1;
    }
    std::printf("[component_registry] signature operations: OK\n");

    if (!stable_hash_test())
    {
        std::printf("[component_registry] stable hash: FAIL\n");
        return 1;
    }
    std::printf("[component_registry] stable hash: OK\n");

    return 0;
}

#endif
