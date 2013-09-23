jhPrimeminer
============

jhPrimeminer is a optimized pool miner for primecoin.

This is based on Mumu's v8.0 build, merged with deschlers linux version.

Requirements
Openssl and libgmp.



Build instructions:

CentOS:

yum groupinstall "Development Tools"

yum install openssl openssl-devel openssh-clients gmp gmp-devel gmp-static git

git clone --branch mumu-v8 https://github.com/tandyuk/jhPrimeminer.git

cd jhPrimeminer

make


Ubuntu:

apt-get install build-essential libssl-dev openssl git libgmp libgmp-dev

git clone --branch mumu-v8 https://github.com/tandyuk/jhPrimeminer.git

cd jhPrimeminer

make



If you found this helpful PLEASE support my work.

XPM: AYwmNUt6tjZJ1nPPUxNiLCgy1D591RoFn4

BTC: 1P6YrvFkwYGcw9sEFVQt32Cn7JJKx4pFG2
