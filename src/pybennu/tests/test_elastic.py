from datetime import datetime, timezone
from unittest.mock import MagicMock, patch

import pytest
from elasticsearch import helpers

from pybennu.elastic import (
    ES_BASE_TYPE_MAPPING,
    ElasticPusher,
)
from pybennu.settings import ElasticSettings, PybennuSettings

# =============================================================================
# Helper Fixtures & Builders
# =============================================================================


def make_conf(
    host: str = "http://localhost:9200",
    index_basename: str = "test-index",
    threads: int = 1,
) -> PybennuSettings:
    return PybennuSettings(
        elastic=ElasticSettings(
            enabled=True,
            host=host,
            index_basename=index_basename,
            num_threads=threads,
        ),
        sceptre_experiment="test_exp",
        sceptre_topology="test_topo",
        sceptre_scenario="test_scen",
        simulator="power_world",
        server_endpoint="tcp://127.0.0.1:5555",
        publish_endpoint="udp://239.0.0.1:40000",
    )


# =============================================================================
# Schema Mapping Tests
# =============================================================================


def test_es_base_type_mapping_date_fields():
    """Verify that all temporal fields are mapped to Elasticsearch date types."""
    date_fields = ["@timestamp", "rtds_time", "sceptre_time", "simulator_time"]
    for field in date_fields:
        assert field in ES_BASE_TYPE_MAPPING
        assert ES_BASE_TYPE_MAPPING[field] == {"type": "date"}

    assert ES_BASE_TYPE_MAPPING["event"]["properties"]["ingested"]["type"] == "date"


def test_es_base_type_mapping_ecs_structure():
    """Verify standard Elastic Common Schema (ECS) fields are properly typed."""
    # ECS version
    assert ES_BASE_TYPE_MAPPING["ecs"]["properties"]["version"]["type"] == "keyword"

    # Observer
    obs = ES_BASE_TYPE_MAPPING["observer"]["properties"]
    assert obs["hostname"]["type"] == "keyword"
    assert obs["ip"]["type"] == "ip"
    assert obs["name"]["type"] == "keyword"
    assert obs["geo"]["properties"]["timezone"]["type"] == "keyword"

    # Network
    net = ES_BASE_TYPE_MAPPING["network"]["properties"]
    assert net["protocol"]["type"] == "keyword"
    assert net["transport"]["type"] == "keyword"

    # PMU metadata
    pmu = ES_BASE_TYPE_MAPPING["pmu"]["properties"]
    assert pmu["name"]["type"] == "keyword"
    assert pmu["label"]["type"] == "keyword"
    assert pmu["ip"]["type"] == "ip"
    assert pmu["port"]["type"] == "integer"
    assert pmu["id"]["type"] == "long"


def test_es_base_type_mapping_measurement_and_modbus():
    """Verify PMU measurements and Modbus protocol field mappings."""
    # Measurement
    meas = ES_BASE_TYPE_MAPPING["measurement"]["properties"]
    assert meas["stream"]["type"] == "byte"
    assert meas["frequency"]["type"] == "double"
    assert meas["dfreq"]["type"] == "double"
    phasor = meas["phasor"]["properties"]
    assert phasor["real"]["type"] == "double"
    assert phasor["angle"]["type"] == "double"

    # Modbus
    mb = ES_BASE_TYPE_MAPPING["modbus"]["properties"]
    assert mb["register"]["type"] == "integer"
    assert mb["name"]["type"] == "keyword"
    assert mb["register_type"]["type"] == "keyword"
    assert mb["human_value"]["type"] == "keyword"


# =============================================================================
# Document Formatting & Enqueuing Tests
# =============================================================================


def test_msg_additions_formatting():
    """Verify default message additions contain ECS, agent, observer, and sceptre context."""
    conf = make_conf()
    pusher = ElasticPusher(conf)

    additions = pusher.msg_additions
    assert additions["ecs"]["version"] == "8.11.0"
    assert additions["agent"]["type"] == "sceptre-provider"
    assert "version" in additions["agent"]
    assert additions["sceptre"]["experiment"] == "test_exp"
    assert additions["sceptre"]["topology"] == "test_topo"
    assert additions["sceptre"]["scenario"] == "test_scen"
    assert additions["sceptre"]["provider"] == "power_world"


def test_enqueue_documents_into_queue():
    """Verify documents are enqueued into data_queue."""
    conf = make_conf()
    pusher = ElasticPusher(conf)

    assert pusher.data_queue.empty()
    doc1 = {"@timestamp": datetime.now(timezone.utc), "value": 42.0}
    doc2 = {"@timestamp": datetime.now(timezone.utc), "value": 99.0}

    pusher.enqueue([doc1, doc2])
    assert not pusher.data_queue.empty()
    queued_batch = pusher.data_queue.get_nowait()
    assert len(queued_batch) == 2
    assert queued_batch[0]["value"] == 42.0
    assert queued_batch[1]["value"] == 99.0


# =============================================================================
# ElasticPusher Bulk Indexing & Mocking Tests
# =============================================================================


