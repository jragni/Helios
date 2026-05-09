# Testing Patterns

**Analysis Date:** 2026-05-09

## Current State

**No formal test suite exists.** The project has no `tests/` directory and no test files (`*.test.py`, `*.spec.py`).

However, **pytest is installed** as a dev dependency in `pyproject.toml`:
```toml
[dependency-groups]
dev = [
    "pytest>=9.0.3",
]
```

Instead of pytest, every module uses **module-level smoke tests** in `if __name__ == "__main__":` blocks.

## Smoke Test Pattern (Current Approach)

Each module implements a lightweight integration check accessible via:
```bash
uv run python -m helios.<module_name>
```

### Structure Template

Every smoke test follows this pattern:

1. **Configure logging** — set up basicConfig for visibility
2. **Create minimal instances** — instantiate the main class/function
3. **Run a representative operation** — call the public method with test data
4. **Assert correctness** — verify return values or state
5. **Handle expected failures** — skip gracefully if dependencies unavailable
6. **Print status** — output `OK`, `SKIP`, or raise on failure

### Examples

**`src/helios/perception/mqtt_sonar_source.py:178-183`** — Mock-based test:
```python
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
    src = MockDistanceSource()
    val = src.get_distance_mm()
    assert val == PLACEHOLDER_DISTANCE_MM, f"expected {PLACEHOLDER_DISTANCE_MM}, got {val!r}"
    print(f"MQTT_SONAR_SOURCE_OK: mock distance = {val} mm")
```

**`src/helios/output/haptic.py:150-157`** — No broker required:
```python
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
    print("ALL_CHANNELS:", ALL_CHANNELS)
    print("topic for WRIST_TOP:", _topic(WRIST_TOP))
    payload = json.dumps({"d": 200, "ms": 180}, separators=(",", ":"))
    print("example payload:", payload)
    print("HAPTIC_OK")
```

**`src/helios/vision/router.py:155-196`** — Handles unavailable Ollama gracefully:
```python
if __name__ == "__main__":
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )

    img = np.full((240, 320, 3), 128, dtype=np.uint8)
    cv2.rectangle(img, (80, 60), (240, 180), (0, 0, 255), -1)

    try:
        router = VisionRouter(use_openai=False)
        text = router.describe_scene(img)
        if not isinstance(text, str) or len(text.strip()) < 5:
            print(
                f"FAIL: local route returned bad value: {text!r}",
                file=sys.stderr,
            )
            sys.exit(2)
        print(f"LOCAL_OK: {text[:120]}")
    except Exception as exc:
        print(
            f"SKIP: Ollama unavailable — {exc!r}. "
            "Start `ollama serve` and re-run to verify the local path.",
            file=sys.stderr,
        )

    print("INFO: use_openai=True path now requires OPENAI_API_KEY; ...")
    print("VisionRouter __main__ integration check PASSED")
```

**`src/helios/perception_loop.py:253-282`** — Full-integration smoke test with CLI:
```python
def main() -> int:
    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    ap = argparse.ArgumentParser()
    ap.add_argument("--duration", type=float, default=10.0, ...)
    ap.add_argument("--prompts", type=str, default=",".join(DEFAULT_PROMPTS))
    ap.add_argument("--show", action="store_true", ...)
    ap.add_argument("--mjpeg", type=str, default=None, ...)
    ap.add_argument("--device", type=int, default=0, ...)
    args = ap.parse_args()
    prompts = [p.strip() for p in args.prompts.split(",") if p.strip()]
    try:
        summary = run(duration_s=args.duration, prompts=prompts, show=args.show,
                      mjpeg_url=args.mjpeg, device=args.device)
    except RuntimeError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2
    print("\nSummary:")
    for k, v in summary.items():
        print(f"  {k}: {v}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
```

## Run Smoke Tests

```bash
# Test a single module
uv run python -m helios.output.haptic

# Test perception loop with camera
uv run python -m helios.perception_loop --duration 10 --show

# Test vision router (requires Ollama running)
uv run python -m helios.vision.router

# Test MQTT sonar source
uv run python -m helios.perception.mqtt_sonar_source
```

## Test Data & Fixtures

**Synthetic frames for vision tests:**
```python
# src/helios/vision/router.py:169
img = np.full((240, 320, 3), 128, dtype=np.uint8)
cv2.rectangle(img, (80, 60), (240, 180), (0, 0, 255), -1)
```

**Mock sources for frame/distance tests:**
- `MockDistanceSource` in `src/helios/perception/mqtt_sonar_source.py:45-56`
- `WebcamSource` doubles as a test fixture in tests

**Constants as test data:**
- `DEFAULT_PROMPTS` in `src/helios/perception/yolo_world.py:15-25` — known object classes
- `PLACEHOLDER_DISTANCE_CM` in `src/helios/perception_loop.py:35`

## Test Coverage Gaps (Priority Opportunities)

**High priority — core perception pipeline not covered:**
- `YoloWorld.detect()` — no unit tests for YOLO detection, prompt-switching logic
- `HandTracker.detect_hands()` — no unit tests for MediaPipe hand detection
- `WebcamSource` threading behavior — no concurrent access tests
- `HapticPublisher` — no message publication verification

**Medium priority — cloud fallback logic:**
- `VisionRouter` offline window (D-28) — no state transition tests
- `VisionRouter` fallback announcement (D-27) — no replay/skipping logic coverage
- OpenAI API error handling in `router.py` — mock API responses not tested

