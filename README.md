# sermux
Serial Multiplexor - connect multiple applications to a single serial port.

I have a serial port which is connected to a low-level board and I want to talk to it with multiple applications.
I can use some sort of locking mechanism but that doesn't work when one of the applications is a Ruby on Rails
app and it ends up holding on to the lock.

Likewise, I have a serial port concentrator which listens on a specific TCP port and maps that traffic to the
serial port, but again it doesn't support more than one client.

sermux listens on a well-known port for connections (for example, port 5001).
Each connection is mapped to the remote serial concentrator or serial device.
The first connection which sends data will end up owning the serial port, until it's idle for
five seconds (configurable).
After that, it is kicked off the device but its connection to sermux is kept open.
Any other device can then use the serial port, on the same terms.
