![Image](https://i.imgur.com/4Ku3xak.png)
[![Chat](https://img.shields.io/discord/908871478576033832?label=%20chat%20on%20discord)](https://discord.gg/tZthSbpUcV)

Midless is a free and open-source voxel game made in C.

## Controls

| Input                        | Action                |
|-------------------------------|----------------------|
| W A S D             | Move                           |
| Space               | Jump                           |
| Left Click          | Break block                    |
| Right Click         | Place block                    |
| Mouse wheel         | Block Selection                |
| T                   | Open Chat                      |
| ESC                 | Open menu                      |

## Dependencies

| Dependency    | Version |
|---------------|---------|
| [Raylib](https://github.com/raysan5/raylib/)        | 4.2     |
| [Zpl-c/ENet](https://github.com/zpl-c/enet)    | 2.3.0   |
| [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) | -       |
| [stb_ds](https://github.com/nothings/stb/blob/master/stb_ds.h) | -       |


## Compiling for Windows using MinGW

1. [Download and Build Raylib](https://github.com/raysan5/raylib/wiki/Working-on-Windows)
2. Download/Place the other dependencies inside client/src and server/src.
3. Run mingw32-make inside the Midless folder where the MakeFile is located. 

Make arguments:
```
BUILD_SERVER=TRUE       - Build Midless Server (Doesn't build the client)
DEBUG=TRUE              - Debug build
```


## License

All code in this repository is licensed under the [MIT License](https://github.com/Sirvoid/Midless/blob/main/LICENSE).