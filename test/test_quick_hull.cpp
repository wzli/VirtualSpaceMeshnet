#include <catch2/catch.hpp>
#include <vsm/quick_hull.hpp>

#include <random>

#include <iostream>

using namespace vsm;

TEST_CASE("Simple 2D Hull", "[quick_hull]") {
    // setup random generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    static constexpr auto N_POINTS = 50;
    static constexpr auto N_DIM = 2;

    std::cout << N_POINTS << " original points\r\n";

    // generate random 2D points
    std::vector<QuickHull::Point> points;
    points.reserve(N_POINTS);
    for (int i = 0; i < N_POINTS; ++i) {
        QuickHull::Point point;
        point.reserve(N_DIM);
        for (int j = 0; j < N_DIM; ++j) {
            point.push_back(dis(gen));
            std::cout << point.back() << " ";
        }
        puts(" ");
        points.push_back(std::move(point));
    }

    puts("\r\ncomputing convex hull");
    auto hull_points = QuickHull::computeConvexHull(points);
    REQUIRE(!hull_points.empty());
    for (auto& point : hull_points) {
        for (const auto& coord : point) {
            std::cout << coord << ' ';
        }
        puts("");
    }

    puts("\r\ncomputing interior hull");
    auto inv_points = points;
    QuickHull::sphereInversion(inv_points, QuickHull::Point(N_DIM, 0));
    hull_points = QuickHull::computeConvexHull(inv_points);
    REQUIRE(!hull_points.empty());
    REQUIRE(points.size() == inv_points.size());
    for (size_t i = 0; i < inv_points.size(); ++i) {
        if (hull_points.count(inv_points[i])) {
            for (const auto& coord : points[i]) {
                std::cout << coord << ' ';
            }
            puts("");
        }
    }

    // TODO: test degenerate dims, test more dims, test non uniform dims, test zero distance invert
}
