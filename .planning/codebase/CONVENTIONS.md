# Coding Conventions

**Analysis Date:** 2026-05-09

## Module Structure

Every module in `src/helios/` follows this consistent pattern:

1. **Docstring at top** — Module-level docstring (triple quotes) explaining purpose, decisions, and usage examples
2. **Future annotations** — `from __future__ import annotations` (PEP 563) on line 1 after docstring
3. **Stdlib imports** — Standard library modules (logging, os, threading, etc.)
4. **Third-party imports** — External packages (cv2, numpy, paho.mqtt, etc.)
5. **Local imports** — Relative imports from sibling modules (`.protocol`, `.prompts`, etc.)
6. **Logger** — `log = logging.getLogger(__name__)` immediately after imports
7. **Constants** — UPPER_SNAKE_CASE constants defined at module level
8. **Classes and functions** — Implementation follows
9. **__all__ export list** — Explicit public API (when module exports are used)
10. **Smoke test block** — `if __name__ == "__main__":` integration check (see Testing Patterns)

### Example from `src/helios/perception/mqtt_sonar_source.py`:
```python
"""MQTT-subscriber sonar distance source.

Sonar (HC-SR04) replaces laser ToF...
"""

from __future__ import annotations

import logging
import os
import threading
from typing import Protocol, runtime_checkable

import paho.mqtt.client as mqtt

log = logging.getLogger(__name__)

PLACEHOLDER_DISTANCE_MM: int = 1000
DEFAULT_DISTANCE_TOPIC: str = "helios/sonar/distance_mm"
DEFAULT_BROKER: str = os.environ.get("MQTT_BROKER", "helios.local")

@runtime_checkable
class DistanceSource(Protocol):
    ...

class MQTTSonarSource:
    ...

__all__ = ["DistanceSource", "MockDistanceSource", "MQTTSonarSource", ...]

if __name__ == "__main__":
    # Integration check
    ...
```

## Naming Patterns

**Files:**
- Lowercase with underscores: `mqtt_sonar_source.py`, `llava_client.py`, `perception_loop.py`
- Underscore prefix for internal modules: `_fallback_audio.py`

**Classes:**
- PascalCase: `WebcamSource`, `HapticPublisher`, `YoloWorld`, `VisionRouter`, `MQTTSonarSource`
- Mock classes use `Mock` prefix: `MockDistanceSource`, `MockWebcamSource`

**Functions:**
- Lowercase with underscores: `downsample()`, `perception_iter()`, `_ensure_model()`, `get_latest_frame()`
- Private/internal functions prefixed with single underscore: `_on_connect()`, `_on_message()`, `_callback()`, `_loop()`
- Module-level helpers use descriptive names: `describe_scene()`, `transcribe()`, `speak()`

**Variables:**
- Lowercase with underscores: `frame_copy`, `describe_busy`, `caption_until`
- Private instance attributes prefixed: `self._lock`, `self._latest`, `self._client`, `self._thread`
- Public attributes (rare) use descriptive names: `self.device`, `self.model`, `self.use_openai`

**Constants:**
- UPPER_SNAKE_CASE at module level: `DEFAULT_PROMPTS`, `DEFAULT_BROKER`, `DEFAULT_DUTY`, `PLACEHOLDER_DISTANCE_CM`, `TARGET_SIZE`, `WRIST_TOP`
- All defaults grouped near top: `DEFAULT_SCENE_PROMPT`, `DEFAULT_PULSE_MS`, `DEFAULT_SAMPLERATE`

**Type Aliases:**
- PascalCase when assigned: `FrameInput = Union[np.ndarray, bytes]`

## Type Hints

**Modern style (Python 3.10+):**
- Union syntax: `dict | None`, `str | int`, `list[str]`
- Built-in generics: `list[Bbox]`, `dict[int, str]`, `tuple[int, int, int, int]`
- Optional shorthand: `frame: np.ndarray | None`

**Named types:**
- `NamedTuple` for lightweight data: `Bbox`, `HandLandmarks` in `src/helios/perception/protocol.py`
- `Protocol` with `@runtime_checkable` for contracts: `FrameSource`, `DistanceSource`

**Type aliases for clarity:**
```python
FrameInput = Union[np.ndarray, bytes]  # From llava_client.py, router.py
```

**Function signatures:**
- Full type hints on parameters and return types
- Descriptive parameter names that self-document intent: `frame: np.ndarray`, `prompts: list[str]`, `duration_s: float`
- Use `-> None` explicitly; never omit return type

