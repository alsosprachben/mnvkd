#!/usr/bin/env python3

def main():
    import sys

    args = dict(enumerate(sys.argv))
    count = int(args.get(1, "0"))
    for i in range(count - 1):
        sys.stdout.write("GET / HTTP/1.1\r\nHost: test\r\n\r\n")

    sys.stdout.write("GET / HTTP/1.1\r\nHost: test\r\nConnection: close\r\n\r\n")
    sys.stdout.flush()


if __name__ == "__main__":
    main()