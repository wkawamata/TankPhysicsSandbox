#pragma once

#include "PhysicsTypes.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace Tank::Physics
{
    struct ImpactMark
    {
        Vec3 position = {};
        Vec3 normal = {0.0f, 1.0f, 0.0f};
        std::uint64_t sequence = 0; // Zero is an unused slot.
    };

    class ImpactMarkBuffer
    {
    public:
        void SetCapacity(int capacity)
        {
            const size_t desired = static_cast<size_t>(std::clamp(capacity, 0, 1024));
            if (desired == m_slots.size()) return;
            std::vector<ImpactMark> newest;
            for (const auto& mark : m_slots)
                if (mark.sequence != 0) newest.push_back(mark);
            std::sort(newest.begin(), newest.end(), [](const auto& a, const auto& b)
                { return a.sequence < b.sequence; });
            if (newest.size() > desired) newest.erase(newest.begin(), newest.end() - desired);
            m_slots.assign(desired, {});
            std::copy(newest.begin(), newest.end(), m_slots.begin());
            m_count = newest.size();
            m_next = desired == 0 ? 0 : m_count % desired;
        }

        void Add(const Vec3& position, const Vec3& normal)
        {
            if (m_slots.empty()) return;
            m_slots[m_next] = {position, normal, ++m_sequence};
            m_next = (m_next + 1) % m_slots.size();
            m_count = std::min(m_count + 1, m_slots.size());
        }

        const std::vector<ImpactMark>& Slots() const { return m_slots; }
        size_t Count() const { return m_count; }
        size_t Capacity() const { return m_slots.size(); }

    private:
        std::vector<ImpactMark> m_slots;
        size_t m_next = 0;
        size_t m_count = 0;
        std::uint64_t m_sequence = 0;
    };
}
