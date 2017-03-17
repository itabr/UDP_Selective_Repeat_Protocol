Samir Tabriz / Stephanie Lam

Usage for server : ./server <port>

Usage for client : ./client <hostname> <port> <filename>

The purpose of this project is to use UDP Socket and C programming language to implement a
reliable data transfer protocol similar to that in TCP.

In this project, we implemented a simple window-based, reliable data transfer
protocol built on top of Selective Repeat protocol.

The two programs communicate using the User Datagram Protocol (UDP) socket, which does
not guarantee reliable data delivery.

The client program take the hostname and port number of the server, and the name of a
file it wants to retrieve from the server as a command line arguments.

This programs acts as both a network application (file transfer program) as well as a
reliable transport layer protocol built over the unreliable UDP transport layer.

To simulate package lost situation use command bellow:

$ sudo qdisc add dev lo root netem loss 10% -> sets up a network emulation with 10% loss w/o delay

$ sudo tc qdisc del dev lo root

$ sudo qdisc add dev lo root netem unorder gap 5 delay 100ms loss 10%  -> sets up a network emulation with 10% loss w 100ms delay