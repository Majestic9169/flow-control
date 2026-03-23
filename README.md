# Networks Mini Project 1

- 23CS30033 (Mehul Mathur)
- 23CS30065 (Tegan Jain)

GitHub repository link: [Majestic9169/flow-control](https://github.com/Majestic9169/flow-control)

Documentation page (with list and description of functions and structures): [Docs](https://majestic9169.github.io/flow-control)

Attached is also the tarball containing the current version of the project (as directed in the assignment).

To run the project:

1. Recommended: 
	  - Clone the github repo by: 

	  ```bash
    git clone https://github.com/Majestic9169/flow-control.git
	  ```

    - Or you can extract the tarball

    ```bash
    tar xzf TEGAN_23CS30065_MEHUL_23CS30033.tar.gz
    ```

1. Build the library

    ```bash
    cd flow-control/
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
