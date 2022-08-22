# fuzzer_libwebp for webp-pixbuf-loader

This folder contains a fuzzer for webp animation (could be used for static webp images too, with different examples in the corpus folder, and minor changes in the code) in webp-pixbuf-loader.
This is based on the example fuzzer for libpng contained, with other example fuzzers, in libAFL ( https://github.com/AFLplusplus/LibAFL ).
This is mostly in Rust, with some C++ and C.
This is regarded as a work-in-progress.
It could be sped up considerably.

The intent of the current code is to detect crashes caused by the webp-pixbuf-loader code.
This contains a copy of the webp-pixbuf-loader code as of Aug 08, 2022, with fixes in io-webp-anim.c to avoid the crashes found. ( https://github.com/aruiz/webp-pixbuf-loader )

The fuzz test is of the complete stack: libwebp -> webp-pixbuf-loader -> gtk3. I thought this was the most thorough way of testing webp-pixbuf-loader. It uses two images (in corpus/ )  to test on, t5.webp and animated-webp-supported.webp . For some reason I haven't tracked down, the later image is deemed "uninteresting" by libAFL and not used.

So the image t5.webp is used, and for each test it is mutated ("corrupted") and fed into t4_gtk3_fuzz_exe with libAFL shepherding the operation. The code in harness.cc, which launches t4_gtk3_fuzz_exe, is set so that crashes of t4_gtk3_fuzz_exe produce error codes which are fed to libAFL and recorded. Other types of errors are changed to 0 (noErr) so that libAFL does not record them. The intent is that the mutated image will kick up a lot of error codes in the software, and that is ok, the webp-pixbuf-loader code can handle that, but crashes are what need to be avoided. So those are what is recorded.

In the tests run, two mutation schemes were used: 
StdScheduledMutator::new(havoc_mutations())
StdScheduledMutator::new(havoc_mutations().merge(tokens_mutations()))
( see src/lib.rs )
 The later was copied from libpng testing.

## Build for fuzzer_libwebp

This assumes the install instructions for libAFL and webp-pixbuf-loader have been followed.

```bash
cd fuzzer_libwebp/
cd webp-pixbuf-loader/
```

Build this sub-project. This is webp-pixbuf-loader as of Aug 08, 2022, with the following changes:
-Fixes in io-webp-anim.c
-Addition of file tests/t4-gtk3-fuzz.c
-Small additions to meson.build and tests/meson.build to compile the above file into an executable: t4_gtk3_fuzz_exe in the builddir/tests/ folder.
The sub-project build will be something like:

```
meson builddir
ninja -C builddir
```

Then move back to the main project and build it.

```
cd ..
cargo build --release
cargo make fuzzer
```

This produces an executable fuzzer_libwebp , 


## Running tests

Images to be used for a test base are in the corpus folder. Currently there are 2 images there, but these could be changed. These images are mutated by libAFL and fed to the code under test. The code in harness.cc creates a TEMP_FILE_<pid> and launches t4_gtk3_fuzz_exe TEMP_FILE_<pid> and checks for any signals being thrown. If no "interesting events" (see below) happen, the TEMP_FILE_<pid> is removed.

Open 2 terminal windows with the working directory being fuzzer-webp/ in both.
In both windows make sure core dumps can be created, for instance with:

```
ulimit -c unlimited
```

Make sure any core dumps created can be seen. For instance in Ubuntu 22.04 doing:

```
sudo apt install systemd-coredump
coredumpctl list
```

Then in one window (which becomes a server), run:

```
./fuzzer_libwebp
```

In the other window (which becomes a client), run:

```
./fuzzer_libwebp
```

Technically more client windows could be opened.


libAFL is set to record "interesting" events. i.e. the code under test crashing, returning a non-zero return code, timing out, etc. As the current fuzzer_libwebp code is constructed, it limits this to signals, almost entirely. (But a few certain other events are recorded.)

When an interesting event happens, libAFL drops a copy of the data file that caused it in the crashes folder. Also a TEMP_FILE_<pid> will be present with the same data file. (These can be run through the debugger with gdb --args t4_gtk3_fuzz_exe TEMP_FILE_<pid> .)
Core dumps can be checked:

```
coredumpctl list
```

If there are any core dumps, these can be examined with

```
coredumpctl list
coredumpctl -o coredumpfile dump <pid>  # <pid> is the PID from list.
gdb  webp-pixbuf-loader/builddir/tests/t4_gtk3_fuzz_exe coredumpfile
```



# The original README.md for Libfuzzer for libpng follows. It has useful information.

# Libfuzzer for libpng

This folder contains an example fuzzer for libpng, using LLMP for fast multi-process fuzzing and crash detection.

In contrast to other fuzzer examples, this setup uses `fuzz_loop_for`, to occasionally respawn the fuzzer executor.
While this costs performance, it can be useful for targets with memory leaks or other instabilities.
If your target is really instable, however, consider exchanging the `InProcessExecutor` for a `ForkserverExecutor` instead.

It also uses the `introspection` feature, printing fuzzer stats during execution.

To show off crash detection, we added a `ud2` instruction to the harness, edit harness.cc if you want a non-crashing example.
It has been tested on Linux.

## Build

To build this example, run

```bash
cargo build --release
```

This will build the library with the fuzzer (src/lib.rs) with the libfuzzer compatibility layer and the SanitizerCoverage runtime functions for coverage feedback.
In addition, it will also build two C and C++ compiler wrappers (bin/libafl_c(libafl_c/xx).rs) that you must use to compile the target.

The compiler wrappers, `libafl_cc` and libafl_cxx`, will end up in `./target/release/` (or `./target/debug`, in case you did not build with the `--release` flag).

Then download libpng, and unpack the archive:
```bash
wget https://deac-fra.dl.sourceforge.net/project/libpng/libpng16/1.6.37/libpng-1.6.37.tar.xz
tar -xvf libpng-1.6.37.tar.xz
```

Now compile libpng, using the libafl_cc compiler wrapper:

```bash
cd libpng-1.6.37
./configure
make CC="$(pwd)/../target/release/libafl_cc" CXX="$(pwd)/../target/release/libafl_cxx" -j `nproc`
```

You can find the static lib at `libpng-1.6.37/.libs/libpng16.a`.

Now, we have to build the libfuzzer harness and link all together to create our fuzzer binary.

```
cd ..
./target/release/libafl_cxx ./harness.cc libpng-1.6.37/.libs/libpng16.a -I libpng-1.6.37/ -o fuzzer_libpng -lz -lm
```

Afterward, the fuzzer will be ready to run.
Note that, unless you use the `launcher`, you will have to run the binary multiple times to actually start the fuzz process, see `Run` in the following.
This allows you to run multiple different builds of the same fuzzer alongside, for example, with and without ASAN (`-fsanitize=address`) or with different mutators.

## Run

The first time you run the binary, the broker will open a tcp port (currently on port `1337`), waiting for fuzzer clients to connect. This port is local and only used for the initial handshake. All further communication happens via shared map, to be independent of the kernel. Currently, you must run the clients from the libfuzzer_libpng directory for them to be able to access the PNG corpus.

```
./fuzzer_libpng

[libafl/src/bolts/llmp.rs:407] "We're the broker" = "We\'re the broker"
Doing broker things. Run this tool again to start fuzzing in a client.
```

And after running the above again in a separate terminal:

```
[libafl/src/bolts/llmp.rs:1464] "New connection" = "New connection"
[libafl/src/bolts/llmp.rs:1464] addr = 127.0.0.1:33500
[libafl/src/bolts/llmp.rs:1464] stream.peer_addr().unwrap() = 127.0.0.1:33500
[LOG Debug]: Loaded 4 initial testcases.
[New Testcase #2] clients: 3, corpus: 6, objectives: 0, executions: 5, exec/sec: 0
< fuzzing stats >
```

As this example uses in-process fuzzing, we added a Restarting Event Manager (`setup_restarting_mgr`).
This means each client will start itself again to listen for crashes and timeouts.
By restarting the actual fuzzer, it can recover from these exit conditions.

In any real-world scenario, you should use `taskset` to pin each client to an empty CPU core, the lib does not pick an empty core automatically (yet).

