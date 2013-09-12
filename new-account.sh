#!/bin/bash
SEED=$(sx newseed)
ADDRESS=$(echo $SEED | sx genaddr 0)
sx qrcode $ADDRESS qrcode.png
echo $SEED | sx mnemonic
ENCSEED=$(echo $SEED | ccencrypt -ef | base64)
echo $ENCSEED

