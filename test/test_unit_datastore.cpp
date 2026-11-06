#include "doctest.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "bennu/devices/field-device/DataStore.hpp"

using bennu::field_device::DataStore;

TEST_CASE("DataStore: storage and retrieval of variant types (int, double, bool)")
{
    DataStore<std::string> store;

    store.addData("pt_int", 42);
    store.addData("pt_double", 3.14159);
    store.addData("pt_bool_t", true);
    store.addData("pt_bool_f", false);

    CHECK(store.getData<int>("pt_int") == 42);
    CHECK(doctest::Approx(store.getData<double>("pt_double")) == 3.14159);
    CHECK(store.getData<bool>("pt_bool_t") == true);
    CHECK(store.getData<bool>("pt_bool_f") == false);

    // Update via setData without timestamp
    CHECK(store.setData("pt_int", 84));
    CHECK(store.getData<int>("pt_int") == 84);

    CHECK(store.setData("pt_double", 2.71828));
    CHECK(doctest::Approx(store.getData<double>("pt_double")) == 2.71828);

    CHECK(store.setData("pt_bool_t", false));
    CHECK(store.getData<bool>("pt_bool_t") == false);

    // Mismatched variant type requests throw std::bad_variant_access
    CHECK_THROWS_AS(store.getData<int>("pt_double"), std::bad_variant_access);
    CHECK_THROWS_AS(store.getData<bool>("pt_int"), std::bad_variant_access);
    CHECK_THROWS_AS(store.getData<double>("pt_bool_t"), std::bad_variant_access);
}

TEST_CASE("DataStore: timestamp tracking via setData and getTimestamp")
{
    DataStore<std::string> store;

    store.addData("sensor1", 10.0);
    // Initial timestamp is 0
    CHECK(store.getTimestamp("sensor1") == 0.0);

    const double ts1 = 1698765432.123;
    CHECK(store.setData("sensor1", 20.0, ts1));
    CHECK(doctest::Approx(store.getData<double>("sensor1")) == 20.0);
    CHECK(doctest::Approx(store.getTimestamp("sensor1")) == ts1);

    const double ts2 = 1698765499.999;
    CHECK(store.setData("sensor1", 30.0, ts2));
    CHECK(doctest::Approx(store.getData<double>("sensor1")) == 30.0);
    CHECK(doctest::Approx(store.getTimestamp("sensor1")) == ts2);

    // setData on nonexistent point returns false and does not create point
    CHECK_FALSE(store.setData("nonexistent", 50.0, ts1));
    CHECK(store.getTimestamp("nonexistent") == 0.0);

    // Re-adding existing point resets timestamp to 0 and updates value
    store.addData("sensor1", 99.0);
    CHECK(doctest::Approx(store.getData<double>("sensor1")) == 99.0);
    CHECK(store.getTimestamp("sensor1") == 0.0);

    // Empty point name is stored and retrieved normally
    store.addData("", 123);
    CHECK(store.hasData(""));
    CHECK(store.getData<int>("") == 123);
}

TEST_CASE("DataStore: default value on nonexistent points")
{
    DataStore<std::string> store;

    CHECK(store.getData<int>("missing") == 0);
    CHECK(store.getData<double>("missing") == 0.0);
    CHECK(store.getData<bool>("missing") == false);
    CHECK(store.getTimestamp("missing") == 0.0);
}

TEST_CASE("DataStore: clear and hasData")
{
    DataStore<std::string> store;

    CHECK_FALSE(store.hasData("p1"));
    CHECK_FALSE(store.hasData("p2"));

    store.addData("p1", 100);
    store.addData("p2", true);

    CHECK(store.hasData("p1"));
    CHECK(store.hasData("p2"));
    CHECK(store.get().size() == 2);

    store.clear();

    CHECK_FALSE(store.hasData("p1"));
    CHECK_FALSE(store.hasData("p2"));
    CHECK(store.get().empty());
}

TEST_CASE("DataStore: concurrent multi-threaded stress test")
{
    DataStore<std::string> store;
    const int numPoints = 20;

    // Initialize points
    for (int i = 0; i < numPoints; ++i)
    {
        store.addData("point_" + std::to_string(i), i * 10);
    }

    std::atomic<bool> start{false};
    std::atomic<bool> done{false};
    std::atomic<int> readErrors{0};

    const int numWriters = 4;
    const int numReaders = 4;
    const int iterationsPerThread = 1000;

    std::vector<std::thread> threads;

    // Launch writer threads
    for (int w = 0; w < numWriters; ++w)
    {
        threads.emplace_back([&store, &start, &done, w, iterationsPerThread, numPoints]() {
            while (!start.load())
            {
                std::this_thread::yield();
            }
            for (int it = 0; it < iterationsPerThread; ++it)
            {
                int ptIdx = (w + it) % numPoints;
                std::string ptName = "point_" + std::to_string(ptIdx);
                double ts = static_cast<double>(it + 1);
                store.setData(ptName, (it + 1) * 10, ts);
            }
        });
    }

    // Launch reader threads
    for (int r = 0; r < numReaders; ++r)
    {
        threads.emplace_back([&store, &start, &done, &readErrors, numPoints]() {
            while (!start.load())
            {
                std::this_thread::yield();
            }
            while (!done.load())
            {
                for (int i = 0; i < numPoints; ++i)
                {
                    std::string ptName = "point_" + std::to_string(i);
                    if (!store.hasData(ptName))
                    {
                        readErrors++;
                    }
                    int val = store.getData<int>(ptName);
                    double ts = store.getTimestamp(ptName);
                    if (val < 0 || ts < 0.0)
                    {
                        readErrors++;
                    }
                }
            }
        });
    }

    // Unleash threads
    start.store(true);

    // Wait for writers
    for (int w = 0; w < numWriters; ++w)
    {
        threads[w].join();
    }
    done.store(true);

    // Wait for readers
    for (int r = numWriters; r < numWriters + numReaders; ++r)
    {
        threads[r].join();
    }

    CHECK(readErrors.load() == 0);
    // Verify points are still present and have non-negative values
    for (int i = 0; i < numPoints; ++i)
    {
        std::string ptName = "point_" + std::to_string(i);
        CHECK(store.hasData(ptName));
        CHECK(store.getData<int>(ptName) > 0);
    }
}
