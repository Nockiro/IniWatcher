# IniWatcher
A lightweight program to watch the status of an ini file key value.

## What does it do?
The IniWatcher watches a given value of an [INI file](https://en.wikipedia.org/wiki/INI_file) (a list of lines with `key=value`) and checks if it is a certain, predefined value. 
If it is, a small window overlay shows "Enabled", otherwise it's "Disabled".



It currently does **not** support sections as I didn't need that.
If you need that, feel free to open an issue or a pull request.

Since it's supposed to be as small as it can be, it doesn't feature more than a small window and the possiblity to remember its window location.
You can exit the program by using the ALT+F4 shortcut after clicking on the window.

### Build
To build the program yourself, you can use the gcc from [MinGW for Windows](http://www.mingw.org/):
```bash
gcc -Wall -std=c99 -mwindows -s iniwatcher.c -o bin\IniWatcher.exe -lgdi32 -lshlwapi
```

To build with MSVC, use the following command:
```
cl iniwatcher.c gdi32.lib shlwapi.lib user32.lib shell32.lib
```

### Usage
Example INI file "settings.ini"

```ini
key=value
abc=moretest
enabled=true
reward=sink
```

If you call the IniWatcher with the following command line ..
```bash
IniWatcher.exe -file settings.ini -key enabled -value true
```

.. you see the current state (disabled/enabled) of the given switch and you can update it by hovering over it with the mouse since it only updates on window redraws.
Again, it's supposed to be as simple as possible at this point.


## Why?
I wanted to try programming directly with the Win32 API and needed to permanently display the status of a certain switch of an INI file.
Secondly, I needed to see the current status without navigating to and opening the file every time.