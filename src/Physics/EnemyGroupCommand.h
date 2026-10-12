#pragma once

#include <algorithm>
#include <span>
#include <string>

namespace Tank::Physics
{
    struct EnemyCommandMember
    {
        std::string enemyId;
        int commandPriority = 0;
        // Supplied by gameplay: false for destroyed or detached members.
        bool canCommand = true;
    };

    // Group-owned command identity. Independent of rendering and vehicle motion.
    // Configuration must provide unique enemy IDs and priorities (validated by the map loader).
    class EnemyGroupCommand
    {
    public:
        void Update(std::span<const EnemyCommandMember> members)
        {
            const auto current = std::find_if(members.begin(), members.end(), [&](const auto& member)
                { return member.enemyId == m_leaderId && member.canCommand; });
            if (current != members.end()) return;
            const EnemyCommandMember* successor = nullptr;
            for (const auto& member : members)
                if (member.canCommand && (!successor || member.commandPriority < successor->commandPriority))
                    successor = &member;
            m_leaderId = successor ? successor->enemyId : std::string{};
        }

        const std::string& LeaderId() const { return m_leaderId; }

    private:
        std::string m_leaderId;
    };
}
