import sys
import subprocess
from qrtools import QR

def call(command):
    return subprocess.check_output(command, shell=True).rstrip("\n")

def main():
    if len(sys.argv) != 2:
        print "FAIL"
        return -1
    qr = QR()
    qr.decode_webcam()
    if qr.data == "NULL":
        print "FAIL"
        return -1
    data = qr.data.split(":", 1)
    if len(data) != 2:
        print "FAIL"
        return -1
    if data[0] != "seed":
        print "NOTSEED"
        return -1
    password = sys.argv[1]
    seed = call("echo \"%s\" | base64 -d | ccat -K %s" % (data[1], password))
    if len(seed) != 32:
        print "BADSEED"
        return -1
    print seed
    return 0

if __name__ == "__main__":
    sys.exit(main())

