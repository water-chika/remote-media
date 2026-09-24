# remote-media

A small, dependency-free C++17 library containing transport-neutral data types
for remote display and input implementations.

## Features

- Validated packed pixel formats (`RGBA8`, `BGRA8`, `RGB8`, `BGR8`, `Gray8`).
- Checked immutable and mutable frame views, rectangular subviews, and owned
  frame storage.
- A thread-safe latest-frame mailbox with monotonically increasing sequence
  numbers and a provider interface.
- Explicit keyboard, pointer, button, and scroll DTOs. `KeyCode` values use the
  USB HID Keyboard/Keypad usage-ID space so producers and consumers share a
  stable, platform-neutral wire vocabulary.
- No third-party dependencies.

All dimensions are in pixels. Stride and buffer sizes are in bytes. Frames must
have non-zero dimensions, sufficient stride and storage, and sizes that do not
overflow `std::size_t`. The mailbox intentionally retains only the newest frame;
slow consumers can detect skipped frames from sequence numbers.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Consumers can link the `remote_media` target directly with `add_subdirectory`,
or use the exported `remote_media::remote_media` target after installation.

## Licensing

This is a private repository with no license grant. No `LICENSE` file is
provided intentionally.
