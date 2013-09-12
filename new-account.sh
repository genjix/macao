#!/bin/bash
SEED=$(sx newseed)
ADDRESS=$(echo $SEED | sx genaddr 0)
sx qrcode $ADDRESS qrcode.png
qrencode seed:$SEED -o qrcode-private.png -s 10

