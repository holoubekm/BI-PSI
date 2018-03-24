import socket
import os
import socket

from time import sleep


UDP_IP = "127.0.0.1"
UDP_PORT = 5005

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

while True:
	print("receiving...")
	data, addr = sock.recvfrom(1024)
	print("received message:", data)
	sock.sendto(bytes([0, 0, 0, 0, 0, 0, 0, 0, 4, 0]), (UDP_IP, 5003))
	sock.sendto(bytes([0, 0, 0, 0, 0, 0, 0, 0, 4, 0]), (UDP_IP, 5003))
	sock.sendto(bytes([0, 0, 0, 0, 0, 0, 0, 0, 4, 0]), (UDP_IP, 5003))
	sock.sendto(bytes([0, 0, 0, 0, 0, 0, 0, 0, 4, 0]), (UDP_IP, 5003))
	sock.sendto(bytes([0, 0, 0, 35, 0, 0, 0, 0, 4, 1]), (UDP_IP, 5003))

	while True:
		sock.sendto(bytes([0, 0, 0, 35, 255, 0, 0, 0, 0, 1, 2, 3, 4]), (UDP_IP, 5003))
		data, addr = sock.recvfrom(1024)
		print("received message:", data)
		sleep(1)
		sock.sendto(bytes([0, 0, 0, 35, 0, 16, 0, 0, 0, 17, 18, 19, 20]), (UDP_IP, 5003))
		data, addr = sock.recvfrom(1024)
		print("received message:", data)
		sleep(1)
		sock.sendto(bytes([0, 0, 0, 35, 0, 4, 0, 0, 0, 5, 6, 7, 8]), (UDP_IP, 5003))
		data, addr = sock.recvfrom(1024)
		print("received message:", data)
		sleep(1)
		sock.sendto(bytes([0, 0, 0, 35, 0, 8, 0, 0, 0, 9, 10, 11, 12]), (UDP_IP, 5003))
		data, addr = sock.recvfrom(1024)
		print("received message:", data)
		sleep(1)
		sock.sendto(bytes([0, 0, 0, 35, 0, 12, 0, 0, 0, 13, 14, 15, 16]), (UDP_IP, 5003))
		data, addr = sock.recvfrom(1024)
		print("received message:", data)
		sleep(1)

		sock.sendto(bytes([0, 0, 0, 0, 0, 1, 0, 0, 0]), (UDP_IP, 5003))
		data, addr = sock.recvfrom(1024)
		print("received message:", data)
		break
		# exit(1)

		# sock.sendto(bytes([0, 0, 0, 10, 0, 0, 0, 0, 4, 1]), (UDP_IP, 5003))
	# sock.sendto(bytes([0, 0, 0, 1, 0, 0, 0, 0, 1, 1]), (UDP_IP, 5003))
	# sock.sendto(b'00010000\x04\x01', (UDP_IP, 5003))
	# sock.sendto(bytes([0, 0, 0, 1, 0, 0, 0, 0, 4, 0]), (UDP_IP, 5003))
	sleep(1)
	print('Repeating')