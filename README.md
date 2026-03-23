# Networks Mini Project 1

- 23CS30033 (Mehul Mathur)
- 23CS30065 (Tegan Jain)

GitHub repository link: [Majestic9169/flow-control](https://github.com/Majestic9169/flow-control)

Documentation page (with list and description of functions and structures): [Docs](https://majestic9169.github.io/flow-control)

Attached is also the tarball containing the current version of the project (as directed in the assignment).

## Directory structure

```bash
.
├── app/
│   ├── Makefile
│   ├── user1.c
│   └── user2.c
├── compile_flags.txt        # helper file for clang language server
├── Doxyfile                 # this is the configuration file for the program that generates documentation from code comments
├── lib/
│   ├── kinternal.h
│   ├── ksocket.c
│   ├── ksocket.h
│   └── Makefile
├── logs/
├── payload.png              # our sample file of > 100KB that we are sending and receiving
├── performance_report.py    # python script to generate performance report from daemon logs
├── README.md                # this looks nicer than documentation.txt i think
├── sys/
│   ├── initksocket.c
│   ├── initksocket.h
│   └── Makefile
├── testfile_small.txt       # some alternate test files to transfer
└── testfile.txt             # a larger one > 100KB
```

[Doxygen](https://www.doxygen.nl/index.html) was used to generate documentation from the code comments for this project. The docs are currently hosted on Github pages where they are built with a workflow everytime this project is updated. 

To build them locally you can do

```bash
doxygen Doxyfile
open docs/html/index.html # open in your local browser (recommended)
cd docs/latex && make     # to genertate a pdf version (will need many latex packages)
```

## Performance Table

A script [performance_report.py](./performance_report.py) is provided and our logs for the run with various probabilities are given in [./logs](./logs/). Run the script from this directory to get the P table for the various logs

```bash
❯ python performance_report.py
Prob     | Unique Sent  | Retrans  | Drops    | Avg Sends/Chunk
---------------------------------------------------------------
0.05     | 350          | 82       | 35       | 1.23
0.10     | 347          | 154      | 76       | 1.44
0.15     | 347          | 235      | 139      | 1.68
0.20     | 350          | 299      | 195      | 1.85
0.25     | 350          | 323      | 249      | 1.92
0.30     | 350          | 436      | 344      | 2.25
0.35     | 350          | 608      | 455      | 2.74
0.40     | 350          | 685      | 518      | 2.96
0.45     | 350          | 740      | 662      | 3.11
0.50     | 350          | 920      | 762      | 3.63
```

- (347 segments for `./testfile.txt`)
- (350 segments for `./payload.png`)

## To run the project:

1. Recommended: 
	  - Clone the github repo by: 

	  ```bash
    git clone https://github.com/Majestic9169/flow-control.git
	  ```

    - Or you can extract the tarball

    ```bash
    tar xzf TEGAN_23CS30065_MEHUL_23CS30033.tar.gz
    ```

    ```bash
    cd flow-control/
    ```

1. Build the library

    ```bash
    cd lib/
    make # this builds the ksocket.a library
    ```

1. Run the init daemon

    ```bash
    cd sys/
    make run
    ```	

4. Run a listener and sender pair

    ```bash
    cd app/
    make # builds both
    ```

    ```bash
    # USAGE
    ❯ ./receiver
    usage: ./receiver <src_port> <dst_ip> <dst_port> <outfile>
    ❯ ./sender
    usage: ./sender <src_port> <dst_ip> <dst_port> <file>
    ```

### Single Socket Pair

```bash
cd app/
make
./receiver 9090 127.0.0.1 8080 received.bin
```

```bash
./sender 8080 127.0.0.1 9090 ../payload.png
```

Run in different terminals for clearer logs. If you want to run in the same consider 

```bash
cd app/
make run
```

### Multiple Socket Pairs

Similar to above

```bash
cd app/
make
# pair 1
./receiver 8080 127.0.0.1 8081 received_1.bin
./sender 8081 127.0.0.1 8080 ../payload.png
# pair 2
./receiver 9090 127.0.0.1 9091 received_2.bin
./sender 9091 127.0.0.1 9090 ../payload.png
```

> [!CAUTION] 
> make sure their receiving files are not the same otherwise you will lose data

If you want to run them in the same terminal (chaotic)

```bash
cd app/
make manyrun
```

> [!NOTE] 
> Listeners will wait for some data for 100 * TIMEOUTVALUE seconds before auto quitting. You can also prematurely CTRL+C to stop the listeners
