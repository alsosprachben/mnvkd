#!/usr/bin/env python3

def main():
    import sys

    args = dict(enumerate(sys.argv))
    count = int(args.get(1, "0"))
    mode = args.get(2, "POST")
    req_open = {
        "POST": "POST / HTTP/1.1\r\nHost: test\r\nContent-Length: 10\r\n\r\n12345678\r\n",
        "POST-chunked": "POST / HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\n\r\n8\r\n12345678\r\n0\r\n\r\n",
        "GET": "GET / HTTP/1.1\r\nHost: test\r\n\r\n",
    }[mode]
    req_closed = {
        "POST": "POST / HTTP/1.1\r\nHost: test\r\nConnection: close\r\nContent-Length: 10\r\n\r\n12345678\r\n",
        "POST-chunked": "POST / HTTP/1.1\r\nHost: test\r\nConnection: close\r\nTransfer-Encoding: chunked\r\n\r\n8\r\n12345678\r\n0\r\n\r\n",
        "GET": "GET / HTTP/1.1\r\nHost: test\r\nConnection: close\r\n\r\n",
    }[mode]

    for i in range(count - 1):
        sys.stdout.write(req_open)

    sys.stdout.write(req_closed)
    sys.stdout.flush()


if __name__ == "__main__":
    main()
