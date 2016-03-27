/******************************************************************************/
/**  YAFFA - Yet Another Forth for Arduino                                   **/
/**  Version 0.6.1                                                           **/
/**                                                                          **/
/**  File: Yaffa.ino                                                         **/
/**  Copyright (C) 2012 Stuart Wood (swood@rochester.rr.com)                 **/
/**                                                                          **/
/**  This file is part of YAFFA.                                             **/
/**                                                                          **/
/**  YAFFA is free software: you can redistribute it and/or modify           **/
/**  it under the terms of the GNU General Public License as published by    **/
/**  the Free Software Foundation, either version 2 of the License, or       **/
/**  (at your option) any later version.                                     **/
/**                                                                          **/
/**  YAFFA is distributed in the hope that it will be useful,                **/
/**  but WITHOUT ANY WARRANTY; without even the implied warranty of          **/
/**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           **/
/**  GNU General Public License for more details.                            **/
/**                                                                          **/
/**  You should have received a copy of the GNU General Public License       **/
/**  along with YAFFA.  If not, see <http://www.gnu.org/licenses/>.          **/
/**                                                                          **/
/******************************************************************************/
/**                                                                          **/
/**  DESCRIPTION:                                                            **/
/**                                                                          **/
/**  YAFFA is an attempt to make a Forth environment for the Arduino UNO     **/
/**  that is as close as possible to the ANSI Forth draft specification      **/
/**  DPANS94.                                                                **/
/**                                                                          **/
/**  The goal is to support at a minimum the ANS Forth C core word set and   **/
/**  to implement wrappers for the basic I/O functions found in the Arduino  **/
/**  library.                                                                **/
/**  YAFFA uses two dictionaries, one for built in words and is stored in    **/
/**  flash memory, and the other for user defined words, that is found in    **/
/**  RAM.                                                                    **/
/**                                                                          **/
/******************************************************************************/
/**                                                                          **/
/**  REVISION HISTORY:                                                       **/
/**                                                                          **/
/**    0.6.1                                                                 **/
/**    - Documentation cleanup. thanks to Dr. Hugh Sasse, BSc(Hons), PhD     **/
/**    0.6                                                                   **/
/**    - Fixed PROGMEM compilation errors do to new compiler in Arduino 1.6  **/
/**    - Embedded the revision in to the compiled code.                      **/
/**    - Revision is now displayed in greeting at start up.                  **/
/**    - the interpreter not clears the word flags before it starts.         **/
/**    - Updated TICK, WORD, and FIND to make use of primitive calls for to  **/
/**      reduce code size.                                                   **/
/**    - Added word flag checks in dot_quote() and _s_quote().               **/
/**                                                                          **/
/**  NOTES:                                                                  **/
/**                                                                          **/
/**    - Compiler now gives "Low memory available, stability problems may    **/
/**      occur." warning. This is expected since most memory is reserved for **/
/**      the FORTH environment. Excessive recursive calls may overrun the C  **/
/**      stack.                                                              **/
/**                                                                          **/
/**  THINGS TO DO:                                                           **/
/**                                                                          **/
/**  CORE WORDS TO ADD:                                                      **/
/**      >NUMBER                                                             **/
/**                                                                          **/
/**  THINGS TO FIX:                                                          **/
/**                                                                          **/
/**    Fix the outer interpreter to use FIND instead of isWord               **/
/**    Fix Serial.Print(w, HEX) from displaying negative numbers as 32 bits  **/
/**    Fix ENVIRONMENT? Query to take a string reference from the stack.     **/
/**                                                                          **/
/******************************************************************************/

#include <EEPROM.h>
#include <pgmspace.h>
#include "Yaffa.h"
#include "Error_Codes.h"

/******************************************************************************/
/** Major and minor revision numbers                                         **/
/******************************************************************************/
#define YAFFA_MAJOR 0
#define YAFFA_MINOR 6
#define ALIGN_P(x) x=((cell_t *)((((cell_t) x) + 3) & -4))
#define ALIGN(x) x=((cell_t)((x + 3) & -4))

/******************************************************************************/
/**  Text Buffers and Associated Registers                                   **/
/******************************************************************************/
char* cpSource;                 // Pointer to the string location that we will
// evaluate. This could be the input buffer or
// some other location in memory
char* cpSourceEnd;              // Points to the end of the source string
char* cpToIn;                   // Points to a position in the source string
// that was the last character to be parsed
char cDelimiter = ' ';          // The parsers delimiter
char cInputBuffer[BUFFER_SIZE]; // Input Buffer that gets parsed
char cTokenBuffer[TOKEN_SIZE];  // Stores Single Parsed token to be acted on

