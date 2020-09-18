#pragma once
#include <limits>
#include <string>
#include <vector>
#include <unordered_set>

namespace vsm {

class QuickHull {
public:
    using Value = float;
    using Point = std::vector<Value>;

    struct PointHash {
        std::size_t operator()(const Point& point) const {
            return std::hash<std::string>()(std::string(reinterpret_cast<const char*>(point.data()),
                    point.size() * sizeof(Point::value_type)));
        }
    };

    using PointSet = std::unordered_set<Point, PointHash>;

    static PointSet convexHull(const std::vector<Point>& points, bool include_coplanar = false);
    static void sphereInversion(std::vector<Point>& points, const Point& origin);
};
}  // namespace vsm
