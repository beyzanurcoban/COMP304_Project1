# COMP304 - Project 1

This is the first project of the COMP304 course (Fall '21) in Koç University.

## Contributors
Beyzanur Çoban\
Kerem Girenes

## Compiling

Use the following to compile and run the shell.

```bash
gcc shellington.c -o shellington
./shellington
```

## Function Usages

Examine the following examples for function executions

### Bookmark Command

To list all bookmarks
```bash
bookmark -l
```

To save a bookmark
```bash
bookmark "command to be saved"
```

To delete a bookmark at index
```bash
bookmark -d 1
```

To execute a bookmark at index
```bash
bookmark -i 1
```

### Short Command

To set a short at current directory
```bash
short set "alias"
```

To jump to a directory with corresponding alias
```bash
short jump "alias"
```

To view all shorts
```bash
short view all
```

### Remind Me Command

To set a reminder
```bash
remindme 10.45 "message to be displayed"
```

### Beyza's Awesome Command

To set a timer in seconds
```bash
beyza 10
```

### Kerem's Awesome Command

To display an aerial advertise
```bash
kerem plane my-message-here
```

To display an emergency
```bash
kerem siren my-message-here
```

To protest things you don't like
```bash
kerem protester my-message-here
```

### PS Traverse Command
Before using this command, make sure that you run "make" command first. Necessary files will be generated during make call.

To traverse in DFS (first argument is PID)
```bash
pstraverse 0 -d

pstraverse 1 -d

pstraverse 1177 -d
```

To check the process tree prints by kernel module
```bash
dmesg
```

To clear dmesg
```bash
sudo dmesg --clear
```
