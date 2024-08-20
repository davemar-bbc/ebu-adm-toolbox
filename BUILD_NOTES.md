# Build Notes

### Portaudio and OSX
There's an oddity with portaudio and vcpkg on OSX, that seems to make it a static library. To overcome this, set the vcpkg triplet to `arm64-osx-dynamic` by running the initial `cmake` like this:

`cmake -DVCPKG_TARGET_TRIPLET=arm64-osx-dynamic  --preset debug`

The `debug` could be `release` too.

### Building the docs

Due to some weirdness in sphinx not find the extension modules (e.g. breathe), there's a way of build the documentation using a python virtual environment. Follow the following commands:

```
> python -m venv .venv
> source .venv/bin/activate
> python -m pip install breathe
> python -m pip install exhale
> python -m pip install sphinx_rtd_theme
> make html
> deactivate
```
This should build the html files in the `_build` directory.

