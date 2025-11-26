# udp_tcp_forwarder

## Application
Build:
```
mkdir build && cd build
cmake ../src
make
```

Usage:
``` ./udp_tcp_forwarder --config <path_to_json_config> ```

Can be safely stopped with SIGINT or SIGTERM.

Scripts in the "useful scripts" folder might also come in handy when you want to simulate UDP client or TCP server behaviour:
```
./udp_generator.sh <ip> <port>
./tcp_echo.sh <port>
```

## Brief description of achitecture
All source files are contained in `src` directory.

- `core` mostly contains common definitions/functions;
- `impl` contains implementations of interfaces, defined in `core`, and a few helper components like `configuration.h`;
- `config` contains json configuration file(s).

\
`core` and `impl` are subdivided into three main categories:
- `endpoints` - TCP and UDP endpoints;
- `scheduling` - Requests scheduling;
- `aux` - Anything else.

Every subdirectory (corresponding to a category) in `impl` also contains `include` and `source` directories in it, since almost every entity in `impl` has associated `.cpp` files.