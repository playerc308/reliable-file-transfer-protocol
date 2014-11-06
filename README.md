# Reliable File Transfer Protocol
Unlimited file sizes are supported!!!

## Group Member
Peng Chu, Li Shen, Jie Zhu

## Introduction
The object of this project is to design a simple transport protocol that provides reliable file transfer over UDP. The protocol will be responsible for ensuring data is delivered in order, without duplicates, missing data, or errors.

## Implementation
For sender side, the packet format is designed as follow.

+------------------------------+
|           BKDRHash           |
+---------------+--------------+
| serial number | payload size |
+---------------+--------------+
|            payload           |
+------------------------------+

BKDRHash is used to ensure the packet content is correct. If serial number == 0, payload is the file name.

For receiver side, the packet format is very simple.

+---------------+
| serial number |
+---------------+

After the receiver checks there is no error in the packet, it will reply with a serial number.

The protocol support unlimited file sizes by using mmap(). At most 8 Megabytes file contents are mapped into memory at once. When this part is transferred to the recevier, these data unmapped from memory by munmap() and next 8 Megabytes file contents are mapped by again until all the file contents are successfully sent to the receiver.
