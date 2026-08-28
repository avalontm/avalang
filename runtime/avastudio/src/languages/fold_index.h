#pragma once

#include <string>
#include <vector>

namespace studio {

struct FoldRange {
    int start_line = 0;
    int end_line = 0;
};

class FoldIndex {
public:
    void Rebuild(const std::string& text);

    const std::vector<FoldRange>& Ranges() const { return ranges_; }

    const FoldRange* RangeStartingAt(int line) const;

private:
    std::vector<FoldRange> ranges_;
};

}
