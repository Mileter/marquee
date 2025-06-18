# Marquee Display System

This marquee system uses the Win32 API to edit and display marquee files (`.mly`).

Suite subprograms:
- Editor
- Renderer
- Validate

## Building

```
make all
```

The makefile assumes that you are using the MinGW-w64 toolchain. If you are using MSVC or something else, feel free to patch the makefile using snot and glue to make it work.

# Documentation

In the `mly/` directory, there is a file called `standard.mly`. It documents all the features that can be used.