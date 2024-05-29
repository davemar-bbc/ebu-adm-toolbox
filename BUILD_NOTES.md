# Build Notes

### Portaudio and OSX
There's an oddity with portaudio and vcpkg on OSX, that seems to make it a static library. To overcome this, set the vcpkg triplet to `arm64-osx-dynamic` by running the initial `cmake` like this:

`cmake -DVCPKG_TARGET_TRIPLET=arm64-osx-dynamic  --preset debug`

The `debug` could be `release` too.

