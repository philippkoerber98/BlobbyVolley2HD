# Audio regression fixture

`tone.ogg` is a generated 0.12-second, 440 Hz stereo sine tone at 48 kHz,
encoded as Ogg Vorbis. It tests actual decoding, streaming loops and mixing;
it is never included in the game archives or portable packages.

To regenerate it with FFmpeg:

```sh
ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=0.12" -ac 2 -c:a libvorbis -q:a 5 -y tone.ogg
```

FFmpeg is not required to run the tests; the encoded fixture is checked in.