**Example from `src/helios/perception_loop.py`:**
```python
def perception_iter(
    source: FrameSource,
    yolo: YoloWorld,
    hands: HandTracker,
    prompts: list[str] | None = None,
) -> Iterator[PerceptionTick]:
    ...

def draw_overlay(display: np.ndarray,
                 small_size: tuple[int, int],
                 bboxes: list[Bbox],
                 hands: list[HandLandmarks],
                 distance_cm: float,
                 latency_ms: float,
                 fps: float,
                 caption: str | None = None) -> np.ndarray:
    ...
```

## Logging

**Logger per module:**
- Every module declares `log = logging.getLogger(__name__)` after imports
- Never use bare `print()` except in `__main__` smoke tests

**Log message format:**
- Class-prefix style when logging from instance methods: `log.info("HapticPublisher: connected to %s:%d", ...)` (see `src/helios/output/haptic.py:133`)
- Class name prevents ambiguity when same module has multiple classes
- Use `%s` formatting (not f-strings) for lazy evaluation

**Log levels:**
- `log.info()` — operational events: connections, subscriptions, prompt changes, iterations
- `log.warning()` — recoverable issues: bad channels, connection failures, fallbacks
- `log.debug()` — detailed diagnostic: frame properties, response lengths

**Examples from codebase:**
```python
# src/helios/output/haptic.py:133
log.info("HapticPublisher: connected to %s:%d", self._broker, self._port)

# src/helios/perception/mqtt_sonar_source.py:134
log.info("MQTTSonarSource: connected to %s:%d, subscribed to %s + %s", ...)

# src/helios/perception/yolo_world.py:54
log.info("YoloWorld prompts -> %s", prompts)

# src/helios/vision/llava_client.py:67
log.debug("describe_scene: %d-char response", len(text))
```

## Error Handling

**Explicit exception types:**
- Raise `RuntimeError` with complete, literal information
- Include environment variable names in error text (D-03 from LESSONS_LEARNED.md)
- Never bare `except:` or `except Exception:`

**Pattern for missing environment variables:**
```python
# src/helios/cloud/openai_client.py:19-35
KEY_MISSING_MSG: str = "OPENAI_API_KEY not set. Add it to .env at repo root."

@lru_cache(maxsize=1)
def get_client() -> OpenAI:
    api_key = os.environ.get("OPENAI_API_KEY")
    if not api_key:
        raise RuntimeError(KEY_MISSING_MSG)
    return OpenAI(api_key=api_key)
```

**Pattern for resource failures (literal env var names):**
```python
# src/helios/mocks/webcam_source.py:45
raise RuntimeError(
    f"cv2.VideoCapture({self.device}) failed to open — "
    "is the camera permission granted to Terminal/Python? "
    "(System Settings → Privacy & Security → Camera)"
)

# src/helios/perception_loop.py:153
raise RuntimeError(
    "No frame within 2.5 s — camera permission denied (webcam) or "
    "MJPEG stream unreachable."
)
```

**Type validation errors:**
```python
# src/helios/vision/llava_client.py:34-40
raise TypeError(f"frame must be np.ndarray or bytes, got {type(frame).__name__}")
raise ValueError(f"frame must be HxWx3 BGR, got shape {frame.shape}")
raise RuntimeError("cv2.imencode failed on frame")
```

**Graceful degradation (with logging):**
```python
# src/helios/vision/router.py:136-149
except (openai.APITimeoutError, openai.APIError, openai.RateLimitError) as exc:
    log.warning("OpenAI vision failed (%s) — falling back to LLaVA", type(exc).__name__)
    self._offline_until = now + self._OFFLINE_WINDOW_S
    if not self._fallback_announced:
        self._fallback_announced = True
        try:
            play_offline_cue()
        except Exception as audio_exc:
            log.warning("offline cue playback failed: %r", audio_exc)
    return self._llava.describe_scene(frame, prompt)
```

Note: `except Exception:` with `# noqa: BLE001` is used only when catching all exceptions for resilience.

## Threading

**Lock management:**
- Use `threading.Lock()` for simple mutual exclusion
- Use `threading.RLock()` when the same thread may re-acquire (example: `YoloWorld._lock` in `src/helios/perception/yolo_world.py:48`)
- Use `threading.Event()` for signaling stop/start conditions

