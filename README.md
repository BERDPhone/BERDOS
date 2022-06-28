# BDOS
A mobile phone operating system built on top of the Raspberry Pi Pico.

Currently a work in progress, open an issue to request a feature, and feel free to post a thread on discussions, to find other ways to help.

## Setup
Have cmake installed, navigate to 'build/' and run `cmake .. && make boot`
The file should be generated in 'BDOS/build/src/os/boot.uf2'

# SOFTWARE REQUIREMENTS


## File System


OS (Stored aboard Raspberry Pico aka. this repository): 

User-Level Applications/Programs (Stored aboard a μSD card, not in existance yet)


## Operating System


### Tasks:



* Memory Management
    * Stack: The location of temporary memory used for multitasking and switching processes. The operating system must manage stack size, move the stack boundary, or report a stack overflow.
    * Allocation: The operating system assigns and keeps the locations or addresses of the heaps, code, and stacks in the random access memory. The operating system must do this with good allocation algorithms and must remain efficient to prevent memory leaks.
* Process Management
    * Multitasking (Multiprogramming):
        * Preemptive: There are two types of multitasking, cooperative and preemptive, but the system ought to be designed as preemptive to prevent infinite looping and provide higher efficiency and consistency. Interrupts should be regular unless only one process is running, so the operating system should have the capacity to regain control via an external interrupt timer.
        * Interpret
    * Context Switching: How the operating system changes the process active in the CPU. This is done following interrupts, and this facilitates multitasking.
    * Task Manager
* Device Discover & Initialization
* Basic User Interface
* Imposing Security Regime


### Components:



* Kernel: (Monolithic Kernel: I/O interacts with the kernel via APIs, and the kernel provides the interface for all software and hardware)? The software provides the means for programs to execute. Task:
    * Handel Interrupts and System Calls: The kernel handles the internal operations and executes the functions to facilitate software and deal with hardware interrupts.
* Shell: A program that provides an interface between the user and computer. Tasks: 
    * Program Access: The shell allows the user to select and run a user-level program effectively and efficiently.
    * Storage Operation: The shell provides trivial file-manipulation functionality, such as moving and copying files across the system and opening directories. 
* Graphical User Interface: The interface which facilitates user interaction with the shell and thereby with the computer. Tasks:
    * Display Information.


### Scope:



* Make the hardware integrate seamlessly with the software (it’s an abstraction from the hardware i.e. volume)./
* Handles Encryption of data
* Battery Handling
* Provide an API for programs to use.
* Operational and efficient kernel: The buffer manages everything and ultimately controls the system. The kernel handles the hardware with device drivers and optimizes common resource usage.


## User-Level Programs/Applications


### Scope:



* Provide a fully functional program that the user can use.
* Not expose any functions to other programs, unless through making its own API. (i.e. being able to create a contact in the call app despite there being a contact app, with the contact app API)
* Must have the Model and View distinctly separate


### Essential Programs:



* Lock Screen
* Home Screen
* Edit User Details
* Encryption


### Desired Applications/Features:



* Cellular Calling
* Messaging
* Email
* Touchscreen Interface
* Keyboard
* Wifi as backup
* Games
    * Conway’s Game of Life
    * Chess
    * Connect Four
* Music
* Clock
* Weather
* Notes
* Books
* Dictionary
    * Autocorrect
* Reminders
* Settings
    * Terminal*
    * Cellular Data
    * Notifications
    * General
        * Date & Time
        * Keyboard Layout
        * Language & Region
        * Dictionary
    * Display
    * UX-UI Settings
    * Wallpaper
    * Password
    * Application Settings
* Calculator
* Files/Storage
* Contacts
* Map*
* Calendar
* Password Protection


# Resources

<span style="text-decoration:underline;">OS Dev Wiki</span>

[https://wiki.osdev.org/Main_Page](https://wiki.osdev.org/Main_Page)<span style="text-decoration:underline;"> </span>

<span style="text-decoration:underline;">Piccolo OS:</span>

[https://github.com/garyexplains/piccolo_os_v1](https://github.com/garyexplains/piccolo_os_v1) 

<span style="text-decoration:underline;">Component Documentation Links</span>:

[Motherboard (Raspberry Pi Pico) Documentation Directory](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html)

[Motherboard (Raspberry Pi Pico) Development Kit](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf) 

[Motherboard (Raspberry Pi Pico) Datasheet](https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf)

[Processor (RP2040) Documentation Directory](https://www.raspberrypi.com/documentation/microcontrollers/rp2040.html#welcome-to-rp2040)

[Processor (RP2040) Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