/******************************************************************************/
/** Common Strings & Terminal Constants                                      **/
/******************************************************************************/
const char prompt_str[] PROGMEM = "> ";
const char compile_prompt_str[] PROGMEM = "|  ";
const char ok_str[] PROGMEM = "OK\n";
const char charset[] PROGMEM = "0123456789abcdef";
const char sp_str[] PROGMEM = " ";
const char tab_str[] PROGMEM = "\t";
const char hexidecimal_str[] PROGMEM = "$";
const char binary_str[] PROGMEM = "%";
const char zero_str[] PROGMEM = "0";

/******************************************************************************/
/**  Stacks and Associated Registers                                         **/
/**                                                                          **/
/**  Control Flow Stack is virtual right now. But it may be but onto the     **/
/**  data stack. Error checking should be done to make sure the data stack   **/
/**  is not corrupted, i.e. the same number of items are on the stack as     **/
/**  at the end of the colon-sys as before it is started.                    **/
/******************************************************************************/
int8_t tos = -1;                        // The data stack index
int8_t rtos = -1;                       // The return stack index
cell_t stack[STACK_SIZE];               // The data stack
cell_t rStack[RSTACK_SIZE];             // The return stack

/******************************************************************************/
/**  Flash Dictionary Structure                                              **/
/******************************************************************************/
const flashEntry_t* pFlashEntry = flashDict;   // Pointer into the flash Dictionary

/******************************************************************************/
/**  User Dictionary is stored in name space.                                **/
/******************************************************************************/
userEntry_t* pLastUserEntry = NULL;
userEntry_t* pUserEntry = NULL;
userEntry_t* pNewUserEntry = NULL;

/******************************************************************************/
/**  Flags - Internal State and Word                                         **/
/******************************************************************************/
uint8_t flags;                 // Internal Flags
#define ECHO_ON        0x01    // Echo characters typed on the serial input
#define NUM_PROC       0x02    // Pictured Numeric Process
#define EXECUTE        0x04

uint8_t wordFlags;             // Word flags

/******************************************************************************/
/** Error Handling                                                           **/
/******************************************************************************/
int8_t errorCode = 0;

/******************************************************************************/
/**  Forth Space (Name, Code and Data Space) and Associated Registers        **/
/******************************************************************************/
char* pPNO;                  // Pictured Numeric Output Pointer
cell_t forthSpace[FORTH_SIZE]; // Reserve a block on RAM for the forth environment
cell_t* pHere;              // HERE, points to the next free position in
// Forth Space
cell_t* pOldHere;           // Used by "colon-sys"
cell_t* pCodeStart;          // used by "colon-sys" and RECURSE
cell_t* pDoes;               // Used by CREATE and DOES>

/******************************************************************************/
/** Forth Global Variables                                                   **/
/******************************************************************************/
uint8_t state; // Holds the text interpreters compile/interpreter state
cell_t* ip;   // Instruction Pointer
cell_t w;     // Working Register
uint8_t base;  // stores the number conversion radix

// Missing pgmspace function for the ESP8266 environment - CAL
PGM_P strchr_P (PGM_P strPtr, int ch) {
  char ch1 = pgm_read_byte(strPtr);
  while ((ch1 != ch) && (ch1 != '\0')) {
    ch1 = pgm_read_byte(++strPtr);
  }
  return (ch1 == ch) ? strPtr : NULL;
}

/******************************************************************************/
/** Initialization                                                           **/
/******************************************************************************/

void yaffaInit(void) {

  flags = ECHO_ON;
  base = 10;

  pHere = pOldHere = forthSpace;

  print_P(PSTR("\nYAFFA - Yet Another Forth For Arduino, "));
  print_P(PSTR("Version "));
  ioDirector.printInt(YAFFA_MAJOR, DEC);
  print_P(PSTR("."));
  ioDirector.printInt(YAFFA_MINOR, DEC);
  print_P(PSTR("\nCopyright (C) 2012 Stuart Wood\r\n"));
  print_P(PSTR("This program comes with ABSOLUTELY NO WARRANTY.\r\n"));
  print_P(PSTR("This is free software, and you are welcome to\r\n"));
  print_P(PSTR("redistribute it under certain conditions.\r\n\r\n"));
  ioDirector.printInt(freeHeap());
  print_P(PSTR(" heap bytes free\r\n"));

  pFlashEntry = flashDict;
  w = 0;
  while (pgm_read_dword(&(pFlashEntry->name))) {
    w++;
    pFlashEntry++;
  }
  ioDirector.printInt(w);
  print_P(PSTR(" primitive Forth words\r\n\r\n"));

  print_P(prompt_str);
}

