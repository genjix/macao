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
    if data[0] != "seed":
        print "NOTSEED"
        return -1
    seed = data[1]
    if len(seed) != 32:
        print "BADSEED"
        return -1
    print seed
    return 0

if __name__ == "__main__":
    sys.exit(main())

