## Documentation

This is a header-only library, as such most of its functional documentation is contained within the "header section" of the
source code in the form of comments. It is highly recommended that you read said documentation before using this library.

## Features

The atomix library is a wait-free mixer making heavy use of stdatomic.h and SSE intrinsics, its features include:

- No hard dependencies besides the standard library and the optional standard header stdatomic.h
- Configurable memory allocation function, allowing for compatibility with custom memory managers
- Pure mixer, allowing usage with the backend of your choice and mixing at any desired sampling rate
- High performance, generally fast enough for real-time mixing at high sampling rates even without SSE

## Future

Possible future features include:

- Automatic miniaudio integration, removing the need for a boilerplate callback function
- Mechanism to allow more advanced layering, coordination, and timing of sounds
- Proper 3D audio support, possibly with position interpolation and doppler

## Attribution

You are not required to give attribution when using this library. If you want to give attribution anyway, either link to
this repository, [my website](https://www.slopegames.com/), or credit me as [BareRose](https://github.com/BareRose).
If you want to support me financially, consider giving to my [Patreon](https://www.patreon.com/slopegames).

## License

Licensed under CC0 aka the most lawyer-friendly way of spelling "public domain".