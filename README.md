# BitChord: Peer-to-Peer File Sharing Engine

## Overview
BitChord is a C++-based Peer-to-Peer (P2P) file sharing engine centered around a scalable Distributed Hash Table (DHT) implemented using the Chord protocol. Developed between October 2024 and November 2024 for the CS744: Design and Engineering of Computing Systems course under Prof. Purushottam Kulkarni, the system provides decentralized peer lookup without requiring a centralized directory. 

## Key Features
* Implemented a Distributed Hash Table (DHT) using the scalable Chord protocol in C++ for decentralized peer lookup.
* Engineered a robust logical identifier ring topology that allows peers to dynamically join or leave without disrupting system stability.
* Leveraged SHA-1 hashing and POSIX pthreads to manage multi-threaded network connections and concurrent file transfers.
* Maintained ring stability through a background stabilization mechanism that periodically verifies and updates node relationships independently of foreground file-sharing operations.

## System Architecture
The logical architecture of BitChord is divided into three interacting layers:
* **Overlay Layer:** Maintains the logical Chord ring and performs decentralized peer lookup.
* **File Layer:** Maps files to the identifier space to determine the responsible peer.
* **Communication Layer:** Manages TCP/IP connections, protocol messages, and concurrent file transfers utilizing pthreads.

## Consistent Hashing and Identifier Space
BitChord uses a circular identifier space where both files and peers are represented by fixed-size identifiers generated via SHA-1 hashing. The mapping is defined mathematically as $id_{file} = H(filename)$ for files, and $id_{peer} = H(peer\ address)$ for participating nodes. A file is subsequently associated with the first peer encountered clockwise from its identifier on the ring.

## Multi-threaded Network Communication
To prevent slow network transfers from blocking unrelated peer operations, the system relies on a multi-threaded design. Using low-level C++ socket programming (including `socket()`, `bind()`, `listen()`, `accept()`, and `connect()`), BitChord separates tasks into dedicated threads for lookups, incoming connections, file transfers, and ring maintenance.
