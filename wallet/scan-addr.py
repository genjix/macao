import sys
from qrtools import QR

def main():
    qr = QR()
    qr.decode_webcam()
    if qr.data == "NULL":
        print "FAIL"
        return -1
    data = qr.data.split(":")
    if len(data) != 2:
        print "FAIL"
        return -1
    if data[0] != "bitcoin":
        print "NOTADDR"
        return -1
    print data[1]
    return 0

if __name__ == "__main__":
    sys.exit(main())

