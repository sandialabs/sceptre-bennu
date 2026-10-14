#include "doctest.h"

// DataManager.hpp does not include these itself
#include <chrono>
#include <memory>

#include "bennu/devices/field-device/DataManager.hpp"

using bennu::field_device::DataManager;

TEST_CASE("snapshot returns copies")
{
    DataManager dm;
    dm.addUpdatedBinaryTag("b", true);
    dm.addUpdatedAnalogTag("a", 1.5);

    auto bins = dm.snapshotUpdatedBinaryTags();
    auto anals = dm.snapshotUpdatedAnalogTags();
    dm.addUpdatedBinaryTag("b", false);
    dm.addUpdatedAnalogTag("a", 2.5);

    CHECK(bins.at("b") == true);
    CHECK(anals.at("a") == 1.5);
    CHECK(dm.snapshotUpdatedBinaryTags().at("b") == false);
}

TEST_CASE("compare-and-erase keeps a newer value")
{
    DataManager dm;
    dm.addUpdatedBinaryTag("b", true);
    dm.addUpdatedAnalogTag("a", 1.0);

    // value changed after the snapshot was sent: entry must survive
    dm.addUpdatedBinaryTag("b", false);
    dm.addUpdatedAnalogTag("a", 2.0);
    dm.deleteBinaryTag("b", true);
    dm.deleteAnalogTag("a", 1.0);
    CHECK(dm.isUpdatedBinaryTag("b"));
    CHECK(dm.isUpdatedAnalogTag("a"));

    // value unchanged: entry is erased
    dm.deleteBinaryTag("b", false);
    dm.deleteAnalogTag("a", 2.0);
    CHECK_FALSE(dm.isUpdatedBinaryTag("b"));
    CHECK_FALSE(dm.isUpdatedAnalogTag("a"));

    // deleting a missing tag is a no-op
    dm.deleteBinaryTag("nope", true);
}

TEST_CASE("internal tags are not touched by external point writes")
{
    DataManager dm;
    dm.addInternalData<bool>("int-tag", false);
    dm.addUpdatedBinaryTag("int-tag", true);
    dm.updateInternalData();
    CHECK(dm.getDataByTag<bool>("int-tag") == true);
    // applied internal updates leave the queue so logic can write them again
    CHECK_FALSE(dm.isUpdatedBinaryTag("int-tag"));

    // external (non-internal) tags stay queued for the OutputModule
    dm.addUpdatedBinaryTag("ext-tag", true);
    dm.updateInternalData();
    CHECK(dm.isUpdatedBinaryTag("ext-tag"));
}

TEST_CASE("internal analog update advances every cycle")
{
    DataManager dm;
    dm.addInternalData<double>("counter", 0.0);
    for (int i = 1; i <= 3; i++)
    {
        CHECK_FALSE(dm.isUpdatedAnalogTag("counter"));
        dm.addUpdatedAnalogTag("counter", dm.getDataByTag<double>("counter") + 1);
        dm.updateInternalData();
        CHECK(dm.getDataByTag<double>("counter") == i);
    }
}

TEST_CASE("isBinary / isAnalog")
{
    DataManager dm;
    dm.addBinaryTag("b");
    dm.addAnalogTag("a");
    CHECK(dm.isBinary("b"));
    CHECK_FALSE(dm.isBinary("a"));
    CHECK(dm.isAnalog("a"));
    CHECK_FALSE(dm.isAnalog("b"));
    CHECK_FALSE(dm.isBinary("zzz"));
    CHECK(dm.getBinaryTags().size() == 1);
}

TEST_CASE("setDataByPoint / hasPoint / setDataByTag")
{
    DataManager dm;
    dm.addExternalData<double>("id1", "pt1");
    REQUIRE(dm.addTagToPointMapping("tag1", "id1"));
    CHECK_FALSE(dm.addTagToPointMapping("tag2", "missing-id"));

    CHECK(dm.hasPoint("pt1"));
    CHECK_FALSE(dm.hasPoint("missing"));
    CHECK(dm.setDataByPoint<double>("pt1", 3.25));
    CHECK_FALSE(dm.setDataByPoint<double>("missing", 1.0));
    CHECK(dm.getDataByTag<double>("tag1") == 3.25);
    CHECK(dm.setDataByTag<double>("tag1", 4.5));
    CHECK(dm.getDataByTag<double>("tag1") == 4.5);
}
