#!/usr/bin/env python3

def main():
    import sys

    args = dict(enumerate(sys.argv))
    count = int(args.get(1, "0"))
    chunked = bool(args.get(2, "0"))
    for i in range(count - 1):
        if chunked:
            sys.stdout.write("POST / HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\n\r\n8\r\n12345678\r\n0\r\n\r\n")
        else:
            sys.stdout.write("POST / HTTP/1.1\r\nHost: test\r\nContent-Length: 10\r\n\r\n12345678\r\n")

    if chunked:
        sys.stdout.write("POST / HTTP/1.1\r\nHost: test\r\nConnection: close\r\nTransfer-Encoding: chunked\r\n\r\n8\r\n12345678\r\n0\r\n\r\n")
    else:
        sys.stdout.write("POST / HTTP/1.1\r\nHost: test\r\nConnection: close\r\nContent-Length: 10\r\n\r\n12345678\r\n")
    sys.stdout.flush()


if __name__ == "__main__":
    main()