/******************************************************************************/
/** Outer interpreter                                                        **/
/******************************************************************************/

void yaffaRun(void) {

  cpSource = cpToIn = cInputBuffer;
  cpSourceEnd = cpSource + getLine(cpSource, BUFFER_SIZE);
  if (cpSourceEnd > cpSource) {
    interpreter();
    if (errorCode) errorCode = 0;
    else {
      if (!state) {
        print_P(ok_str);
      }
    }
  }
  if (state) {
    print_P(compile_prompt_str);
  } else {
    print_P(prompt_str);
  }
}

/******************************************************************************/
/** getKey                                                                   **/
/**   waits for the next valid char code from the IODirector and return its  **/
/**   value. Valid characters are:  Backspace, Carriage Return, Escape, Tab  **/
/**   and standard printable characters                                      **/
/******************************************************************************/
char getKey(void) {
  char inChar;

  while (1) {
    yield();
    // Get all input from the IODirector
    if (ioDirector.available()) {
      inChar = ioDirector.read();
      if (inChar == 8 || inChar == 9 || inChar == 13 ||
          inChar == 27 || isprint(inChar)) {
        return inChar;
      }
    }
  }
}

/******************************************************************************/
/** getLine                                                                 **/
/**   read in a line of text ended by a Carriage Return (ASCII 13)           **/
/**   Valid characters are:  Backspace, Carriage Return, Escape, Tab, and    **/
/**   standard printable characters. Passed the address to store the string, **/
/**   and Returns the length of the string stored                            **/
/******************************************************************************/
uint8_t getLine(char* addr, uint8_t length) {
  char inChar;
  char* start = addr;
  do {
    inChar = getKey();
    if (inChar == 8) {             // backspace
      if (addr > start) {
        *--addr = 0;
        if (flags & ECHO_ON) print_P(PSTR("\b \b"));
      }
    } else if (inChar == 9 || inChar == 27) { // TAB or ECS
      if (flags & ECHO_ON) ioDirector.printString("\a");         // Beep
    } else if (inChar == 13) {    // Carriage return
      if (flags & ECHO_ON) ioDirector.printString("\n");
      break;
    } else {
      if (flags & ECHO_ON) ioDirector.write(inChar);
      *addr++ = inChar;
      *addr = 0;
    }
  } while (addr < start + length);
  return ((uint8_t)(addr - start));
}

/******************************************************************************/
/** GetToken                                                                 **/
/**   Find the next token in the buffer and stores it into the token buffer  **/
/**   with a NULL terminator. Returns length of the token or 0 if at end off **/
/**   the buffer.                                                            **/
/******************************************************************************/
uint8_t getToken(void) {
  uint8_t tokenIdx = 0;
  while (cpToIn <= cpSourceEnd) {
    if ((*cpToIn == cDelimiter) || (*cpToIn == 0)) {
      cTokenBuffer[tokenIdx] = '\0';       // Terminate SubString
      cpToIn++;
      if (tokenIdx) return tokenIdx;
    } else {
      if (tokenIdx < (TOKEN_SIZE - 1)) {
        cTokenBuffer[tokenIdx++] = *cpToIn++;
      }
    }
  }
  // If we get to SourceEnd without a delimiter and the token buffer has
  // something in it return that. Else return 0 to show we found nothing
  if (tokenIdx) return tokenIdx;
  else return 0;
}

