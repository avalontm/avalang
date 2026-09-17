#pragma once

#include <vector>

#include "designer/types.h"

namespace studio::designer {

class SelectionManager {
public:
    void Select(const NodeId& id, bool additive = false);
    void SelectMany(const std::vector<NodeId>& ids);
    void Toggle(const NodeId& id);
    void Deselect(const NodeId& id);
    void Clear();

    void SetHovered(const NodeId& id);
    void ClearHovered();

    void SetFocused(const NodeId& id);
    void ClearFocused();

    bool IsSelected(const NodeId& id) const;
    bool IsEmpty() const;
    const std::vector<NodeId>& Selected() const;
    NodeId Primary() const;
    const NodeId& Hovered() const;
    const NodeId& Focused() const;

private:
    std::vector<NodeId> selected_;
    NodeId hovered_;
    NodeId focused_;
};

}
