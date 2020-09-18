#include <catch2/catch.hpp>
#include <vsm/quick_hull.hpp>

#include <random>

#include <iostream>

using namespace vsm;

TEST_CASE("Degenerate Input", "[quick_hull]") {
    // setup random generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    int n_points, n_dims;

    SECTION("2D") {
        n_points = 100;
        n_dims = 2;
    }

    SECTION("3D") {
        n_points = 1000;
        n_dims = 3;
    }

    SECTION("4D") {
        n_points = 1000;
        n_dims = 4;
    }

    std::vector<QuickHull::Point> points;
    points.reserve(n_points);
    for (int i = 0; i < n_points; ++i) {
        QuickHull::Point point;
        point.reserve(n_dims);
        for (int j = 0; j < n_dims; ++j) {
            point.push_back(dis(gen));
        }
        points.push_back(std::move(point));
    }

    auto hull_points = QuickHull::convexHull(points);
    REQUIRE(!hull_points.empty());

    points.front().resize(2 * n_dims, 0);
    auto degenerate_hull_points = QuickHull::convexHull(points);
    REQUIRE(degenerate_hull_points.size() == hull_points.size());
    for (auto point : degenerate_hull_points) {
        point.resize(n_dims);
        REQUIRE(hull_points.count(point));
    }
}

TEST_CASE("Random N-D Points", "[quick_hull]") {
    // setup random generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    int n_points, n_dims;
    SECTION("2D") {
        n_points = 10000;
        n_dims = 2;
    }
    SECTION("3D") {
        n_points = 10000;
        n_dims = 3;
    }
    SECTION("4D") {
        n_points = 10000;
        n_dims = 4;
    }
    SECTION("5D") {
        n_points = 10000;
        n_dims = 5;
    }

    // generate random 2D points
    std::vector<QuickHull::Point> points;
    points.reserve(n_points);
    for (int i = 0; i < n_points; ++i) {
        QuickHull::Point point;
        point.reserve(n_dims);
        for (int j = 0; j < n_dims; ++j) {
            point.push_back(dis(gen));
            // std::cout << point.back() << " ";
        }
        // puts(" ");
        points.push_back(std::move(point));
    }

    auto hull_points = QuickHull::convexHull(points);
    REQUIRE(!hull_points.empty());
#if 0
    puts("\r\nconvex hull");
    for (auto& point : hull_points) {
        for (const auto& coord : point) {
            std::cout << coord << ' ';
        }
        puts("");
    }
#endif

    auto inv_points = points;
    QuickHull::sphereInversion(inv_points, QuickHull::Point(n_dims, 0));
    hull_points = QuickHull::convexHull(inv_points);
    REQUIRE(!hull_points.empty());
    REQUIRE(points.size() == inv_points.size());
#if 0
    puts("\r\ninterior hull");
    for (size_t i = 0; i < inv_points.size(); ++i) {
        if (hull_points.count(inv_points[i])) {
            for (const auto& coord : points[i]) {
                std::cout << coord << ' ';
            }
            puts("");
        }
    }
#endif
}

TEST_CASE("Zero Distance Interior", "[quick_hull]") {
    // setup random generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    int n_points, n_dims;

    SECTION("2D") {
        n_points = 10000;
        n_dims = 2;
    }

    SECTION("3D") {
        n_points = 10000;
        n_dims = 3;
    }

    SECTION("4D") {
        n_points = 10000;
        n_dims = 4;
    }

    // generate random 2D points
    std::vector<QuickHull::Point> points;
    points.reserve(n_points);
    for (int i = 0; i < n_points; ++i) {
        QuickHull::Point point;
        point.reserve(n_dims);
        for (int j = 0; j < n_dims; ++j) {
            point.push_back(dis(gen));
            // std::cout << point.back() << " ";
        }
        // puts(" ");
        points.push_back(std::move(point));
    }

    auto origin = QuickHull::Point(n_dims, 0);
    auto points_with_origin = points;
    points_with_origin.push_back(origin);
    QuickHull::sphereInversion(points, origin);
    QuickHull::sphereInversion(points_with_origin, origin);
    auto hull_points = QuickHull::convexHull(points);
    auto hull_points_with_origin = QuickHull::convexHull(points_with_origin);
    REQUIRE(!hull_points.empty());
    REQUIRE(hull_points_with_origin.size() == hull_points.size() + 1);
    for (auto& hull_point : hull_points) {
        REQUIRE(hull_points_with_origin.count(hull_point));
    }
}