/******************************************************************************/
/** Interpreter - Interprets a new string                                     **/
/**                                                                          **/
/** Parse the new line. For each parsed subString, try to execute it.  If it **/
/** can't be executed, try to interpret it as a number.  If that fails,      **/
/** signal an error.                                                         **/
/******************************************************************************/
void interpreter(void) {
  func function;

  while (getToken()) {
    yield();
    if (state) {
      /*************************/
      /** Compile Mode        **/
      /*************************/
      if (isWord(cTokenBuffer)) {
        if (wordFlags & IMMEDIATE) {
          if (w > 255) {
            rPush(0);            // Push 0 as our return address
            ip = (cell_t *)w;          // set the ip to the XT (memory location)
            executeWord();
          } else {
            function = (func) pgm_read_dword(&(flashDict[w - 1].function));
            function();
            if (errorCode) return;
          }
          executeWord();
        } else {
          *pHere++ = w;
        }
      } else if (isNumber(cTokenBuffer)) {
        _literal();
      } else {
        push(-13);
        _throw();
      }
    } else {

      /************************/
      /* Interpret Mode       */
      /************************/
      if (isWord(cTokenBuffer)) {
        if (wordFlags & COMP_ONLY) {
          push(-14);
          _throw();
          return;
        }

        if (w > 255) {
          rPush(0);                  // push 0 as return address
          ip = (cell_t *)w;          // set the ip to the XT (memory location)
          executeWord();
          if (errorCode) return;
        } else {
          function = (func) pgm_read_dword(&(flashDict[w - 1].function));
          function();
          if (errorCode) return;
        }
        //        executeWord(); // CAL why is this here ????
      } else if (isNumber(cTokenBuffer)) {
      } else {
        push(-13);
        _throw();
        return;
      }
    }
  }
  cpToIn = cpSource;
}

/******************************************************************************/
/** Virtual Machine that executes Code Space                                 **/
/******************************************************************************/
void executeWord(void) {
  func function;
  flags |= EXECUTE;

  yield();  // CAL

  while (ip != NULL) {
    w = *ip++;
    if (w > 255) {
      // ip is an address in code space
      rPush((size_t)ip);        // push the address to return to
      ip = (cell_t*)w;          // set the ip to the new address
    }
    else {
      function = (func) pgm_read_dword(&(flashDict[w - 1].function));
      function();
      if (errorCode) return;
    }
  }
  flags &= ~EXECUTE;
}

/******************************************************************************/
/** Find the word in the Dictionaries                                        **/
/** Return execution token value in the w register.                          **/
/** Returns 1 if the word is found                                           **/
/**                                                                          **/
/** Also set wordFlags, from the definition of the word.                     **/
/**                                                                          **/
/** Could this be come the word FIND or ' (tick)?                            **/
/******************************************************************************/
uint8_t isWord(char* addr) {
  uint8_t index = 0;

  yield();

  pUserEntry = pLastUserEntry;
  // First search through the user dictionary
  while (pUserEntry) {
    if (strcmp(pUserEntry->name, addr) == 0) {
      wordFlags = pUserEntry->flags;
      w = (cell_t) pUserEntry->cfa;
      return 1;
    }
    pUserEntry = (userEntry_t*)pUserEntry->prevEntry;
  }

  // Second search through the flash Dictionary
  while (pgm_read_dword(&(flashDict[index].name))) {
    if (!strcasecmp_P(addr, (char*) pgm_read_dword(&(flashDict[index].name)))) {
      w = index + 1;
      wordFlags = pgm_read_byte(&(flashDict[index].flags));
      delay(1); // WHY IS THIS NECESSARY ? CAL
      if (wordFlags & SMUDGE) {
        return 0;
      }
      else {
        return 1;
      }
    }
    index++;
  }
  w = 0;
  return 0;
}

/******************************************************************************/
/** Attempt to interpret token as a number.  If it looks like a number, push **/
/** it on the stack and return 1.  Otherwise, push nothing and return 0.     **/
/**                                                                          **/
/** Numbers without a prefix are assumed to be decimal.  Decimal numbers may **/
/** have a negative sign in front which does a 2's complement conversion at  **/
/** the end.  Prefixes are # for decimal, $ for hexadecimal, and % for       **/
/** binary.                                                                  **/
/******************************************************************************/
uint8_t isNumber(char* subString) {
  unsigned char negate = 0;                  // flag if number is negative
  cell_t tempBase = base;
  cell_t number = 0;

  wordFlags = 0;

  // Look at the initial character, handling either '-', '$', or '%'
  switch (*subString) {
    case '$':  base = 16;  goto SKIP;
    case '%':  base = 2;   goto SKIP;
    case '#':  base = 10;  goto SKIP;
    case '-':  negate = 1;
SKIP:                // common code to skip initial character
      subString++;
      break;
  }
  // Iterate over rest of token, and if rest of digits are in
  // the valid set of characters, accumulate them.  If any
  // invalid characters found, abort and return 0.
  while (*subString) {
    PGM_P pos = strchr_P(charset, (int)tolower(*subString));
    cell_t offset = pos - charset;
    if ((offset < base) && (offset > -1))
      number = (number * base) + (pos - charset);
    else {
      base = tempBase;
      return 0;           // exit, signalling subString isn't a number
    }
    subString++;
  }
  if (negate) number = ~number + 1;     // apply sign, if necessary
  push(number);
  base = tempBase;
  return 1;
}

