ESP8266ForthLoader Running Instructions

Prerequisites
====================
1. ESP8266ForthLoader.jar
2. jssc.jar (version 2.6.0 or newer)

Executing From Shell
====================
1. Change directory to where jar files are located
2. export FORTH_HOME=<“full path to directory with esp8266forth project files>”
3. export CLASSPATH=“./ESP8266ForthLoader.jar:./jssc.jar”
4. java com.craigl.esp8266forthloader.ESP8266ForthLoader

Alternatively add the following to your .profile file in your home directory
====================
# Items for ESP8266ForthLoader for ESP8266
export FORTH_HOME=~/Documents/dev/ESP8266ForthLoader/projects
export CLASSPATH=~/Documents/dev/ESP8266ForthLoader/ESP8266ForthLoader.jar:~/Documents/dev/ESP8266ForthLoader/jssc.jar
alias fl="java com.craigl.esp8266forthloader.ESP8266ForthLoader"

Operation
====================
1. Connect your ESP8266Forth device to your computer
2. Execute ESP8266ForthLoader as described above
3. Once loader is operational, select appropriate Serial Port from drop down list
4. Click Open button in the UI to open the selected Serial Port
5. Type #help into the Input Area to see the help info
6. Type ESP8266Forth commands to interact with ESP8266Forth
7. Type #include <filename> to load Forth code from a file
8. Use up/down cursor keys to retrieve command history
9. Type #bye to terminate ESP8266ForthLoader


NOTES:
1. ESP8266ForthLoader’s window can be resized as necessary