**Medium priority — data encoding/validation:**
- `LLavaClient._encode()` — edge cases like non-BGR frames, invalid sizes
- MQTT payload parsing in `MQTTSonarSource._on_message()` — malformed payloads

**Low priority — smoke tests already cover:**
- `MQTTSonarSource` basic instantiation and mock distance
- `HapticPublisher` constant definitions and topic generation
- `VisionRouter` local (LLaVA) path happy path

## Recommended Test Scaffolding

### Directory Structure
```
tests/
├── __init__.py
├── conftest.py                          # pytest fixtures
├── unit/
│   ├── test_perception_yolo.py
│   ├── test_perception_hands.py
│   ├── test_vision_router.py
│   ├── test_output_haptic.py
│   └── test_cloud_openai_client.py
├── integration/
│   ├── test_perception_loop.py          # end-to-end frame → detection
│   └── test_vision_router_fallback.py   # OpenAI ↔ LLaVA switchover
└── fixtures/
    ├── synthetic_frames.py              # test image generators
    ├── mock_mqtt.py                     # mock MQTT broker
    └── mock_openai.py                   # mock OpenAI responses
```

### Fixture Pattern (pytest)
```python
# tests/conftest.py
import pytest
import numpy as np

@pytest.fixture
def synthetic_frame():
    """Generate a 320x240 test frame with a red rectangle."""
    img = np.full((240, 320, 3), 128, dtype=np.uint8)
    cv2.rectangle(img, (80, 60), (240, 180), (0, 0, 255), -1)
    return img

@pytest.fixture
def mock_webcam_source(synthetic_frame):
    """Return a mock FrameSource that always yields the same frame."""
    from helios.mocks.webcam_source import WebcamSource
    source = WebcamSource()
    source._latest = synthetic_frame
    return source
```

### Unit Test Example
```python
# tests/unit/test_perception_yolo.py
import pytest
from helios.perception.yolo_world import YoloWorld, DEFAULT_PROMPTS

def test_yolo_detects_with_known_prompts(synthetic_frame):
    yolo = YoloWorld()
    bboxes = yolo.detect(synthetic_frame, DEFAULT_PROMPTS)
    assert isinstance(bboxes, list)
    # Either empty list or list of Bbox tuples
    for bbox in bboxes:
        assert hasattr(bbox, "label")
        assert hasattr(bbox, "xyxy")
        assert hasattr(bbox, "score")

def test_yolo_switches_prompts_efficiently(synthetic_frame):
    yolo = YoloWorld()
    # First detect with prompts A
    yolo.detect(synthetic_frame, DEFAULT_PROMPTS)
    assert yolo._current_prompts == DEFAULT_PROMPTS
    # Second detect with same prompts — no re-set
    yolo.detect(synthetic_frame, DEFAULT_PROMPTS)
    # Third detect with different prompts — re-set called
    other_prompts = ["cat", "dog"]
    yolo.detect(synthetic_frame, other_prompts)
    assert yolo._current_prompts == other_prompts
```

### Integration Test Example
```python
# tests/integration/test_perception_loop.py
import pytest
from helios.perception_loop import perception_iter, PerceptionTick
from helios.perception.yolo_world import YoloWorld
from helios.perception.hands import HandTracker

def test_perception_iter_yields_ticks(mock_webcam_source):
    yolo = YoloWorld()
    hands = HandTracker()
    iterator = perception_iter(mock_webcam_source, yolo, hands)
    
    tick = next(iterator)
    assert isinstance(tick, PerceptionTick)
    assert tick.ts > 0
    assert isinstance(tick.bboxes, list)
    assert isinstance(tick.hands, list)
    assert tick.latency_ms >= 0
```

### Mock Pattern (for cloud services)
```python
# tests/fixtures/mock_openai.py
from unittest.mock import Mock, patch

@pytest.fixture
def mock_openai_client():
    mock = Mock()
    mock.chat.completions.create.return_value = Mock(
        choices=[Mock(message=Mock(content="A desk with a lamp"))]
    )
    return mock

def test_vision_router_cloud_path(synthetic_frame, mock_openai_client):
    with patch('helios.cloud.openai_client.get_client', return_value=mock_openai_client):
        router = VisionRouter(use_openai=True)
        result = router.describe_scene(synthetic_frame)
        assert result == "A desk with a lamp"
```

## Running Tests with pytest

```bash
# Install pytest from dev dependencies
uv sync --group dev

# Run all tests
uv run pytest

# Run with verbose output
uv run pytest -v

# Run single test file
uv run pytest tests/unit/test_perception_yolo.py

# Run single test
uv run pytest tests/unit/test_perception_yolo.py::test_yolo_detects_with_known_prompts

# Watch mode (requires pytest-watch)
uv run pytest-watch

# Coverage report
uv run pytest --cov=src/helios --cov-report=html
```

## Mocking Guidelines

**What to mock:**
- External services (OpenAI API, Ollama server, MQTT broker)
- Hardware (webcam, microphone, MQTT device connections)
- Time-dependent behavior (use `freezegun` or `unittest.mock.patch`)
- File I/O and network I/O

**What NOT to mock:**
- Core business logic — test the real algorithms
- Data structures — test with actual frame shapes, bbox formats
- Threading primitives — test actual Lock/Event behavior

**Mocking library:**
- Use `unittest.mock` (standard library)
- No third-party mocking required yet

## CI/CD Integration

Not yet configured. When adding CI:
- Run smoke tests via `python -m helios.<module_name>` for quick validation
- Run full pytest suite before merge
- Collect coverage reports
- Require minimum coverage (suggest 70% for core perception modules)

---

*Testing analysis: 2026-05-09*
