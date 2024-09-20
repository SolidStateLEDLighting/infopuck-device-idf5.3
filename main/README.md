# System (Main) Component

We called this the System but because all this is inside a directory called Main, the project recognizes Main as the name of the component.  It is possible to change the folder name but this requires some special work that would be more complex and not worth doing at this point.  Main is set as the default name of the primary component.

The system is made up of one object and three tasks.  This object is a singleton and has no destructor.  The system can only be restarted upon a full shutdown and reboot of the hardware.

The system mangages everything at a high level.  It does the following:
* Initalizes all components.
* Has the power to shutdown and restart components (objects).
* May open and close external communications.
* Triggers periodic system-wide tasks.
* Handles error recovery on a system scale (if possible).
* It will be in charge of putting the system to sleep. **(code is in a testing state at this time)**

You may follow these links to System documentation:
1) [System Abstractions](./docs/system_abstractions.md)
2) [System Block Diagram](./docs/system_blocks.md)
3) [System Flowcharts](./docs/system_flowcharts.md)
4) [System Operations](./docs/system_operations.md)
5) [System Sequences](./docs/system_sequences.md)
6) [System State Models](./docs/system_state_models.md)
___    