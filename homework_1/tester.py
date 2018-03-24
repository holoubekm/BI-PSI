import socket
import time
import os

data = "matt is ok"

HOST, PORT = "127.0.0.1", 3004
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Connect to server and send data
sock.connect((HOST, PORT))
print(str(sock.recv(1024), "utf-8"))
sock.send(bytes("Robot Karel\r\n", "utf-8"))
print(str(sock.recv(1024), "utf-8"))
sock.send(bytes("1045\r\n", "utf-8"))
print(str(sock.recv(1024), "utf-8"))
sock.send(bytes("I", "utf-8"))
time.sleep(1)
sock.send(bytes("NFO", "utf-8"))
time.sleep(1)
sock.send(bytes(" ", "utf-8"))
time.sleep(1)
sock.send(bytes("sadflasdfasdf\r\n", "utf-8"))
print(str(sock.recv(1024), "utf-8"))
# time.sleep(1000)

crc = 0;
size = os.path.getsize("src.png")
print(size)

data = []
with open("src.png", "rb") as f:
    byte = f.read(size)
       

for b in byte:
	crc += b

print(crc)

sock.send(bytes("FOTO " + str(size) + " ", "utf-8"))
sock.send(byte)
sock.send(b'\x00\x24')
sock.send(b'\x67\xC4')
print(str(sock.recv(1024), "utf-8"))

sock.send(bytes("I", "utf-8"))
time.sleep(1)
sock.send(bytes("NFO", "utf-8"))
time.sleep(1)
sock.send(bytes(" ", "utf-8"))
time.sleep(1)
sock.send(bytes("sadflasdfasdf\r\n", "utf-8"))
print(str(sock.recv(1024), "utf-8"))

sock.send(bytes("FOTO " + str(size) + " ", "utf-8"))
sock.send(byte)
sock.send(b'\x00\x24')
sock.send(b'\x67\xC4')
print(str(sock.recv(1024), "utf-8"))


sock.send(bytes("FOTO " + str(size) + " ", "utf-8"))
sock.send(byte)
sock.send(b'\x00\x24')
sock.send(b'\x67\xC5')
print(str(sock.recv(1024), "utf-8"))

sock.send(bytes("I", "utf-8"))
time.sleep(1)
sock.send(bytes("N", "utf-8"))
time.sleep(1)
sock.send(bytes("FO ", "utf-8"))
time.sleep(1)
sock.send(bytes(" dulezita informace\r\n", "utf-8"))
print(str(sock.recv(1024), "utf-8"))
# sock.close()
