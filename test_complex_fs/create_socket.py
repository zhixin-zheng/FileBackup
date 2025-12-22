import socket
import os
import time

sock_path = "misc/unix_socket.sock"

# 确保旧的 socket 不残留
try:
    os.unlink(sock_path)
except FileNotFoundError:
    pass

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.bind(sock_path)
s.listen(1)

print(f"Unix socket created at: {sock_path}")
try:
    # 保持进程存活，让 socket 文件一直存在
    while True:
        time.sleep(60)
except KeyboardInterrupt:
    pass
finally:
    s.close()
    try:
        os.unlink(sock_path)
    except FileNotFoundError:
        pass