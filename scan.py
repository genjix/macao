import sys
from qrtools import QR

qr = QR()
qr.decode_webcam()
if qr.data_decode[qr.data_type](qr.data) == 'NULL':
    print >> sys.stder, "Fail."
else:
    print qr.data

