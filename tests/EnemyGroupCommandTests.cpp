#include "Physics/EnemyGroupCommand.h"

#include <iostream>
#include <stdexcept>
#include <vector>

int main()
{
    using namespace Tank::Physics;
    try
    {
        const auto check = [](bool valid, const char* message)
            { if (!valid) throw std::runtime_error(message); };
        EnemyGroupCommand command;
        std::vector<EnemyCommandMember> members = {
            {"tank", 20, true}, {"turret", 10, true}, {"reserve", 30, true} };
        command.Update(members);
        check(command.LeaderId() == "turret", "Priority elects leader independent of movement type");
        members[1].canCommand = false;
        command.Update(members);
        check(command.LeaderId() == "tank", "Destroyed or detached leader transfers command");
        members[1].canCommand = true;
        command.Update(members);
        check(command.LeaderId() == "tank", "Recovered leader does not reclaim command");
        members.erase(members.begin());
        command.Update(members);
        check(command.LeaderId() == "turret", "Removed leader transfers command");
        for (auto& member : members) member.canCommand = false;
        command.Update(members);
        check(command.LeaderId().empty(), "No eligible successor has no leader");
        members[1].canCommand = true;
        command.Update(members);
        check(command.LeaderId() == "reserve", "Command resumes when a member becomes eligible");
        command.Update({});
        check(command.LeaderId().empty(), "Empty group clears command");
        std::cout << "PASS EnemyGroupCommand\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL EnemyGroupCommand: " << error.what() << '\n';
        return 1;
    }
}
