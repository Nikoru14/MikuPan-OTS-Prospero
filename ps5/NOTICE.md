# PS5 title support files

The files in `native/` come from
[ps5-native-app-boilerplate](https://github.com/drakmor/ps5-native-app-boilerplate)
at commit `470695e0c557f99ff2df0e36e4df713c5e636526`, by BlackBearReloaded,
licensed GPL-3.0-or-later (see each file's header). They are combined with
MikuPan, which is licensed AGPL-3.0, as both licenses allow.

Modified: `native/ps5-pie.ld` additionally defines the `__eh_frame_start`,
`__eh_frame_end`, `__eh_frame_hdr_start` and `__eh_frame_hdr_end` markers
that the ps5-payload-sdk's libunwind needs for C++ exceptions, and places
`.got` and `.got.plt` before `.data.rel.ro` at the start of the RELRO segment,
without which the console refuses to start the title (CE-107750-0).
`native/app-symbols.map` makes every symbol local, not only the allocation
operators, because the title converter accepts no exports. `native/app_crt.cpp`
is unchanged.

`native/cxa_thread_atexit.cpp` and `native/payload_shims.cpp` are MikuPan's
own, licensed like the rest of MikuPan.

- `native/app_crt.cpp`: title entry point (`_start`). Initializes the runtime,
  runs static constructors, calls `main` and exits.
- `native/ps5-pie.ld`: linker layout that keeps code, read-only data, RELRO and
  writable data in the separate ranges the console's loader expects.
- `native/app-symbols.map`: keeps the C++ allocation operators internal to the
  executable.

`sce_sys/icon0.png` is the MikuPan logo scaled to the 512x512 icon size.
`sce_sys/param.json.in` follows the layout of the boilerplate's and the
OpenLF2 PS5 port's `param.json`.

The build also needs two artifacts from that boilerplate, built on your
machine: its `ps5-native-tool` converter and its clean-room `libc.prx`
runtime module. See "PlayStation 5" in the README.
