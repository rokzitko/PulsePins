## Extension cookbook

This page is a practical guide for extending PulsePins. It is intentionally procedural: the goal is to help contributors make focused additions without first reverse-engineering the entire repository.

For build and onboarding background, also see:

* [Build and deployment](build.md)
* [Development](development.md)
* [Hacking on PulsePins](hacking.md)
* [C++ application programming interface](cpp.md)
* [Python bindings](python.md)

### General advice

Before adding new functionality:

* keep the change narrow
* update tests in the same change when practical
* update docs for user-visible tools and APIs
* prefer existing shared helpers over new one-off parsing or formatting logic

The project has increasingly moved toward centralized semantics in the core types. When extending sequence behavior or tool behavior, it is usually better to add one shared helper in the right place than to duplicate logic in several command handlers.

## Recipe 1: add a new `pp...` CLI tool

The `pptool` executable family is dispatched by program name. Most commands are symlink-style entry points into the same binary.

### Files to touch

* [`c++/pptool_commands.hh`]({{ source_file("c++/pptool_commands.hh") }}) - declare the handler
* [`c++/pptool.cc`]({{ source_file("c++/pptool.cc") }}) - register the handler in the dispatch table
* one implementation file under [`c++/`]({{ source_file("c++/") }}):
    * often [`pptool.cc`]({{ source_file("c++/pptool.cc") }})
    * or [`pptool_streaming.cc`]({{ source_file("c++/pptool_streaming.cc") }})
    * or [`pptool_measurement.cc`]({{ source_file("c++/pptool_measurement.cc") }})
    * or a new dedicated `.cc`/`.hh` pair if the command is large enough
* [`c++/Makefile`]({{ source_file("c++/Makefile") }}) if the command needs a new symlink name on deployment
* `docs/docs/<tool>.md` for the command reference page
* optionally [`recipes/<tool>`]({{ source_file("recipes/") }}) for reusable command examples

### Minimal implementation sequence

1. Add a function declaration in [`c++/pptool_commands.hh`]({{ source_file("c++/pptool_commands.hh") }}):

```cpp
int ppmytool(FPGA &fpga, const InputParser &input, const Verbosity &v);
```

2. Implement the handler in the most appropriate existing source file.

3. Register the program name in the dispatch table in [`c++/pptool.cc`]({{ source_file("c++/pptool.cc") }}).

4. If the command should be callable as its own shell name, add it to the `SYMLINKS` list in [`c++/Makefile`]({{ source_file("c++/Makefile") }}).

5. Add or update tests.

6. Add the command doc page and link it from [`docs/mkdocs.yml`]({{ source_file("docs/mkdocs.yml") }}).

### Design hints

* Prefer reusing shared startup/runtime code through `HostRuntime` and the existing wrappers.
* For streaming operations, look at the helpers in [`ppworkflow.hh`]({{ source_file("c++/ppworkflow.hh") }}) before inventing a new send/trigger/readback path.
* For parsing command options, follow the existing `InputParser` pattern and keep validation close to the command handler.

## Recipe 2: add or change a sequence construct

If the new feature changes the meaning or representation of sequence elements, the core files are:

* [`c++/elements.hh`]({{ source_file("c++/elements.hh") }})
* [`c++/sequence.hh`]({{ source_file("c++/sequence.hh") }})
* [`c++/unit_tests.cc`]({{ source_file("c++/unit_tests.cc") }})

### What lives where

* [`elements.hh`]({{ source_file("c++/elements.hh") }}) owns:
    * the `el` representation
    * control classification
    * regular token mapping
    * raw reconstruction helpers
    * per-element text serialization
* [`sequence.hh`]({{ source_file("c++/sequence.hh") }}) owns:
    * stream/file conversion around `Sequence`
    * parsing loops and sequence-level operations

### Recommended order

