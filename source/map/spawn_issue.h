#ifndef RME_MAP_SPAWN_ISSUE_H_
#define RME_MAP_SPAWN_ISSUE_H_

#include <string>
#include "map/position.h"

struct SpawnIssue {
    std::string name;
    Position pos;
    std::string kind;
    std::string reason;
};

#endif
