#include <vsm/quick_hull.hpp>
#include <quickhull.hpp>

namespace vsm {

QuickHull::PointSet QuickHull::computeConvexHull(const std::vector<Point>& points) {
    PointSet hull_points;
    // filter input
    std::vector<Point> filtered_points;
    filtered_points.reserve(points.size());
    for (const auto& point : points) {
        // filter out if any coordinate extends to infinity from input
        if (std::none_of(point.cbegin(), point.cend(), [](Value coord) {
                return std::abs(coord) >= std::numeric_limits<Value>::max();
            })) {
            // append point after filters
            filtered_points.push_back(point);
        } else {
            // add infinite points directly to hull
            hull_points.insert(point);
        }
    }
    for (auto n_dims = points.front().size(); n_dims > 1; --n_dims) {
        // force uniform dimensions
        for (auto& filtered_point : filtered_points) {
            filtered_point.resize(n_dims, 0);
        }
        // instantiate quick hull
        static constexpr auto EPS = std::numeric_limits<Value>::epsilon();
        quick_hull<std::vector<Point>::const_iterator> quick_hull(n_dims, EPS);
        // add points and find initial simplex
        quick_hull.add_points(filtered_points.cbegin(), filtered_points.cend());
        const auto initial_simplex = quick_hull.get_affine_basis();
        // check for degenerate input dimensions
        if (initial_simplex.size() == quick_hull.dimension_ + 1) {
            // compute convex hull
            quick_hull.create_initial_simplex(
                    initial_simplex.cbegin(), std::prev(initial_simplex.cend()));
            quick_hull.create_convex_hull();
            // convert facet to hull points
            for (const auto& facet : quick_hull.facets_) {
                for (const auto& vertex : facet.vertices_) {
                    hull_points.insert(*vertex);
                }
                for (const auto& vertex : facet.coplanar_) {
                    hull_points.insert(*vertex);
                }
            }
            break;
        }
    }
    return hull_points;
}

void QuickHull::sphereInversion(std::vector<Point>& points, const Point& origin) {
    for (auto& point : points) {
        // center points around origin
        point.resize(origin.size(), 0);
        for (size_t i = 0; i < origin.size(); ++i) {
            point[i] -= origin[i];
        }
        // calculate distance from origin
        Value r2 = 0;
        for (auto coord : point) {
            r2 += coord * coord;
        }
        // divide by zero check
        if (r2 == 0) {
            point = Point(point.size(), std::numeric_limits<Value>::max());
            continue;
        }
        // invert distance from origin
        for (auto& coord : point) {
            coord /= r2;
        }
    }
}

}  // namespace vsm