/******************************************************************************/
/** freeHeap returns the amount of free heap remaining.                      **/
/******************************************************************************/
static unsigned int freeHeap(void) {
  return ESP.getFreeHeap();
}

/******************************************************************************/
/** freeForthSpacePercent returns the percent of free forth space remaining. **/
/******************************************************************************/
static unsigned int freeForthSpacePercent(void) {
  cell_t percentage = 100 - (((pHere - forthSpace) * 100) / FORTH_SIZE);
  return percentage;
}

/******************************************************************************/
/** Start a New Entry in the Dictionary                                      **/
/******************************************************************************/
void openEntry(void) {
  uint8_t index = 0;
  pOldHere = pHere;            // Save the old location of HERE so we can
  // abort out of the new definition
  pNewUserEntry = (userEntry_t*)pHere;
  if (pLastUserEntry == NULL)
    pNewUserEntry->prevEntry = 0;              // Initialize User Dictionary
  else pNewUserEntry->prevEntry = (size_t)pLastUserEntry;
  if (!getToken()) {
    push(-16);
    _throw();
  }
  uint8_t *ptr = (uint8_t*) pNewUserEntry->name;
  do {
    *ptr++ = cTokenBuffer[index++];
  } while (cTokenBuffer[index] != '\0');
  *ptr++ = '\0';
  pHere = (cell_t *) ptr;
  ALIGN_P(pHere);

  pNewUserEntry->cfa = (size_t)pHere;
  pCodeStart = (cell_t*)pHere;
}

/******************************************************************************/
/** Finish an new Entry in the Dictionary                                    **/
/******************************************************************************/
void closeEntry(void) {
  if (errorCode == 0) {
    *pHere++ = EXIT_IDX;
    pNewUserEntry->flags = 0; // clear the word's flags
    pLastUserEntry = pNewUserEntry;
  } else pHere = pOldHere;   // Revert pHere to what it was before the start
  // of the new word definition
}

/******************************************************************************/
/** Stack Functions                                                          **/
/******************************************************************************/
void push(cell_t value) {
  if (tos < STACK_SIZE - 1) {
    stack[++tos] = value;
  } else {
    stack[tos] = -3;
    _throw();
  }
}

void rPush(cell_t value) {
  if (rtos < RSTACK_SIZE - 1) {
    rStack[++rtos] = value;
  } else {
    push(-5);
    _throw();
  }
}

cell_t pop(void) {
  if (tos > -1) {
    return (stack[tos--]);
  } else {
    push(-4);
    _throw();
  }
  return 0;
}

cell_t rPop(void) {
  if (rtos > -1) {
    return (rStack[rtos--]);
  } else {
    push(-6);
    _throw();
  }
  return 0;
}

/******************************************************************************/
/** String and Serial Functions                                              **/
/******************************************************************************/
void displayValue(void) {
  switch (base) {
    case 10: ioDirector.printInt(w, DEC);
      break;
    case 16:
      print_P(hexidecimal_str);
      ioDirector.printInt(w, HEX);
      break;
    case 8:  ioDirector.printInt(w, OCT);
      break;
    case 2:
      print_P(binary_str);
      ioDirector.printInt(w, BIN);
      break;
  }
  print_P(sp_str);
}

uint8_t print_P(PGM_P ptr) {
  char ch;
  uint8_t i = 79;
  // Process the queues to make room
  ioDirector.processQueues();
  for (; i > 0; i--) {
    ch = pgm_read_byte(ptr++);
    if (ch == 0) break;
    ioDirector.write(ch);
  }
  return (79 - i);
}

/******************************************************************************/
/** Functions for decompiling words                                          **/
/******************************************************************************/
char* xtToName(cell_t xt) {
  uint8_t index = 0;
  uint8_t length = 0;

  pUserEntry = pLastUserEntry;

  // Second Search through the flash Dictionary
  if (xt < 256) {
    print_P((char*) pgm_read_dword(&(flashDict[xt - 1].name)));
  } else {
    while (pUserEntry) {
      if (pUserEntry->cfa == xt) {
        ioDirector.printString(pUserEntry->name);
        break;
      }
      pUserEntry = (userEntry_t*)pUserEntry->prevEntry;
    }
  }
  return 0;
}


