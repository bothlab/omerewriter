# OMERewriter

A tool to edit common fields in OME-TIFF files and to rewrite the output of [ScanImage](https://docs.scanimage.org/index.html)
TIFF files into metadata-rich [OME-TIFF](https://ome-model.readthedocs.io/en/stable/ome-tiff/index.html) files.

![OMERewriter screenshot](data/screenshot-v0.1.0.avif)

## Features

* Open and inspect OME-TIFF files, including multi-channel Z-stack images
* Edit metadata - physical voxel sizes, numerical aperture, lens immersion, embedding medium, etc.
* Edit channel parameters, like microscope type, excitation/emission wavelengths, pinhole size, etc.
* Deinterleave raw TIFF files to save them as standards-compliant OME-TIFF files
* Save and reload parameter sets for quick re-use across acquisitions

## Building

### Linux

Dependencies: Qt 6.5+, CMake 3.19+, [ome-files-cpp](https://gitlab.com/codelibre/ome/ome-files-cpp), a C++23 compiler.
The OME libraries are fetched automatically by CMake if they are not found.

```bash
cmake -GNinja -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows

We strongly recommend [MSYS2](https://www.msys2.org/) (UCRT64) for Windows builds.
All dependencies are installed via `pacman`/`pacboy` or built from source by CMake.
See the CI workflow in `.github/workflows/build-test.yml` for the exact steps.

## License

This project is licensed under the *GNU Lesser General Public License v3.0 or later* (LGPL-3.0-or-later).
See `LICENSE.LGPL-3.0` for the full license text.
