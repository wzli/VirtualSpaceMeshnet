#include <vsm/quick_hull.hpp>
#include <quickhull.hpp>

namespace vsm {

QuickHull::PointSet QuickHull::convexHull(
        const std::vector<Point>& points, bool include_coplanar, Value epsilon) {
    PointSet hull_points;
    // reject empty input
    if (points.empty() || points.front().empty()) {
        return hull_points;
    }
    // filter input
    std::vector<Point> filtered_points;
    filtered_points.reserve(points.size());
    for (const auto& point : points) {
        // filter out if any coordinate extends to infinity from input
        if (std::any_of(point.cbegin(), point.cend(), [](Value coord) {
                return std::abs(coord) >= std::numeric_limits<Value>::max();
            })) {
            // add infinite points directly to hull
            hull_points.insert(point);
        } else {
            // append point after filters
            filtered_points.push_back(point);
        }
    }
    for (auto n_dims = filtered_points.front().size(); n_dims > 1; --n_dims) {
        // force uniform dimensions
        for (auto& filtered_point : filtered_points) {
            filtered_point.resize(n_dims, 0);
        }
        // skip empty dimensions
        if (std::all_of(filtered_points.cbegin(), filtered_points.cend(),
                    [&](const Point& filtered_point) {
                        return std::abs(filtered_point.back() - filtered_points.front().back()) <
                               epsilon;
                    })) {
            continue;
        }
        // return if fewer points than required for initial basis
        if (filtered_points.size() <= n_dims) {
            for (const auto& filtered_point : filtered_points) {
                hull_points.insert(filtered_point);
            }
            break;
        }
        // instantiate quick hull
        quick_hull<std::vector<Point>::const_iterator> quick_hull(n_dims, epsilon);
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
                if (include_coplanar) {
                    for (const auto& vertex : facet.coplanar_) {
                        hull_points.insert(*vertex);
                    }
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
