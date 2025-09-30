#!/bin/bash

echo "Initiating SFF congestion test..."

# Overload the server with requests
# Threads A and B will be busy with long requests
./wclient localhost 10000 "/spin.cgi?5" & 
./wclient localhost 10000 "/spin.cgi?5" &

# These requests will arrive while the threads are busy, forming the queue
./wclient localhost 10000 "/spin.cgi?4" &
./wclient localhost 10000 "/spin.cgi?3" &
./wclient localhost 10000 "/index.html" &      # The shortest request, in the middle of the queue!
./wclient localhost 10000 "/spin.cgi?2" &

echo "Requests sent. Waiting for completion..."
wait
echo "Test completed."