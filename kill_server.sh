ss --all --tcp --process | grep 12345 | grep -Eo pid\=[0-9]* | grep -o [0-9]* | xargs -i kill -9 {}
ss --all --tcp --process | grep 30000 | grep -Eo pid\=[0-9]* | grep -o [0-9]* | xargs -i kill -9 {}
rm /tmp/*socket
iptables --flush
