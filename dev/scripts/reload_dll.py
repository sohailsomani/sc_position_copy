import sys
import os
import time
import socket
import psutil

primary_dll, primary_udp_port_prefix, secondary_dll, secondary_udp_port_prefix = sys.argv[
    1:]

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

primary_dll = os.path.abspath(primary_dll)
secondary_dll = os.path.abspath(secondary_dll)

def send_message(message: str, ip: str, port: int) -> None:
    print("SENDING", message, "TO", ip, ":", port)
    print(sock.sendto(message.encode(), (ip, port)))


conns = list(psutil.net_connections(kind='udp'))
conns.sort(key = lambda x: x.laddr) # type: ignore
for conn in conns:
    if not hasattr(conn.laddr, 'port'):
        continue

    port = conn.laddr.port
    sport = str(port)
    dll = None
    if len(sport) > len(primary_udp_port_prefix) and sport.startswith(
            primary_udp_port_prefix):
        dll = primary_dll
    elif len(sport) > len(secondary_udp_port_prefix) and sport.startswith(secondary_udp_port_prefix):
        dll = secondary_dll
    else:
        continue

    p = psutil.Process(pid=conn.pid)
    root = p.cwd()
    assert os.path.isfile(os.path.join(root,"SierraChart_64.exe"))

    assert dll is not None

    dll_basename = os.path.basename(dll)
    dll_path = os.path.join(root, "Data", dll_basename)
    dllwin_path = "Z:" + dll_path.replace("/","\\")

    send_message(f"RELEASE_DLL--{dllwin_path}", conn.laddr.ip, conn.laddr.port)
    try:
        os.unlink(dll_path)
    except:
        pass

    time.sleep(1)
    os.symlink(dll, dll_path)
    time.sleep(1)

    send_message(f"ALLOW_LOAD_DLL--{dllwin_path}", conn.laddr.ip, conn.laddr.port)

    break
