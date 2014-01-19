jhPrimeminer
============
jhPrimeminer is an optimized pool miner for primecoin.

This is Ray De Bourbon's 3.3 build, merged with deschlers linux version.
Imported girino's tweaks for Cygwin and OSX.



Pre-requisites:
CentOS:
yum groupinstall "Development Tools"
yum install openssl openssl-devel openssh-clients gmp gmp-devel gmp-static git

Ubuntu:
apt-get install build-essential libssl-dev openssl git libgmp10 libgmp-dev

OSX:
install xcode
install mac ports
sudo port install openssl openssh gmp git

Cygwin:
install g++, libssl-dev, openssl, git, libgmp, libgmp-dev, etc using setup.exe




Build instructions:
git clone https://github.com/tandyuk/jhPrimeminer.git
cd jhPrimeminer
make



Command Line Options:








If you found this helpful PLEASE support my work.
XPM: AYwmNUt6tjZJ1nPPUxNiLCgy1D591RoFn4
BTC: 1P6YrvFkwYGcw9sEFVQt32Cn7JJKx4pFG2