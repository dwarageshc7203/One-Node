import socket
import threading
import sys

def handle_client(conn, addr):
    try:
        print(f"Connected by {addr}")
        conn.sendall(b'\x01' + (b'\x55' * 32)) # serverNonce
        print("Sent server greeting")
        
        data = conn.recv(1024)
        print(f"Received {len(data)} bytes")
        
        if len(data) >= 66:
            clientNonce = data[:32]
            hmacC = data[32:64]
            idLen = (data[64] << 8) | data[65]
            print(f"Parsed idLen: {idLen}")
            if len(data) >= 66 + idLen:
                recvId = data[66:66+idLen]
                print(f"recvId: {recvId}")
            else:
                print("Not enough data for recvId")
        else:
            print("Not enough data for header")
            
        conn.close()
    except Exception as e:
        print(f"Error: {e}")

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('127.0.0.1', 45679))
    s.listen(1)
    print("Listening on 45679...")
    while True:
        conn, addr = s.accept()
        t = threading.Thread(target=handle_client, args=(conn, addr))
        t.start()

if __name__ == '__main__':
    main()
