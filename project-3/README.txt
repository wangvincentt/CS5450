1. slipping day used: 1day

2. This is Peer-to-Peer Internet Relay Chat (IRC) program, I change to port to larger scale, 
   so the program can contains more clients. 

3. When a program/client is running, any typed message with pressed enter will send to random neighbor (port +/- 1),
   and when neighbor receiving the message, it compare with its map to check whether received before with id and seqNum matching. 

4. If received message matches, program will send status-ACK back and re-distribute the message to its random neighbor again, so other clients can receive message. 

5. ID is in the format of "< Port @ hostname >" 

6. It periodically check(5 seconds) "Want" (which is ACK) message for next seqNum message, and update if message has received.

7. This program is pure peer-to-peer with no fault-tolerant and failure-recovery methods. 

