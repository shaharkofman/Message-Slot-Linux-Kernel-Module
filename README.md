# Linux Kernel Message Slot Driver

**A custom Linux kernel module implementing a multi-channel Inter-Process Communication (IPC) mechanism via character devices.**

## Overview

This project implements a character device driver (`message_slot`) that allows distinct processes to exchange messages through a unified interface file. Unlike standard pipes or queues, this driver implements a **channel-based architecture**, where a single device file can host multiple independent data streams (channels).

The project demonstrates low-level kernel programming concepts including:
* Character device registration and management.
* User-space to Kernel-space memory transfer (`copy_from_user` / `copy_to_user`).
* Custom `ioctl` command implementation.
* Dynamic memory management within the kernel (using `kmalloc`/`kfree`).
* Complex data structures (Nested Linked Lists) for efficient resource scaling.

## Key Features

* **Multi-Channel Support:** A single device file supports up to $2^{32}$ independent channels.
* **Dynamic Allocation:** Channels are allocated lazily upon first use, optimizing memory usage.
* **Censorship Mode:** An optional feature toggled via `ioctl` that processes messages in kernel space, censoring specific byte patterns before storage.
* **Atomic Operations:** Ensures data integrity during read/write operations.
* **Standard Interface:** Complies with standard Linux file operations (`open`, `read`, `write`, `ioctl`, `release`).

## Architecture

The driver uses a **Nested Linked List** data structure to manage memory efficiently:

1.  **Slots List:** A global linked list where each node represents an active device file (distinguished by Minor Number).
2.  **Channels List:** Each Slot node contains a pointer to a linked list of Channels.

```mermaid
graph LR
    Head --> Slot1[Slot 0]
    Slot1 --> Slot2[Slot 1]
    Slot1 --> Ch1[Channel 10]
    Ch1 --> Ch2[Channel 45]
    Slot2 --> Ch3[Channel 5]