1. Add tests first in [`c++/unit_tests.cc`]({{ source_file("c++/unit_tests.cc") }}).
2. Extend [`elements.hh`]({{ source_file("c++/elements.hh") }}) shared helpers if the feature affects element semantics.
3. Extend [`sequence.hh`]({{ source_file("c++/sequence.hh") }}) only where the sequence container or parser/writer glue must change.
4. Update Python bindings if the feature is surfaced there.
5. Update C++/Python docs if the API or grammar changed.

### Important rule

Do not add parallel token/control/behavior mappings in several places if the feature is really an `el`-level concern. The preferred pattern is to centralize the semantics in [`elements.hh`]({{ source_file("c++/elements.hh") }}) and let text/binary I/O call into those helpers.

## Recipe 3: add a Python binding

The Python bindings live in:

* [`python/pp.cc`]({{ source_file("python/pp.cc") }}) - nanobind module entry point for `pp`
* [`python/pp_bind_*.cc`]({{ source_file("python/") }}) - the split high-level object bindings for `pp`
* [`python/pp_impl.cc`]({{ source_file("python/pp_impl.cc") }}) - constants and low-level exported values
* [`python/test.py`]({{ source_file("python/test.py") }}) - Python-side tests

### When to touch which file

* add or update a high-level class or method wrapper: the relevant [`python/pp_bind_*.cc`]({{ source_file("python/") }}) file
* export a new constant or symbolic string: [`python/pp_impl.cc`]({{ source_file("python/pp_impl.cc") }})
* verify the binding surface: [`python/test.py`]({{ source_file("python/test.py") }})

### Typical sequence

1. Add the binding in the relevant [`python/pp_bind_*.cc`]({{ source_file("python/") }}) file.
2. If the Python tests need a constant from C++, export it in [`python/pp_impl.cc`]({{ source_file("python/pp_impl.cc") }}).
3. Add host-safe tests to [`python/test.py`]({{ source_file("python/test.py") }}).
4. If the test requires a live board or `/dev/mem`, mark it with `@pytest.mark.hardware`.

### CI note

Host CI runs:

```bash
make -C python USE_PREGENERATED=1 build test-host
```

So Python tests that do not need real hardware should stay unmarked and should pass in a normal Linux build environment.

## Recipe 4: add a `ppwebgui` feature

The `ppwebgui` stack is deliberately split into layers.

Main files:

* [`c++/ppwebgui_types.hh`]({{ source_file("c++/ppwebgui_types.hh") }}) - request/response/value types
* [`c++/ppwebgui_service_api.hh`]({{ source_file("c++/ppwebgui_service_api.hh") }}) - service interface exposed to HTTP/UI code
* [`c++/ppwebgui_service.hh`]({{ source_file("c++/ppwebgui_service.hh") }}) and [`ppwebgui_service.cc`]({{ source_file("c++/ppwebgui_service.cc") }}) - hardware-owning implementation
* [`c++/ppwebgui_http.hh`]({{ source_file("c++/ppwebgui_http.hh") }}) and [`ppwebgui_http.cc`]({{ source_file("c++/ppwebgui_http.cc") }}) - route registration, request parsing, HTTP errors
* [`c++/ppwebgui_json.cc`]({{ source_file("c++/ppwebgui_json.cc") }}) - JSON rendering
* [`c++/ppwebgui_assets.cc`]({{ source_file("c++/ppwebgui_assets.cc") }}) - embedded frontend assets
* [`c++/unit_tests.cc`]({{ source_file("c++/unit_tests.cc") }}) - host-side HTTP and request-validation tests

### Recommended order

1. Add or extend the value type in [`ppwebgui_types.hh`]({{ source_file("c++/ppwebgui_types.hh") }}).
2. Extend the service interface in [`ppwebgui_service_api.hh`]({{ source_file("c++/ppwebgui_service_api.hh") }}) if the HTTP layer needs a new operation.
3. Implement the hardware-owning logic in [`ppwebgui_service.cc`]({{ source_file("c++/ppwebgui_service.cc") }}).
4. Add the HTTP parsing/route logic in [`ppwebgui_http.cc`]({{ source_file("c++/ppwebgui_http.cc") }}).
5. Extend JSON rendering if needed.
6. Add unit tests in [`c++/unit_tests.cc`]({{ source_file("c++/unit_tests.cc") }}).