@patch("pybennu.elastic.helpers.bulk")
def test_send_messages_bulk_success(mock_bulk):
    """Verify send_messages structures actions, creates index if needed, and calls helpers.bulk."""
    mock_bulk.return_value = (2, [])  # (count, errors)

    mock_es = MagicMock()
    mock_es.indices.exists.return_value = False

    conf = make_conf(index_basename="sceptre-pmu")
    handler = ElasticPusher(conf)

    ts = datetime(2026, 11, 6, 12, 0, 0, tzinfo=timezone.utc)
    messages = [
        {"@timestamp": ts, "measurement": {"frequency": 60.01}},
        {"@timestamp": ts, "measurement": {"frequency": 59.99}},
    ]

    success = handler.send_messages(mock_es, messages)
    assert success is True

    expected_index = "sceptre-pmu-2026.11.06"
    # Index was not in cache and didn't exist, so exists and create called
    mock_es.indices.exists.assert_called_once_with(index=expected_index)
    mock_es.indices.create.assert_called_once()
    assert expected_index in handler.index_cache

    # Verify helpers.bulk called with actions
    assert mock_bulk.call_count == 1
    args, _ = mock_bulk.call_args
    passed_es = args[0]
    actions = args[1]
    assert passed_es == mock_es
    assert len(actions) == 2
    assert actions[0]["_index"] == expected_index
    assert actions[0]["_source"]["measurement"]["frequency"] == 60.01
    assert "event" in actions[0]["_source"]
    assert "ingested" in actions[0]["_source"]["event"]
    assert actions[0]["_source"]["sceptre"]["experiment"] == "test_exp"


@patch("pybennu.elastic.helpers.bulk")
def test_send_messages_index_cache_skips_redundant_creation(mock_bulk):
    """Verify that cached indices do not trigger repeated ES indices.exists checks."""
    mock_bulk.return_value = (1, [])
    mock_es = MagicMock()

    conf = make_conf(index_basename="sceptre-fast")
    handler = ElasticPusher(conf)

    ts = datetime(2026, 11, 6, 12, 0, 0, tzinfo=timezone.utc)
    index_name = "sceptre-fast-2026.11.06"
    handler.index_cache.add(index_name)

    messages = [{"@timestamp": ts, "measurement": {"frequency": 60.0}}]
    handler.send_messages(mock_es, messages)

    # indices.exists should NOT be called because index was already cached
    mock_es.indices.exists.assert_not_called()
    mock_es.indices.create.assert_not_called()
    assert mock_bulk.call_count == 1


@patch("pybennu.elastic.helpers.bulk")
def test_send_messages_bulk_index_error_raises_runtime_error(mock_bulk):
    """Verify that helpers.BulkIndexError is raised as RuntimeError."""
    mock_bulk.side_effect = helpers.BulkIndexError(
        "2 document(s) failed to index.", errors=[{"index": {"error": "type mismatch"}}]
    )
    mock_es = MagicMock()
    mock_es.indices.exists.return_value = True

    conf = make_conf()
    handler = ElasticPusher(conf)

    ts = datetime(2026, 11, 6, 12, 0, 0, tzinfo=timezone.utc)
    messages = [{"@timestamp": ts, "data": "bad"}]

    with pytest.raises(
        RuntimeError, match="errors happened during elastic bulk ingest"
    ):
        handler.send_messages(mock_es, messages)


@patch("pybennu.elastic.helpers.bulk")
def test_send_messages_unexpected_exception_returns_false(mock_bulk):
    """Verify that generic exceptions during bulk ingest return False gracefully."""
    mock_bulk.side_effect = ConnectionError("Network unreachable")
    mock_es = MagicMock()
    mock_es.indices.exists.return_value = True

    conf = make_conf()
    handler = ElasticPusher(conf)

    ts = datetime(2026, 11, 6, 12, 0, 0, tzinfo=timezone.utc)
    messages = [{"@timestamp": ts, "data": "val"}]

    result = handler.send_messages(mock_es, messages)
    assert result is False


# =============================================================================
# Clean Shutdown & Queue Flushing Tests
# =============================================================================


@patch("pybennu.elastic.helpers.bulk")
def test_queue_drain_sends_all_messages(mock_bulk):
    """Verify draining all queued batches and publishes them to ES."""
    mock_bulk.return_value = (3, [])
    mock_es = MagicMock()
    mock_es.indices.exists.return_value = True

    conf = make_conf()
    handler = ElasticPusher(conf)

    ts = datetime(2026, 11, 6, 12, 0, 0, tzinfo=timezone.utc)
    handler.enqueue([{"@timestamp": ts, "id": 1}, {"@timestamp": ts, "id": 2}])
    handler.enqueue([{"@timestamp": ts, "id": 3}])

    assert handler.data_queue.qsize() == 2

    # Drain the queue the way a shutdown path would, then bulk-send it.
    messages = []
    while not handler.data_queue.empty():
        messages.extend(handler.data_queue.get_nowait())
    assert len(messages) == 3
    assert handler.send_messages(mock_es, messages)
    assert handler.data_queue.empty()
    assert mock_bulk.call_count == 1
