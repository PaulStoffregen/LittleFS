### Additional Methods

### Formatting

```myfs.quickFormat()``` performs a quick format of the media specified
```myfs.lowLevelFormat(char, Serial Port)``` performs a low level format.  Uses the specified character, e.g, "." to show progress and is sent to the specified Serial port.

### File Operations

```file.peek()``` Return the next available byte without consuming it. (SDFat class reference)
```file.available()``` The number of bytes available from the current position to EOF 
```file.flush()``` Ensures that any bytes written to the file are physically saved
```file.truncate(bytes)``` Truncate a file to a specified length
```file.seek(position, mode)``` Sets the position in a file based on mode. There are 3 modes available:

1. SeekSet - Sets a file's position.
2. SeekCur - Set the files position to current position + pos
3. SeekEnd - Set the files position to end-of-file + pos

```file.position()``` The current position in a file
```file.size()``` The total number of bytes in a file 