**Context manager pattern (always preferred):**
```python
with self._lock:
    self._latest = frame
    self._frames_read += 1
```

**Daemon threads with explicit names:**
```python
# src/helios/mocks/webcam_source.py:54
self._thread = threading.Thread(target=self._loop, daemon=True, name="WebcamSource")
self._thread.start()

# src/helios/perception_loop.py:221
threading.Thread(
    target=_describe_async,
    args=(full.copy(),),
    daemon=True,
).start()
```

**Stop signals:**
```python
# src/helios/mocks/webcam_source.py:34, 60
self._stop_evt = threading.Event()
...
while not self._stop_evt.is_set():
    # loop body
...
self._stop_evt.set()
if self._thread is not None:
    self._thread.join(timeout=2.0)
```

## Strategy Pattern (Protocol-based)

Used throughout for pluggable implementations:

**Contract definition:**
```python
# src/helios/perception/protocol.py
@runtime_checkable
class FrameSource(Protocol):
    def start(self) -> None: ...
    def get_latest_frame(self) -> np.ndarray | None: ...
    def stop(self) -> None: ...
```

**Implementations:**
- `WebcamSource` — `src/helios/mocks/webcam_source.py`
- `MJPEGSource` — `src/helios/perception/mjpeg_source.py`
- Both satisfy `FrameSource` contract

**Router pattern for strategy switching:**
```python
# src/helios/vision/router.py — switches between LLaVA (local) and OpenAI (cloud)
class VisionRouter:
    def __init__(self, use_openai: bool = False, llava_client: LLavaClient | None = None) -> None:
        self.use_openai = use_openai
        self._llava = llava_client or LLavaClient()
        ...

    def describe_scene(self, frame: FrameInput, prompt: str = DEFAULT_SCENE_PROMPT) -> str:
        if not self.use_openai:
            return self._llava.describe_scene(frame, prompt)
        # cloud path with fallback...
```

## Module __all__ Export Lists

When a module exports public APIs, define `__all__` explicitly:

```python
# src/helios/output/haptic.py:139-147
__all__ = [
    "HapticPublisher",
    "WRIST_TOP", "WRIST_BOT", "WRIST_L", "WRIST_R",
    "BELT_L", "BELT_C", "BELT_R",
    "ALL_CHANNELS",
    "DEFAULT_TOPIC_PREFIX",
    "DEFAULT_DUTY",
    "DEFAULT_PULSE_MS",
]

# src/helios/vision/llava_client.py:83
__all__ = ["LLavaClient", "describe_scene", "DEFAULT_SCENE_PROMPT"]
```

## Data Classes

Use `dataclasses.dataclass` for lightweight structured data:

```python
# src/helios/perception_loop.py:39-45
@dataclasses.dataclass
class PerceptionTick:
    ts: float
    bboxes: list[Bbox]
    hands: list[HandLandmarks]
    distance_cm: float
    latency_ms: float
```

Never mix inheritance; keep dataclasses flat and immutable where possible.

## Context Managers

Used for resource cleanup (files, connections, threads):

```python
# src/helios/mocks/webcam_source.py:87-92
def __enter__(self) -> "WebcamSource":
    self.start()
    return self

def __exit__(self, *exc) -> None:
    self.stop()
```

Enables idiomatic usage:
```python
with WebcamSource() as source:
    frame = source.get_latest_frame()
```

## Comments

**When to comment:**
- Explain WHY, not WHAT (code already shows what)
- Reference decisions by code-review ID: `# D-13`, `# D-28`, `# L-14`
- Explain threading rationale and race conditions
- Clarify non-obvious performance tradeoffs

**Examples from codebase:**
```python
# src/helios/perception/yolo_world.py:42-47
# set_classes() rewrites model head class embeddings AND a follow-up
# forward pass must use the matching head shape. If two threads call
# detect() with different prompt lists concurrently they can race
# between set_classes and forward — the head reshape lands mid-flight
# and you get errors like "shape '[1, 84, -1]' is invalid for input
# of size 312000". Lock the whole (set_classes + forward) sequence.
self._lock = threading.RLock()

# src/helios/perception_loop.py:146
# Block briefly waiting for first frame so we don't time the cold-start.
for _ in range(50):
    if source.get_latest_frame() is not None:
        break
    time.sleep(0.05)
```

**No docstring for simple getters/setters:** Let the type hints and name speak.

---

*Convention analysis: 2026-05-09*