### Important ownership rule

Keep the hardware-owning object graph anchored in `WebGuiController`. Do not copy or re-own that graph from higher layers. New GUI/HTTP features should normally pass values through the service interface, not bypass it.

## Recipe 5: add a new tool helper under [`tools/`]({{ source_file("tools/") }})

Use [`tools/`]({{ source_file("tools/") }}) when the code is useful but too device-specific or too experimental for the main CLI.

Good candidates:

* sequence generators for a specific peripheral
* conversion utilities
* specialized bring-up helpers

Recommended structure:

* keep a local `README`
* keep the build minimal (`Makefile` or one small CMake target)
* include exact qout wiring or hardware assumptions
* if the helper emits PulsePins sequences, document the expected playback command

The recent [`tools/spi_payload/`]({{ source_file("tools/spi_payload/") }}) work is a good example of this pattern.

## Recipe 6: add tests in the right place

### C++ core and host-side logic

Use:

* [`c++/unit_tests.cc`]({{ source_file("c++/unit_tests.cc") }})

Best for:

* sequence semantics
* parsing/formatting
* HTTP validation
* helper wrappers that are safe on the host

### Python binding surface

Use:

* [`python/test.py`]({{ source_file("python/test.py") }})

Best for:

* constructor coverage
* binding method coverage
* round-trip tests through Python-facing APIs

Mark board-backed tests with:

```python
@pytest.mark.hardware
```

### HDL and RTL behavior

Use:

* [`ip/`]({{ source_file("ip/") }}) test benches

Best for:

* hardware logic correctness
* CDC/reset-sensitive functionality
* pre-software interface validation

## Recipe 7: document a new feature properly

For user-visible functionality, update docs in the same change.

Typical places:

* command reference page in `docs/docs/`
* [`docs/mkdocs.yml`]({{ source_file("docs/mkdocs.yml") }}) navigation
* [`recipes/`]({{ source_file("recipes/") }}) if the feature benefits from reusable command examples
* [`README.md`]({{ source_file("README.md") }}) if it materially changes the project’s discoverability

If the feature is mainly for contributors, update:

* [`HACKING.md`]({{ source_file("HACKING.md") }})
* [`CONTRIBUTING.md`]({{ source_file("CONTRIBUTING.md") }})
* [`c++/README.md`]({{ source_file("c++/README.md") }}) or [`python/README*`]({{ source_file("python/") }}) if they are the best maintainer-facing entry point

## Quick checklists

### New `pp...` command

* handler declared in [`pptool_commands.hh`]({{ source_file("c++/pptool_commands.hh") }})
* handler registered in [`pptool.cc`]({{ source_file("c++/pptool.cc") }})
* symlink added to [`c++/Makefile`]({{ source_file("c++/Makefile") }}) if needed
* tests added
* docs page added

### New Python API surface

* binding added in the relevant [`python/pp_bind_*.cc`]({{ source_file("python/") }}) file
* constants added in [`python/pp_impl.cc`]({{ source_file("python/pp_impl.cc") }}) if needed
* tests added in [`python/test.py`]({{ source_file("python/test.py") }})
* hardware-only tests marked
* Python docs updated

### New `ppwebgui` operation

* type added in [`ppwebgui_types.hh`]({{ source_file("c++/ppwebgui_types.hh") }}) if needed
* service API updated
* controller implementation updated
* route/parser added in [`ppwebgui_http.cc`]({{ source_file("c++/ppwebgui_http.cc") }})
* unit tests added in [`c++/unit_tests.cc`]({{ source_file("c++/unit_tests.cc") }})

## Final advice

When in doubt:

* start from an existing nearby feature
* follow the same layering pattern
* add tests before broad refactors
* prefer one shared semantic helper over repeated switch statements in multiple files

That approach has worked well for the recent [`elements.hh`]({{ source_file("c++/elements.hh") }}), Python binding, and `ppwebgui` cleanup work.
