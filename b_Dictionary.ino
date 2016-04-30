/******************************************************************************/
/**  YAFFA - Yet Another Forth for Arduino                                   **/
/**                                                                          **/
/**  File: Dictionary.ino                                                    **/
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

#include "Yaffa.h"

const char not_done_str[] PROGMEM = " NOT Implemented Yet \n\r";

/******************************************************************************/
/**                       Primitives for Control Flow                        **/
/******************************************************************************/
const PROGMEM char jump_str[] = "jump";
static void _jump(void) {
  ip = (cell_t*)((size_t)ip + *ip);
}

const PROGMEM char zjump_str[] = "zjump";
static void _zjump(void) {
  if (!pop()) ip = (cell_t*)((size_t)ip + *ip);
  else ip++;
}

const PROGMEM char subroutine_str[] = "subroutine";
static void _subroutine(void) {
  *pDoes = (cell_t) * ip++;
}

const PROGMEM char do_sys_str[] = "do-sys";
// ( n1|u1 n2|u2 -- ) (R: -- loop_sys )
// Set up loop control parameters with index n2|u2 and limit n1|u1. An ambiguous
// condition exists if n1|u1 and n2|u2 are not the same type. Anything already
// on the return stack becomes unavailable until the loop-control parameters
// are discarded.
static void _do_sys(void) {
  rPush(LOOP_SYS);
  rPush(pop());   // push index on to return stack
  rPush(pop());   // push limit on to return stack
}

const PROGMEM char loop_sys_str[] = "loop-sys";
// ( n1|u1 n2|u2 -- ) (R: -- loop_sys )
// Set up loop control parameters with index n2|u2 and limit n1|u1. An ambiguous
// condition exists if n1|u1 and n2|u2 are not the same type. Anything already
// on the return stack becomes unavailable until the loop-control parameters
// are discarded.
static void _loop_sys(void) {
  cell_t limit = rPop();    // fetch limit
  cell_t index = rPop();    // fetch index
  index++;
  if (limit - index) {
    rPush(index);
    rPush(limit);
    ip = (cell_t*)*ip;
  } else {
    ip++;
    if (rPop() != LOOP_SYS) {
      push(-22);
      _throw();
      return;
    }
  }
}

const PROGMEM char leave_sys_str[] = "leave-sys";
// ( -- ) (R: loop-sys -- )
// Discard the current loop control parameters. An ambiguous condition exists
// if they are unavailable. Continue execution immediately following the
// innermost syntactically enclosing DO ... LOOP or DO ... +LOOP.
static void _leave_sys(void) {
  rPop();    // fetch limit
  rPop();    // fetch index
  if (rPop() != LOOP_SYS) {
    push(-22);
    _throw();
    return;
  }
  ip = (cell_t*)*ip;
}

const PROGMEM char plus_loop_sys_str[] = "plus_loop-sys";
// ( n1|u1 n2|u2 -- ) (R: -- loop_sys )
// Set up loop control parameters with index n2|u2 and limit n1|u1. An ambiguous
// condition exists if n1|u1 and n2|u2 are not the same type. Anything already
// on the return stack becomes unavailable until the loop-control parameters
// are discarded.
static void _plus_loop_sys(void) {
  cell_t limit = rPop();    // fetch limit
  cell_t index = rPop();    // fetch index
  index += pop();
  if (limit != index) {
    rPush(index);
    rPush(limit);
    ip = (cell_t*)*ip;
  } else {
    ip++;
    if (rPop() != LOOP_SYS) {
      push(-22);
      _throw();
      return;
    }
  }
}

/*******************************************************************************/
/**                          Core Forth Words                                 **/
/*******************************************************************************/
const PROGMEM char store_str[] = "!";
// ( x a-addr --)
// Store x at a-addr
static void _store(void) {
  addr_t address = pop();
  *((cell_t*) address) = pop();
}

const PROGMEM char number_sign_str[] = "#";
// ( ud1 -- ud2)
// Divide ud1 by number in BASE giving quotient ud2 and remainder n. Convert
// n to external form and add the resulting character to the beginning of the
// pictured numeric output string.
static void _number_sign(void) {
  udcell_t ud;
  ud = (udcell_t)pop() << sizeof(ucell_t) * 8;
  ud += (udcell_t)pop();
  *--pPNO = pgm_read_byte(&charset[ud % base]);
  ud /= base;
  push((ucell_t)ud);
  push((ucell_t)(ud >> sizeof(ucell_t) * 8));
}

const PROGMEM char number_sign_gt_str[] = "#>";
// ( xd -- c-addr u)
// Drop xd. Make the pictured numeric output string available as a character
// string c-addr and u specify the resulting string. A program may replace
// characters within the string.
static void _number_sign_gt(void) {
  _two_drop();
  push((size_t)pPNO);
  push((size_t)strlen(pPNO));
  flags &= ~NUM_PROC;
}

const PROGMEM char number_sign_s_str[] = "#s";
// ( ud1 -- ud2)
static void _number_sign_s(void) {
  udcell_t ud;
  ud = (udcell_t)pop() << sizeof(ucell_t) * 8;
  ud += (udcell_t)pop();
  while (ud) {
    *--pPNO = pgm_read_byte(&charset[ud % base]);
    ud /= base;
  }
  push((ucell_t)ud);
  push((ucell_t)(ud >> sizeof(ucell_t) * 8));
}

const PROGMEM char tick_str[] = "'";
// ( "<space>name" -- xt)
// Skip leading space delimiters. Parse name delimited by a space. Find name and
// return xt, the execution token for name. An ambiguous condition exists if
// name is not found. When interpreting "' xyz EXECUTE" is equivalent to xyz.
static void _tick(void) {
  push(' ');
  _word();
  _find();
  pop();
}

const PROGMEM char paren_str[] = "(";
// ( "ccc<paren>" -- )
// imedeate
static void _paren(void) {
  push(')');
  _word();
  _drop();
}

const PROGMEM char star_str[] = "*";
// ( n1|u1 n2|u2 -- n3|u3 )
// multiply n1|u1 by n2|u2 giving the product n3|u3
static void _star(void) {
  push(pop() * pop());
}

const PROGMEM char star_slash_str[] = "*/";
// ( n1 n2 n3 -- n4 )
// multiply n1 by n2 producing the double cell result d. Divide d by n3
// giving the single-cell quotient n4.
static void _star_slash(void) {
  cell_t n3 = pop();
  cell_t n2 = pop();
  cell_t n1 = pop();
  dcell_t d = (dcell_t)n1 * (dcell_t)n2;
  push((cell_t)(d / n3));
}

const PROGMEM char star_slash_mod_str[] = "*/mod";
// ( n1 n2 n3 -- n4 n5)
// multiply n1 by n2 producing the double cell result d. Divide d by n3
// giving the single-cell remainder n4 and quotient n5.
static void _star_slash_mod(void) {
  cell_t n3 = pop();
  cell_t n2 = pop();
  cell_t n1 = pop();
  dcell_t d = (dcell_t)n1 * (dcell_t)n2;
  push((cell_t)(d % n3));
  push((cell_t)(d / n3));
}

const PROGMEM char plus_str[] = "+";
// ( n1|u1 n2|u2 -- n3|u3 )
// add n2|u2 to n1|u1, giving the sum n3|u3
static void _plus(void) {
  cell_t x = pop();
  cell_t y = pop();
  push(x +  y);
}

const PROGMEM char plus_store_str[] = "+!";
// ( n|u a-addr -- )
// add n|u to the single cell number at a-addr
static void _plus_store(void) {
  addr_t address = pop();
  if (address >= (size_t)&forthSpace[0] &&
      address < (size_t)&forthSpace[FORTH_SIZE])
    *((unsigned char*) address) += pop();
  else {
    push(-9);
    _throw();
  }
}

const PROGMEM char plus_loop_str[] = "+loop";
// Interpretation: Interpretation semantics for this word are undefined.
// Compilation: (C: do-sys -- )
// Append the run-time semantics given below to the current definition. Resolve
// the destination of all unresolved occurrences of LEAVE between the location
// given by do-sys and the next location for a transfer of control, to execute
// the words following +LOOP.
// Run-Time: ( n -- )(R: loop-sys1 -- | loop-sys2 )
// An ambiguous condition exists if the loop control parameters are unavailable.
// Add n to the index. If the loop index did not cross the boundary between the
// loop limit minus one and the loop limit, continue execution at the beginning
// of the loop. Otherwise, discard the current loop control parameters and
// continue execution immediately following the loop.
static void _plus_loop(void) {
  *pHere++ = PLUS_LOOP_SYS_IDX;
  *pHere++ = pop();
  cell_t* leave = (cell_t*)pop();
  if (leave != (cell_t*)DO_SYS) {
    if (stack[tos] == DO_SYS) {
      *leave = (size_t)pHere;
      pop();
    } else {
      push(-22);
      _throw();
      return;
    }
  }
}

const PROGMEM char comma_str[] = ",";
// ( x --  )
// Reserve one cell of data space and store x in the cell. If the data-space
// pointer is aligned when , begins execution, it will remain aligned when ,
// finishes execution. An ambiguous condition exists if the data-space pointer
// is not aligned prior to execution of ,.
static void _comma(void) {
  *pHere++ = pop();
}

const PROGMEM char minus_str[] = "-";
// ( n1|u1 n2|u2 -- n3|u3 )
static void _minus(void) {
  cell_t temp = pop();
  push(pop() -  temp);
}

const PROGMEM char dot_str[] = ".";
// ( n -- )
// display n in free field format
static void _dot(void) {
  w = pop();
  displayValue();
}

const PROGMEM char dot_quote_str[] = ".\x22";
// Compilation ("ccc<quote>" -- )
// Parse ccc delimited by ". Append the run time semantics given below to
// the current definition.
// Run-Time ( -- )
// Display ccc.
static void _dot_quote(void) {
  uint8_t i;
  char length;
  if (flags & EXECUTE) {
    ioDirector.printString((char*)ip);
    cell_t len = strlen((char*)ip) + 1;  // include null terminator
    ip = (cell_t*)((size_t)ip + len);
    ALIGN_P(ip);
  }
  else if (state) {
    cDelimiter = '"';
    if (!getToken()) {
      push(-16);
      _throw();
    }
    length = strlen(cTokenBuffer);
    *pHere++ = DOT_QUOTE_IDX;
    char *ptr = (char *) pHere;
    for (uint8_t i = 0; i < length; i++) {
      *ptr++ = cTokenBuffer[i];
    }
    *ptr++ = '\0';    // Terminate String
    pHere = (cell_t *) ptr;
    ALIGN_P(pHere);  // re- align the pHere for any new code
    cDelimiter = ' ';
  }
}

const PROGMEM char slash_str[] = "/";
// ( n1 n2 -- n3 )
// divide n1 by n2 giving a single cell quotient n3
static void _slash(void) {
  cell_t temp = pop();
  if (temp)
    push(pop() /  temp);
  else {
    push(-10);
    _throw();
  }
}

const PROGMEM char slash_mod_str[] = "/mod";
// ( n1 n2 -- n3 n4)
// divide n1 by n2 giving a single cell remainder n3 and quotient n4
static void _slash_mod(void) {
  cell_t n2 = pop();
  cell_t n1 = pop();
  if (n2) {
    push(n1 %  n2);
    push(n1 /  n2);
  } else {
    push(-10);
    _throw();
  }
}

const PROGMEM char zero_less_str[] = "0<";
// ( n -- flag )
// flag is true if and only if n is less than zero.
static void _zero_less(void) {
  if (pop() < 0) push(TRUE);
  else push(FALSE);
}

const PROGMEM char zero_equal_str[] = "0=";
// ( n -- flag )
// flag is true if and only if n is equal to zero.
static void _zero_equal(void) {
  if (pop() == 0) push(TRUE);
  else push(FALSE);
}

const PROGMEM char one_plus_str[] = "1+";
// ( n1|u1 -- n2|u2 )
// add one to n1|u1 giving sum n2|u2.
static void _one_plus(void) {
  push(pop() + 1);
}

const PROGMEM char one_minus_str[] = "1-";
// ( n1|u1 -- n2|u2 )
// subtract one to n1|u1 giving sum n2|u2.
static void _one_minus(void) {
  push(pop() - 1);
}

const PROGMEM char two_store_str[] = "2!";
// ( x1 x2 a-addr --)
// Store the cell pair x1 x2 at a-addr, with x2 at a-addr and x1 at a-addr+1
static void _two_store(void) {
  addr_t address = pop();
  if (address >= (size_t)&forthSpace[0] &&
      address < (size_t)&forthSpace[FORTH_SIZE - 4]) {
    *(cell_t*)address++ = pop();
    *(cell_t*)address = pop();
  } else {
    push(-9);
    _throw();
  }
}

const PROGMEM char two_star_str[] = "2*";
// ( x1 -- x2 )
// x2 is the result of shifting x1 one bit to toward the MSB
static void _two_star(void) {
  push(pop() << 1);
}

const PROGMEM char two_slash_str[] = "2/";
// ( x1 -- x2 )
// x2 is the result of shifting x1 one bit to toward the LSB
static void _two_slash(void) {
  push(pop() >> 1);
}

const PROGMEM char two_fetch_str[] = "2@";  // \x40 == '@'
// ( a-addr -- x1 x2 )
// Fetch cell pair x1 x2 at a-addr. x2 is at a-addr, and x1 is at a-addr+1
static void _two_fetch(void) {
  addr_t address = pop();
  cell_t value = *(cell_t *)address;
  push(value);
  address += sizeof(cell_t);
  value = *(cell_t *)address;
  push(value);
}

const PROGMEM char two_drop_str[] = "2drop";
// ( x1 x2 -- )
static void _two_drop(void) {
  pop();
  pop();
}

const PROGMEM char two_dup_str[] = "2dup";
// ( x1 x2 -- x1 x2 x1 x2 )
static void _two_dup(void) {
  push(stack[tos - 1]);
  push(stack[tos - 1]);
}

const PROGMEM char two_over_str[] = "2over";
// ( x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2 )
static void _two_over(void) {
  push(stack[tos - 3]);
  push(stack[tos - 2]);
}

const PROGMEM char two_swap_str[] = "2swap";
// ( x1 x2 x3 x4 -- x3 x4 x1 x2 )
static void _two_swap(void) {
  cell_t x4 = pop();
  cell_t x3 = pop();
  cell_t x2 = pop();
  cell_t x1 = pop();
  push(x3);
  push(x4);
  push(x1);
  push(x2);
}

const PROGMEM char colon_str[] = ":";
// (C: "<space>name" -- colon-sys )
// Skip leading space delimiters. Parse name delimited by a space. Create a
// definition for name, called a "colon definition" Enter compilation state
// and start the current definition, producing a colon-sys. Append the
// initiation semantics given below to the current definition....
static void _colon(void) {
  state = TRUE;
  push(COLON_SYS);
  openEntry();
}

const PROGMEM char semicolon_str[] = ";";
// IMMEDIATE
// Interpretation: undefined
// Compilation: (C: colon-sys -- )
// Run-time: ( -- ) (R: nest-sys -- )
static void _semicolon(void) {
  if (pop() != COLON_SYS) {
    push(-22);
    _throw();
    return;
  }
  closeEntry();
  state = FALSE;
}

const PROGMEM char lt_str[] = "<";
// ( n1 n2 -- flag )
static void _lt(void) {
  if (pop() > pop()) push(TRUE);
  else push(FALSE);
}

const PROGMEM char lt_number_sign_str[] = "<#";
// ( -- )
// Initialize the pictured numeric output conversion process.
static void _lt_number_sign(void) {
  pPNO = (char*)pHere + HOLD_SIZE + 1;
  *pPNO = '\0';
  //  *pPNO = NULL;
  flags |= NUM_PROC;
}

const PROGMEM char eq_str[] = "=";
// ( x1 x2 -- flag )
// flag is true if and only if x1 is bit for bit the same as x2
static void _eq(void) {
  if (pop() == pop()) push(TRUE);
  else push(FALSE);
}

const PROGMEM char gt_str[] = ">";
// ( n1 n2 -- flag )
// flag is true if and only if n1 is greater than n2
static void _gt(void) {
  if (pop() < pop()) push(TRUE);
  else push(FALSE);
}

const PROGMEM char to_body_str[] = ">body";
// ( xt -- a-addr )
// a-addr is the data-field address corresponding to xt. An ambiguous condition
// exists if xt is not for a word defined by CREATE.
static void _to_body(void) {
  cell_t* xt = (cell_t*)pop();
  if ((size_t)xt > 0xFF) {
    if (*xt++ == LITERAL_IDX) {
      push(*xt);
      return;
    }
  }
  push(-31);
  _throw();
}

const PROGMEM char to_in_str[] = ">in";
// ( -- a-addr )
static void _to_in(void) {
  push((size_t)&cpToIn);
}

const PROGMEM char to_number_str[] = ">number";
// ( ud1 c-addr1 u1 -- ud2 c-addr u2 )
static void _to_number(void) {
  print_P(not_done_str);
}

const PROGMEM char to_r_str[] = ">r";
// ( x -- ) (R: -- x )
static void _to_r(void) {
  rPush(pop());
}

const PROGMEM char question_dup_str[] = "?dup";
// ( x -- 0 | x x )
static void _question_dup(void) {
  if (stack[tos]) {
    push(stack[tos]);
  } else {
    pop();
    push(0);
  }
}

const PROGMEM char fetch_str[] = "@";
// ( a-addr -- x1 )
// Fetch cell x1 at a-addr.
static void _fetch(void) {
  addr_t address = pop();
  cell_t value = *(cell_t*)address;
  push(value);
}


const PROGMEM char abort_str[] = "abort";
// (i*x -- ) (R: j*x -- )
// Empty the data stack and preform the function of QUIT, which includes emptying
// the return stack, without displaying a message.
static void _abort(void) {
  push(-1);
  _throw();
}

const PROGMEM char abort_quote_str[] = "abort\x22";
// Interpretation: Interpretation semantics for this word are undefined.
// Compilation: ( "ccc<quote>" -- )
// Parse ccc delimited by a ". Append the run-time semantics given below to the
// current definition.
// Runt-Time: (i*x x1 -- | i*x ) (R: j*x -- |j*x )
// Remove x1 from the stack. If any bit of x1 is not zero, display ccc and
// preform an implementation-defined abort sequence that included the function
// of ABORT.
static void _abort_quote(void) {
  *pHere++ = ZJUMP_IDX;
  push((size_t)pHere);  // Push the address for our origin
  *pHere++ = 0;
  _dot_quote();
  *pHere++ = LITERAL_IDX;
  *pHere++ = -2;
  *pHere++ = THROW_IDX;
  cell_t* orig = (cell_t*)pop();
  *orig = (size_t)pHere - (size_t)orig;
}

const PROGMEM char abs_str[] = "abs";
// ( n -- u)
// Runt-Time:
static void _abs(void) {
  cell_t n = pop();
  push(n < 0 ? 0 - n : n);
}

const PROGMEM char accept_str[] = "accept";
// ( c-addr +n1 -- +n2 )
static void _accept(void) {
  cell_t length = pop();
  char* addr = (char*)pop();
  length = getLine(addr, length);
  push(length);
}

const PROGMEM char align_str[] = "align";
// ( -- )
// if the data-space pointer is not aligned, reserve enough space to align it.
static void _align(void) {
  ALIGN_P(pHere);
}

const PROGMEM char aligned_str[] = "aligned";
// ( addr -- a-addr)
static void _aligned(void) {
  push((pop() + 3) & -4);
}

const PROGMEM char allot_str[] = "allot";
// ( n -- )
// if n is greater than zero, reserve n address units of data space. if n is less
// than zero, release |n| address units of data space. If n is zero, leave the
// data-space pointer unchanged.
static void _allot(void) {
  cell_t *pNewHere = pHere + pop();

  // Check that the new pHere is not outside of the forth space
  if (pNewHere >= &forthSpace[0] &&
      pNewHere < &forthSpace[FORTH_SIZE]) {
    pHere = pNewHere;      // Save the valid address
  } else {                 // Throw an exception
    push(-9);
    _throw();
  }
}

const PROGMEM char and_str[] = "and";
// ( x1 x2 -- x3 )
// x3 is the bit by bit logical and of x1 with x2
static void _and(void) {
  push(pop() & pop());
}

const PROGMEM char base_str[] = "base";
// ( -- a-addr)
static void _base(void) {
  push((size_t)&base);
}

const PROGMEM char begin_str[] = "begin";
// Interpretation: Interpretation semantics for this word are undefined.
// Compilation: (C: -- dest )
// Put the next location for a transfer of control, dest, onto the control flow
// stack. Append the run-time semantics given below to the current definition.
// Run-time: ( -- )
// Continue execution.
static void _begin(void) {
  push((size_t)pHere);
  *pHere = 0;
}

const PROGMEM char bl_str[] = "bl";
// ( -- char )
// char is the character value for a space.
static void _bl(void) {
  push(' ');
}

const PROGMEM char c_store_str[] = "c!";
// ( char c-addr -- )
static void _c_store(void) {
  uint8_t *addr = (uint8_t*) pop();
  *addr = (uint8_t)pop();
}

const PROGMEM char c_comma_str[] = "c,";
// ( char -- )
static void _c_comma(void) {
  *(char*)pHere++ = (char)pop();
}

const PROGMEM char c_fetch_str[] = "c@";
// ( c-addr -- char )
static void _c_fetch(void) {
  uint8_t *addr = (uint8_t *) pop();
  push(*addr);
}

const PROGMEM char cell_plus_str[] = "cell+";
// ( a-addr1 -- a-addr2 )
static void _cell_plus(void) {
  push((addr_t)(pop() + sizeof(cell_t)));
}

const PROGMEM char cells_str[] = "cells";
// ( n1 -- n2 )
// n2 is the size in address units of n1 cells.
static void _cells(void) {
  push(pop() * sizeof(cell_t));
}

const PROGMEM char char_str[] = "char";
// ( "<spaces>name" -- char )
// Skip leading space delimiters. Parse name delimited by a space. Put the value
// of its first character onto the stack.
static void _char(void) {
  if (getToken()) push(cTokenBuffer[0]);
  else {
    push(-16);
    _throw();
  }
}

const PROGMEM char char_plus_str[] = "char+";
// ( c-addr1 -- c-addr2 )
static void _char_plus(void) {
  push(pop() + 1);
}

const PROGMEM char chars_str[] = "chars";
// ( n1 -- n2 )
// n2 is the size in address units of n1 characters.
static void _chars(void) {
}

const PROGMEM char constant_str[] = "constant";
// ( x"<spaces>name" --  )
static void _constant(void) {
  openEntry();
  *pHere++ = LITERAL_IDX;
  *pHere++ = pop();
  closeEntry();
}

const PROGMEM char count_str[] = "count";
// ( c-addr1 -- c-addr2 u )
static void _count(void) {
  addr_t addr = pop();
  push(addr + sizeof(cell_t));
  push(*((cell_t *) addr));
}

const PROGMEM char cr_str[] = "cr";
// ( -- )
// Carriage Return
static void _cr(void) {
  ioDirector.printString("\n");
}

const PROGMEM char create_str[] = "create";
// ( "<spaces>name" -- )
// Skip leading space delimiters. Parse name delimited by a space. Create a
// definition for name with the execution semantics defined below. If the data-space
// pointer is not aligned, reserve enough data space to align it. The new data-space
// pointer defines name's data field. CREATE does not allocate data space in name's
// data field.
// name EXECUTION: ( -- a-addr )
// a-addr is the address of name's data field. The execution semantics of name may
// be extended by using DOES>.
static void _create(void) {
  openEntry();
  *pHere++ = LITERAL_IDX;
  // Location of Data Field at the end of the definition.
  *pHere++ = (size_t)pHere + 2 * sizeof(cell_t);
  *pHere = EXIT_IDX;   // Store an extra exit reference so
  // that it can be replace by a
  // subroutine pointer created by DOES>
  pDoes = pHere;       // Save this location for uses by subroutine.
  pHere += 1;
  if (!state) closeEntry();           // Close the entry if interpreting
}

const PROGMEM char decimal_str[] = "decimal";
// ( -- )
// Set BASE to 10
static void _decimal(void) { // value --
  base = 10;
}

const PROGMEM char depth_str[] = "depth";
// ( -- +n )
// +n is the number of single cells on the stack before +n was placed on it.
static void _depth(void) {
  push(tos + 1);
}

const PROGMEM char do_str[] = "do";
// Compilation: (C: -- do-sys)
// Run-Time: ( n1|u1 n2|u2 -- ) (R: -- loop-sys )
static void _do(void) {
  push(DO_SYS);
  *pHere++ = DO_SYS_IDX;
  push((size_t)pHere); // store the origin address of the do loop
}

const PROGMEM char does_str[] = "does>";
// Compilation: (C: colon-sys1 -- colon-sys2)
// Run-Time: ( -- ) (R: nest-sys1 -- )
// Initiation: ( i*x -- i*x a-addr ) (R: -- next-sys2 )
static void _does(void) {
  *pHere++ = SUBROUTINE_IDX;
  // Store location for a subroutine call
  *pHere++ = (size_t)pHere + sizeof(cell_t);
  *pHere++ = EXIT_IDX;
  // Start Subroutine coding
}

const PROGMEM char drop_str[] = "drop";
// ( x -- )
// Remove x from stack
static void _drop(void) {
  pop();
}

const PROGMEM char dupe_str[] = "dup";
// ( x -- x x )
// Duplicate x
static void _dupe(void) {
  push(stack[tos]);
}

const PROGMEM char else_str[] = "else";
// Interpretation: Undefine
// Compilation: (C: orig1 -- orig2)
// Run-Time: ( -- )
static void _else(void) {
  cell_t* orig = (cell_t*)pop();
  *pHere++ = JUMP_IDX;
  push((size_t)pHere++);
  *orig = (size_t)pHere - (size_t)orig;
}

const PROGMEM char emit_str[] = "emit";
// ( x -- )
// display x as a character
static void _emit(void) {
  ioDirector.write((char) pop());
}

const PROGMEM char environment_str[] = "environment?";
// ( c-addr u  -- false|i*x true )
// c-addr is the address of a character string and u is the string's character
// count. u may have a value in the range from zero to an implementation-defined
// maximum which shall not be less than 31. The character string should contain
// a keyword from 3.2.6 Environmental queries or the optional word sets to to be
// checked for correspondence with  an attribute of the present environment.
// If the system treats the attribute as unknown, the return flag is false;
// otherwise, the flag is true and i*x returned is the of the type specified in
// the table for the attribute queried.
static void _environment(void) {
  char length = (char)pop();
  char* pStr = (char*)pop();
  if (length && length < STRING_SIZE) {
    if (!strcmp_P(pStr, PSTR("/counted-string"))) {
      push(STRING_SIZE);
      return;
    }
    if (!strcmp_P(pStr, PSTR("/hold"))) {
      push(HOLD_SIZE);
      return;
    }
    if (!strcmp_P(pStr, PSTR("address-unit-bits"))) {
      push(ADDRESS_BITS);
      return;
    }
    if (!strcmp_P(pStr, PSTR("core"))) {
      push(FALSE);
      return;
    }
    if (!strcmp_P(pStr, PSTR("core-ext"))) {
      push(FALSE);
      return;
    }
    if (!strcmp_P(pStr, PSTR("floored"))) {
      push(FLOORED);
      return;
    }
    if (!strcmp_P(pStr, PSTR("max-char"))) {
      push(MAX_CHAR);
      return;
    }
    if (!strcmp_P(pStr, PSTR("max-d"))) {
      push(MAX_D);
      return;
    }
    if (!strcmp_P(pStr, PSTR("max-n"))) {
      push(MAX_N);
      return;
    }
    if (!strcmp_P(pStr, PSTR("max-u"))) {
      push(MAX_U);
      return;
    }
    if (!strcmp_P(pStr, PSTR("max-ud"))) {
      push(MAX_UD);
      return;
    }
    if (!strcmp_P(pStr, PSTR("return-stack-size"))) {
      push(RSTACK_SIZE);
      return;
    }
    if (!strcmp_P(pStr, PSTR("stack-size"))) {
      push(STACK_SIZE);
      return;
    }
  }
  push(-13);
  _throw();
}

const PROGMEM char evaluate_str[] = "evaluate";
// ( i*x c-addr u  -- j*x )
// Save the current input source specification. Store minus-one (-1) in SOURCE-ID
// if it is present. Make the string described by c-addr and u both the input
// source and input buffer, set >IN to zero, and interpret. When the parse area
// is empty, restore the prior source specification. Other stack effects are due
// to the words EVALUATEd.
static void _evaluate(void) {
  char* tempSource = cpSource;
  char* tempSourceEnd = cpSourceEnd;
  char* tempToIn = cpToIn;

  uint8_t length = pop();
  cpSource = (char*)pop();
  cpSourceEnd = cpSource + length;
  cpToIn = cpSource;
  interpreter();
  cpSource = tempSource;
  cpSourceEnd = tempSourceEnd;
  cpToIn = tempToIn;
}

const PROGMEM char execute_str[] = "execute";
// ( i*x xt -- j*x )
// Remove xt from the stack and preform the semantics identified by it. Other
// stack effects are due to the word EXECUTEd
static void _execute(void) {
  func function;
  w = pop();
  if (w > 255) {
    // rpush(0);
    rPush((cell_t) ip);        // CAL - Push our return address
    ip = (cell_t *)w;          // set the ip to the XT (memory location)
    executeWord();
  } else {
    function = (func) pgm_read_dword(&(flashDict[w - 1].function));
    function();
    if (errorCode) return;
  }
}

const PROGMEM char exit_str[] = "exit";
// Interpretation: undefined
// Execution: ( -- ) (R: nest-sys -- )
// Return control to the calling definition specified by nest-sys. Before
// executing EXIT within a do-loop, a program shall discard the loop-control
// parameters by executing UNLOOP.
static void _exit(void) {
  ip = (cell_t*)rPop();
}

const PROGMEM char fill_str[] = "fill";
// ( c-addr u char -- )
// if u is greater than zero, store char in u consecutive characters of memory
// beginning with c-addr.
static void _fill(void) {
  char ch = (char)pop();
  cell_t limit = pop();
  char* addr = (char*)pop();
  for (int i = 1; i < limit; i++) {
    *addr++ = ch;
  }
}

const PROGMEM char find_str[] = "find";
// ( c-addr -- c-addr 0 | xt 1 | xt -1)
// Find the definition named in the counted string at c-addr. If the definition
// is not found, return c-addr and zero. If the definition is found, return its
// execution token xt. If the definition is immediate, also return one (1),
// otherwise also return minus-one (-1).
static void _find(void) {
  uint8_t index = 0;

  cell_t *addr = (cell_t *) pop();
  cell_t length = *addr++;

  char *ptr = (char*) addr;
  if (length = 0) {
    push(-16);
    _throw();
    return;
  } else if (length > STRING_SIZE) {
    push(-18);
    _throw();
    return;
  }

  pUserEntry = pLastUserEntry;
  // First search through the user dictionary
  while (pUserEntry) {
    if (strcmp(pUserEntry->name, ptr) == 0) {
      length = strlen(pUserEntry->name);
      push(pUserEntry->cfa);
      wordFlags = pUserEntry->flags;
      if (wordFlags & IMMEDIATE) push(1);
      else push(-1);
      return;
    }
    pUserEntry = (userEntry_t*)pUserEntry->prevEntry;
  }
  // Second Search through the flash Dictionary
  while (pgm_read_dword(&(flashDict[index].name))) {
    if (!strcasecmp_P(ptr, (char*) pgm_read_dword(&(flashDict[index].name)))) {
      push(index + 1);
      wordFlags = pgm_read_byte(&(flashDict[index].flags));
      if (wordFlags & IMMEDIATE) push(1);
      else push(-1);
      return;
    }
    index++;
  }
  push((size_t)ptr);
  push(0);
}

const PROGMEM char fm_slash_mod_str[] = "fm/mod";
// ( d1 n1 -- n2 n3 )
// Divide d1 by n1, giving the floored quotient n3 and remainder n2.
static void _fm_slash_mod(void) {
  cell_t n1 = pop();
  cell_t d1 = pop();
  push(d1 /  n1);
  push(d1 %  n1);
}

const PROGMEM char here_str[] = "here";
// ( -- addr )
// addr is the data-space pointer.
static void _here(void) {
  push((size_t)pHere);
}

const PROGMEM char hold_str[] = "hold";
// ( char -- )
// add char to the beginning of the pictured numeric output string.
static void _hold(void) {
  if (flags & NUM_PROC) {
    *--pPNO = (char) pop();
  }
}

const PROGMEM char i_str[] = "i";
// Interpretation: undefined
// Execution: ( -- n|u ) (R: loop-sys -- loop-sys )
static void _i(void) {
  push(rStack[rtos - 1]);
}

const PROGMEM char if_str[] = "if";
// Compilation: (C: -- orig )
// Run-Time: ( x -- )
static void _if(void) {
  *pHere++ = ZJUMP_IDX;
  *pHere = 0;
  push((size_t)pHere++);
}

const PROGMEM char immediate_str[] = "immediate";
// ( -- )
// make the most recent definition an immediate word.
static void _immediate(void) {
  if (pLastUserEntry) {
    pLastUserEntry->flags |= IMMEDIATE;
  }
}

const PROGMEM char invert_str[] = "invert";
// ( x1 -- x2 )
// invert all bits in x1, giving its logical inverse x2
static void _invert(void)   {
  push(~pop());
}

const PROGMEM char j_str[] = "j";
// Interpretation: undefined
// Execution: ( -- n|u ) (R: loop-sys1 loop-sys2 -- loop-sys1 loop-sys2 )
// n|u is a copy of the next-outer loop index. An ambiguous condition exists
// if the loop control parameters of the next-outer loop, loop-sys1, are
// unavailable.
static void _j(void) {
  push(rStack[rtos - 4]);
}

const PROGMEM char key_str[] = "key";
// ( -- char )
static void _key(void) {
  push(getKey());
}

const PROGMEM char leave_str[] = "leave";
// Interpretation: undefined
// Execution: ( -- ) (R: loop-sys -- )
static void _leave(void) {
  *pHere++ = LEAVE_SYS_IDX;
  push((size_t)pHere);
  *pHere++ = 0;
  _swap();
}

const PROGMEM char literal_str[] = "literal";
// Interpretation: undefined
// Compilation: ( x -- )
// Run-Time: ( -- x )
// Place x on the stack
static void _literal(void) {
  if (state) {
    *pHere++ = LITERAL_IDX;
    *pHere++ = pop();
  } else {
    push(*ip++);
  }
}

const PROGMEM char loop_str[] = "loop";
// Interpretation: undefined
// Compilation: (C: do-sys -- )
// Run-Time: ( -- ) (R: loop-sys1 -- loop-sys2 )
static void _loop(void) {
  *pHere++ = LOOP_SYS_IDX;
  *pHere++ = pop();
  cell_t* leave = (cell_t*)pop();
  if (leave != (cell_t*)DO_SYS) {
    if (stack[tos] == DO_SYS) {
      *leave = (size_t)pHere;
      pop();
    } else {
      push(-22);
      _throw();
      return;
    }
  }
}

const PROGMEM char lshift_str[] = "lshift";
// ( x1 u -- x2 )
// x2 is x1 shifted to left by u positions.
static void _lshift(void) {
  cell_t u = pop();
  cell_t x1 = pop();
  push(x1 << u);
}

const PROGMEM char max_str[] = "max";
// ( n1 n2 -- n3 )
// n3 is the greater of of n1 or n2.
static void _ymax(void) {
  cell_t n2 = pop();
  cell_t n1 = pop();
  if (n1 > n2) push(n1);
  else push(n2);
}

const PROGMEM char min_str[] = "min";
// ( n1 n2 -- n3 )
// n3 is the lesser of of n1 or n2.
static void _ymin(void) {
  cell_t n2 = pop();
  cell_t n1 = pop();
  if (n1 > n2) push(n2);
  else push(n1);
}

const PROGMEM char mod_str[] = "mod";
// ( n1 n2 -- n3 )
// Divide n1 by n2 giving the remainder n3.
static void _mod(void) {
  cell_t temp = pop();
  push(pop() %  temp);
}

const PROGMEM char move_str[] = "move";
// ( addr1 addr2 u -- )
// if u is greater than zero, copy the contents of u consecutive address
// starting at addr1 to u consecutive address starting at addr2.
static void _move(void) {
  cell_t u = pop();
  addr_t *to = (addr_t*)pop();
  addr_t *from = (addr_t*)pop();
  for (cell_t i = 0; i < u; i++) {
    *to++ = *from++;
  }
}

const PROGMEM char negate_str[] = "negate";
// ( n1 -- n2 )
// Negate n1, giving its arithmetic inverse n2.
static void _negate(void) {
  push(!pop());
}

const PROGMEM char or_str[] = "or";
// ( x1 x2 -- x3 )
// x3 is the bit by bit logical or of x1 with x2
static void _or(void) {
  push(pop() |  pop());
}

const PROGMEM char over_str[] = "over";
// ( x y -- x y x )
static void _over(void) {
  push(stack[tos - 1]);
}

const PROGMEM char postpone_str[] = "postpone";
// Compilation: ( "<spaces>name" -- )
// Skip leading space delimiters. Parse name delimited by a space. Find name.
// Append the compilation semantics of name to the current definition. An
// ambiguous condition exists if name is not found.
static void _postpone(void) {
  func function;
  if (!getToken()) {
    push(-16);
    _throw();
  }
  if (isWord(cTokenBuffer)) {
    if (wordFlags & COMP_ONLY) {
      if (w > 255) {
        rPush(0);            // Push 0 as our return address
        ip = (cell_t *)w;          // set the ip to the XT (memory location)
        executeWord();
      } else {
        function = (func) pgm_read_dword(&(flashDict[w - 1].function));
        function();
        if (errorCode) return;
      }
    } else {
      *pHere++ = (cell_t)w;
    }
  } else {
    push(-13);
    _throw();
    return;
  }
}

const PROGMEM char quit_str[] = "quit";
// ( -- ) (R: i*x -- )
// Empty the return stack, store zero in SOURCE-ID if it is present,
// make the user input device the input source, enter interpretation state.
static void _quit(void) {
  rtos = -1;
  *cpToIn = 0;          // Terminate buffer to stop interpreting
  Serial.flush();
}

const PROGMEM char r_from_str[] = "r>";
// Interpretation: undefined
// Execution: ( -- x ) (R: x -- )
// move x from the return stack to the data stack.
static void _r_from(void) {
  push(rPop());
}

const PROGMEM char r_fetch_str[] = "r@";
// Interpretation: undefined
// Execution: ( -- x ) (R: x -- x)
// Copy x from the return stack to the data stack.
static void _r_fetch(void) {
  push(rStack[rtos]);
}

const PROGMEM char recurse_str[] = "recurse";
// Interpretation: Interpretation semantics for this word are undefined
// Compilation: ( -- )
// Append the execution semantics of the current definition to the current
// definition. An ambiguous condition exists if RECURSE appends in a definition
// after DOES>.
static void _recurse(void) {
  *pHere++ = (size_t)pCodeStart;
}

const PROGMEM char repeat_str[] = "repeat";
// Interpretation: undefined
// Compilation: (C: orig dest -- )
// Run-Time ( -- )
// Continue execution at the location given.
static void _repeat(void) {
  cell_t dest;
  cell_t* orig;
  *pHere++ = JUMP_IDX;
  *pHere++ = pop() - (size_t)pHere;
  orig = (cell_t*)pop();
  *orig = (size_t)pHere - (size_t)orig;
}

const PROGMEM char rot_str[] = "rot";
// ( x1 x2 x3 -- x2 x3 x1)
static void _rot(void) {
  cell_t x3 = pop();
  cell_t x2 = pop();
  cell_t x1 = pop();
  push(x2);
  push(x3);
  push(x1);
}

const PROGMEM char rshift_str[] = "rshift";
// ( x1 u -- x2 )
// x2 is x1 shifted to right by u positions.
static void _rshift(void) {
  cell_t u = pop();
  cell_t x1 = pop();
  push(x1 >> u);
}

const PROGMEM char s_quote_str[] = "s\x22";
// Interpretation: Interpretation semantics for this word are undefined.
// Compilation: ("ccc<quote>" -- )
// Parse ccc delimited by ". Append the run-time semantics given below to the
// current definition.
// Run-Time: ( -- c-addr u )
// Return c-addr and u describing a string consisting of the characters ccc. A program
// shall not alter the returned string.
static void _s_quote(void) {
  uint8_t i;
  char length;
  if (flags & EXECUTE) {
    push((size_t)ip);
    cell_t len = strlen((char*)ip);
    push(len++);    // increment for the null terminator
    ALIGN(len);
    ip = (cell_t*)((size_t)ip + len);
  }
  else if (state) {
    cDelimiter = '"';
    if (!getToken()) {
      push(-16);
      _throw();
    }
    length = strlen(cTokenBuffer);
    *pHere++ = S_QUOTE_IDX;
    char *ptr = (char *) pHere;
    for (uint8_t i = 0; i < length; i++) {
      *ptr++ = cTokenBuffer[i];
    }
    *ptr++ = '\0';    // Terminate String
    pHere = (cell_t *) ptr;
    ALIGN_P(pHere);  // re- align pHere for any new code
    cDelimiter = ' ';
  }
}

const PROGMEM char s_to_d_str[] = "s>d";
// ( n -- d )
static void _s_to_d(void) {
  cell_t n = pop();
  push(0);
  push(n);
}

const PROGMEM char sign_str[] = "sign";
// ( n -- )
static void _sign(void) {
  if (flags & NUM_PROC) {
    cell_t sign = pop();
    if (sign < 0) *--pPNO = '-';
  }
}

const PROGMEM char sm_slash_rem_str[] = "sm/rem";
// ( d1 n1 -- n2 n3 )
// Divide d1 by n1, giving the symmetric quotient n3 and remainder n2.
static void _sm_slash_rem(void) {
  cell_t n1 = pop();
  cell_t d1 = pop();
  push(d1 /  n1);
  push(d1 %  n1);
}

const PROGMEM char source_str[] = "source";
// ( -- c-addr u )
// c-addr is the address of, and u is the number of characters in, the input buffer.
static void _source(void) {
  push((size_t)&cInputBuffer);
  push(strlen(cInputBuffer));
}

const PROGMEM char space_str[] = "space";
// ( -- )
// Display one space
static void _space(void) {
  print_P(sp_str);
}

const PROGMEM char spaces_str[] = "spaces";
// ( n -- )
// if n is greater than zero, display n space
static void _spaces(void) {
  char n = (char) pop();
  while (n > 0) {
    print_P(sp_str);
  }
}

const PROGMEM char state_str[] = "state";
// ( -- a-addr )
// a-addr is the address of the cell containing compilation state flag.
static void _state(void) {
  push((size_t)&state);
}

const PROGMEM char swap_str[] = "swap";
static void _swap(void) { // x y -- y x
  cell_t x, y;

  y = pop();
  x = pop();
  push(y);
  push(x);
}

const PROGMEM char then_str[] = "then";
// Interpretation: Undefine
// Compilation: (C: orig -- )
// Run-Time: ( -- )
static void _then(void) {
  cell_t* orig = (cell_t*)pop();
  *orig = (size_t)pHere - (size_t)orig;
}

const PROGMEM char type_str[] = "type";
// ( c-addr u -- )
// if u is greater than zero display character string specified by c-addr and u
static void _type(void) {
  uint8_t length = (uint8_t)pop();
  char* addr = (char*)pop();
  for (char i = 0; i < length; i++)
    ioDirector.write(*addr++);
}

const PROGMEM char u_dot_str[] = "u.";
// ( u -- )
// Displau u in free field format
static void _u_dot(void) {
  ioDirector.printUInt((ucell_t) pop());
}

const PROGMEM char u_lt_str[] = "u<";
// ( u1 u2 -- flag )
// flag is true if and only if u1 is less than u2.
static void _u_lt(void) {
  if ((ucell_t)pop() > ucell_t(pop())) push(TRUE);
  else push(FALSE);
}

const PROGMEM char um_star_str[] = "um*";
// ( u1 u2 -- ud )
// multiply u1 by u2, giving the unsigned double-cell product ud
static void _um_star(void) {
  udcell_t ud = pop() * pop();
  cell_t lsb = (ucell_t)ud;
  cell_t msb = (ucell_t)(ud >> sizeof(ucell_t) * 8);
  push(msb);
  push(lsb);
}

const PROGMEM char um_slash_mod_str[] = "um/mod";
// ( ud u1 -- u2 u3 )
// Divide ud by u1 giving quotient u3 and remainder u2.
static void _um_slash_mod(void) {
  ucell_t u1 = pop();
  udcell_t lsb = pop();
  udcell_t msb = pop();
  udcell_t ud = (msb << 16) + (lsb);
  push(ud % u1);
  push(ud / u1);
}

const PROGMEM char unloop_str[] = "unloop";
// Interpretation: Undefine
// Execution: ( -- )(R: loop-sys -- )
static void _unloop(void) {
  print_P(not_done_str);
  rPop();
  rPop();
  if (rPop() != LOOP_SYS) {
    push(-22);
    _throw();
  }
}

const PROGMEM char until_str[] = "until";
// Interpretation: Undefine
// Compilation: (C: dest -- )
// Run-Time: ( x -- )
static void _until(void) {
  *pHere++ = ZJUMP_IDX;
  *pHere = pop() - (size_t)pHere;
  pHere += 1;
}

const PROGMEM char variable_str[] = "variable";
// ( "<spaces>name" -- )
// Parse name delimited by a space. Create a definition for name with the
// execution semantics defined below. Reserve one cell of data space at an
// aligned address.
// name Execution: ( -- a-addr )
// a-addr is the address of the reserved cell. A program is responsible for
// initializing the contents of a reserved cell.
static void _variable(void) {
  if (flags & EXECUTE) {
    push((size_t)ip++);
  } else {
    openEntry();
    *pHere++ = VARIABLE_IDX;
    *pHere++ = 0;
    closeEntry();
  }
}

const PROGMEM char while_str[] = "while";
// Interpretation: Undefine
// Compilation: (C: dest -- orig dest )
// Run-Time: ( x -- )
static void _while(void) {
  ucell_t dest;
  ucell_t orig;
  dest = pop();
  *pHere++ = ZJUMP_IDX;
  orig = (size_t)pHere;
  *pHere++ = 0;
  push(orig);
  push(dest);
}

const PROGMEM char word_str[] = "word";
// ( char "<chars>ccc<chars>" -- c-addr )
// Skip leading delimiters. Parse characters ccc delimited by char. An ambiguous
// condition exists if the length of the parsed string is greater than the
// implementation-defined length of a counted string.
//
// c-addr is the address of a transient region containing the parsed word as a
// counted string. If the parse area was empty or contained no characters other than
// the delimiter, the resulting string has a zero length. A space, not included in
// the length, follows the string. A program may replace characters within the
// string.
//
// NOTE: The requirement to follow the string with a space is obsolescent and is
// included as a concession to existing programs that use CONVERT. A program shall
// not depend on the existence of the space.
static void _word(void) {
  uint8_t *start, *ptr;

  cDelimiter = (char)pop();
  start = (uint8_t *) pHere++;
  ptr = (uint8_t *) pHere;
  while (cpToIn <= cpSourceEnd) {
    if (*cpToIn == cDelimiter || *cpToIn == 0) {
      *((cell_t *)start) = (ptr - start) - sizeof(cell_t); // write the length
      pHere = (cell_t *) start;                      // reset pHere (transient memory)
      push((size_t)start);                // push the c-addr onto the stack
      cpToIn++;
      break;
    } else *ptr++ = *cpToIn++;
  }
  cDelimiter = ' ';
}

const PROGMEM char xor_str[] = "xor";
// ( x1 x2 -- x3 )
// x3 is the bit by bit exclusive or of x1 with x2
static void _xor(void) {
  push(pop() ^  pop());
}

const PROGMEM char left_bracket_str[] = "[";
// Interpretation: undefined
// Compilation: Preform the execution semantics given below
// Execution: ( -- )
// Enter interpretation state. [ is an immediate word.
static void _left_bracket(void) {
  state = FALSE;
}

const PROGMEM char bracket_tick_str[] = "[']";
// Interpretation: Interpretation semantics for this word are undefined.
// Compilation: ( "<space>name" -- )
// Skip leading space delimiters. Parse name delimited by a space. Find name.
// Append the run-time semantics given below to the current definition.
// An ambiguous condition exist if name is not found.
// Run-Time: ( -- xt )
// Place name's execution token xt on the stack. The execution token returned
// by the compiled phrase "['] X" is the same value returned by "' X" outside
// of compilation state.
static void _bracket_tick(void) {
  if (!getToken()) {
    push(-16);
    _throw();
  }
  if (isWord(cTokenBuffer)) {
    *pHere++ = LITERAL_IDX;
    *pHere++ = w;
  } else {
    push(-13);
    _throw();
    return;
  }
}

const PROGMEM char bracket_char_str[] = "[char]";
// Interpretation: Interpretation semantics for this word are undefined.
// Compilation: ( "<space>name" -- )
// Skip leading spaces delimiters. Parse name delimited by a space. Append
// the run-time semantics given below to the current definition.
// Run-Time: ( -- char )
// Place char, the value of the first character of name, on the stack.
static void _bracket_char(void) {
  if (getToken()) {
    *pHere++ = LITERAL_IDX;
    *pHere++ = cTokenBuffer[0];
  } else {
    push(-16);
    _throw();
  }
}

const PROGMEM char right_bracket_str[] = "]";
// ( -- )
// Enter compilation state.
static void _right_bracket(void) {
  state = TRUE;
}

/*******************************************************************************/
/**                          Core Extension Set                               **/
/*******************************************************************************/
#ifdef CORE_EXT_SET
const PROGMEM char neq_str[] = "<>";
static void _neq(void) {
  push(pop() != pop());
}
const PROGMEM char hex_str[] = "hex";
// ( -- )
// Set BASE to 16
static void _hex(void) { // value --
  base = 16;
}
#endif

/*******************************************************************************/
/**                            Double Cell Set                                **/
/*******************************************************************************/
#ifdef DOUBLE_SET
#endif

/*******************************************************************************/
/**                             Exception Set                                 **/
/*******************************************************************************/
#ifdef EXCEPTION_SET
const PROGMEM char throw_str[] = "throw";
// ( k*x n -- k*x | i*x n)
// if any bit of n are non-zero, pop the topmost exception frame from the
// exception stack, along with everything on the return stack above that frame.
// ...
static void _throw(void) {
  errorCode = pop();
  uint8_t index = 0;
  int tableCode;
  _cr();
  ioDirector.printString(cTokenBuffer);
  print_P(PSTR(" EXCEPTION("));
  do {
    tableCode = pgm_read_dword(&(exception[index].code));
    if (errorCode == tableCode) {
      ioDirector.printInt((int)errorCode);
      print_P(PSTR("): "));
      print_P((char*) pgm_read_dword(&exception[index].name));
      _cr();
    }
    index++;
  } while (tableCode);
  tos = -1;                       // Clear the stack.
  _quit();
  state = FALSE;

#ifdef HAS_SD_CARD
  // Clean up any IODirector file activity
  ioDirector.fileHousekeeping();
#endif
}
#endif

/*******************************************************************************/
/**                              Local Set                                    **/
/*******************************************************************************/
#ifdef LOCAL_SET
#endif

/*******************************************************************************/
/**                              Memory Set                                   **/
/*******************************************************************************/
#ifdef MEMORY_SET
#endif

/*******************************************************************************/
/**                          Programming Tools Set                            **/
/*******************************************************************************/
#ifdef TOOLS_SET
const PROGMEM char dot_s_str[] = ".s";
static void _dot_s(void) {
  char i;
  char depth = tos + 1;
  if (tos >= 0) {
    for (i = 0; i < depth ; i++) {
      w = stack[i];
      displayValue();
    }
  }
}

const PROGMEM char dump_str[] = "dump";
// ( addr u -- )
// Display the contents of u consecutive address starting at addr. The format of
// the display is implementation dependent.
// DUMP may be implemented using pictured numeric output words. Consequently,
// its use may corrupt the transient region identified by #>.
static void _dump(void) {
  uint8_t len = (uint8_t) pop();
  addr_t addr_start = (addr_t) pop();
  addr_t addr_end = addr_start + len;

  volatile uint8_t* addr = (uint8_t*) addr_start;

  while (addr < (uint8_t*)addr_end) {
    print_P(PSTR("\r\n$"));
    if (addr < (uint8_t*)0x10) print_P(zero_str);
    if (addr < (uint8_t*)0x100) print_P(zero_str);
    ioDirector.printInt((size_t)addr, HEX);
    ioDirector.printString(" - ");
    for (uint8_t i = 0; i < 16; i++) {
      if (*addr < 0x10) print_P(zero_str);
      ioDirector.printInt(*addr++, HEX);
      print_P(sp_str);
    }
    print_P(tab_str);
    addr -= 16;
    for (uint8_t i = 0; i < 16; i++) {
      if (*addr < 127 && *addr > 31)
        ioDirector.write((char)*addr);
      else print_P(PSTR("."));
      addr++;
    }
    ioDirector.printString("\r");
  }
  ioDirector.printString("\r\n");
}

const PROGMEM char see_str[] = "see";
// ("<spaces>name" -- )
// Display a human-readable representation of the named word's definition. The
// source of the representation (object-code decompilation, source block, etc.)
// and the particular form of the display in implementation defined.
static void _see(void) {
  bool isLiteral, done;

  _tick();
  char flags = wordFlags;
  if (flags && IMMEDIATE)
    print_P(PSTR("\r\nImmediate Word"));
  cell_t xt = pop();
  if (xt < 255) {
    print_P(PSTR("\r\nWord is a primitive"));
  } else {
    cell_t *addr = (cell_t *) xt;
    print_P(PSTR("\r\nCode Field Address: $"));
    ioDirector.printInt((size_t)addr, HEX);
    print_P(PSTR("\r\nAddr\tXT\tName"));
    do {
      isLiteral = done = false;
      print_P(PSTR("\r\n$"));
      ioDirector.printInt((size_t)addr, HEX);
      print_P(tab_str);
      ioDirector.printInt(*addr, HEX);
      print_P(tab_str);
      xtToName(*addr);
      switch (*addr) {
        case 2:
          isLiteral = true;
        case 4:
        case 5:
          print_P(PSTR("("));
          ioDirector.printInt(*++addr);
          print_P(PSTR(")"));
          break;
        case 13:
        case 14:
          print_P(sp_str);
          char *ptr = (char*)++addr;
          do {
            ioDirector.write(*ptr++);
          } while (*ptr != 0);
          print_P(PSTR("\x22"));
          addr = (cell_t *)++ptr;
          ALIGN_P(addr);
          addr--;
          break;
      }
      // We're done if exit code but not a literal with value of one
      done = ((*addr++ == 1) && (! isLiteral));
    } while (! done);
  }
  ioDirector.printString("\n");
}

const PROGMEM char words_str[] = "words";
static void _words(void) { // --
  uint8_t count = 0;
  uint8_t index = 0;
  uint8_t length = 0;
  char* pChar;

  while (pgm_read_dword(&(flashDict[index].name))) {
    if (count > 70) {
      ioDirector.printString("\n");
      count = 0;
    }
    if (!(pgm_read_byte(&(flashDict[index].flags)) & SMUDGE)) {
      count += print_P((char*) pgm_read_dword(&(flashDict[index].name)));
      count += print_P(sp_str);
    }
    index++;
  }

  pUserEntry = pLastUserEntry;
  while (pUserEntry) {
    if (count > 70) {
      ioDirector.printString("\n");
      count = 0;
    }
    if (!(pUserEntry->flags & SMUDGE)) {
      count += ioDirector.printString(pUserEntry->name);
      count += print_P(sp_str);
    }
    pUserEntry = (userEntry_t*)pUserEntry->prevEntry;
  }
  ioDirector.printString("\n");
}

#endif

/*******************************************************************************/
/**                               Search Set                                  **/
/*******************************************************************************/
#ifdef SEARCH_SET
#endif

/*******************************************************************************/
/**                               String Set                                  **/
/*******************************************************************************/
#ifdef STRING_SET
#endif

/********************************************************************************/
/**                         EEPROM Operations                                  **/
/********************************************************************************/
#ifdef EN_EEPROM_OPS
const PROGMEM char eeRead_str[] = "eeRead";
static void _eeprom_read(void) {             // address -- value
  push(EEPROM.read(pop()));
}

const PROGMEM char eeWrite_str[] = "eeWrite";
static void _eeprom_write(void) {             // value address --
  char address;
  char value;
  address = (char) pop();
  value = (char) pop();
  EEPROM.write(address, value);
}
#endif

/********************************************************************************/
/**                      Arduino Library Operations                            **/
/********************************************************************************/
#ifdef EN_ARDUINO_OPS

const PROGMEM char delay_str[] = "delay";
static void _delay(void) {
  delay(pop());
}

const PROGMEM char pinWrite_str[] = "pinWrite";
// ( u1 u2 -- )
// Write a high (1) or low (0) value to a digital pin
// u2 is the pin and u1 is the value ( 1 or 0 ).
static void _pinWrite(void) {
  digitalWrite(pop(), pop());
}

const PROGMEM char pinMode_str[] = "pinMode";
// ( u1 u2 -- )
// Set the specified pin behavior to either an input (0) or output (1)
// u2 is the pin and u1 is the mode ( 1 or 0 ).
static void _pinMode(void) {
  pinMode(pop(), pop());
}

const PROGMEM char pinRead_str[] = "pinRead";
static void _pinRead(void) {
  push(digitalRead(pop()));
}

const PROGMEM char analogRead_str[] = "analogRead";
static void _analogRead(void) {
  push(analogRead(pop()));
}

const PROGMEM char analogWrite_str[] = "analogWrite";
// ( u1 u2 -- )
// Write analog PWM value to a pin
// u2 is the pin and u1 is the value ( 0 to 255 ).
static void _analogWrite(void) {
  analogWrite(pop(), pop());
}

const PROGMEM char to_name_str[] = ">name";
static void _toName(void) {
  xtToName(pop());
}
#endif

#ifdef EN_CALEXT_OPS

// ***************************************************************
// Begin CAL extensions
// ***************************************************************

const PROGMEM char gt_equal_str[] = ">=";
// ( x1 x2 -- flag )
// flag is true if and only if x1 >= x2
static void _gt_equal(void) {
  cell_t x2 = pop();
  cell_t x1 = pop();
  if (x1 >= x2) push(TRUE);
  else push(FALSE);
}

const PROGMEM char freeHeap_str[] = "freeHeap";
static void _freeHeap(void) {
  push(freeHeap());
}

const PROGMEM char restart_str[] = "restart";
// Performs hardware reset of the ESP8266
// There is no return from this word
static void _restart(void) {
  ioDirector.printString("\n\n*** System Restart ***\n\n");
  delay(1000);

  // Pull reset line low to force hardware reset
  digitalWrite(GPIO_RESET, LOW);
  pinMode(GPIO_RESET, OUTPUT);
  delay(100);
  // Change mode back to input or we won't be able to
  // upload code.
  pinMode(GPIO_RESET, INPUT);
}

const PROGMEM char randomMax_str[] = "randomMax";
// Returns a random number between 0 and max-1
// (max -- randomMax)
static void _randomMax(void) {
  push(random(pop()));
}

const PROGMEM char randomMinMax_str[] = "randomMinMax";
// Returns a random number between min and max-1
// (max min -- randomMinMax)
static void _randomMinMax(void) {
  push(random(pop(), pop()));
}

#ifdef HAS_SD_CARD
const PROGMEM char load_str[] = "load";
// Load a named file of Forth from SD card
static void _load(void) {
  // Parse the filename from the command line
  if (!getToken()) {
    push(-16);
    _throw();
    return;
  }
  // Pass filename to IODirector
  if (ioDirector.setFile(cTokenBuffer)) {

    // Select FILE_IO as the source
    ioDirector.selectChannel(FILE_IO);
  }
}
#endif

const PROGMEM char freeFSP_str[] = "freeForthSpacePercent";
static void _freeFSP(void) {
  push(freeForthSpacePercent());
}

const PROGMEM char serialIO_str[] = "serialIO";
static void _serialIO(void) {
  ioDirector.selectChannel(SERIAL_IO);
}

const PROGMEM char netIO_str[] = "netIO";
static void _netIO(void) {
  ioDirector.selectChannel(NET_IO);
}

const PROGMEM char setLastEntry_str[] = "setLastEntry";
// (n -- )
// addr is a data-space pointer to the last defined non-primitive word.
static void _setLastEntry(void) {
  pLastUserEntry = (userEntry_t *) pop();
}

const PROGMEM char setHere_str[] = "setHere";
//  (n -- )
static void _setHere(void) {
  pHere = (cell_t *) pop();
}

// Search through the flashDict and return index associated with name
uint8_t findIndex(const char* name) {
  uint8_t index = 0;

  // Search through the flash Dictionary
  while (pgm_read_dword(&(flashDict[index].name))) {
    if (!strcasecmp_P(name, (char*) pgm_read_dword(&(flashDict[index].name)))) {
      return index + 1;
    }
    index++;
  }
  // Didn't find name
  return 0;
}

const PROGMEM char marker_str[] = "marker";
static void _marker(void) {
  _create();
  *pHere++ = (cell_t) pNewUserEntry;
  *pHere++ = (cell_t) pNewUserEntry->prevEntry;
  closeEntry();
  *pDoes = (cell_t) pHere;
  *pHere++ = findIndex("dup");
  *pHere++ = findIndex("@");
  *pHere++ = findIndex("setHere");
  *pHere++ = findIndex("cell+");
  *pHere++ = findIndex("@");
  *pHere++ = findIndex("setLastEntry");
  *pHere++ = EXIT_IDX;
}

const PROGMEM char comment_str[] = "\\";
// Comment to end of line
// ( "ccc<backslash>" -- )
static void _comment(void) {
  push('\n');
  _word();
  _drop();
}

const PROGMEM char case_str[] = "case";
// Interpretation semantics for this word are undefined.
// Compilation: ( C: -- case-sys )
// Mark the start of the CASE ... OF ... ENDOF ... ENDCASE structure. Append the run-time
// semantics given below to the current definition.
// Run-time: ( -- )
// Continue execution.
static void _case(void) {
  push(CASE_SYS);
  push(0); // Count of of clauses
}

const PROGMEM char of_str[] = "of";
// Interpretation semantics for this word are undefined.
// Compilation: ( C: -- of-sys )
// Put of-sys onto the control flow stack. Append the run-time semantics given below to
// the current definition. The semantics are incomplete until resolved by a consumer of
// of-sys such as ENDOF.
// Run-time: ( x1 x2 -- | x1 )
// If the two values on the stack are not equal, discard the top value and continue execution
// at the location specified by the consumer of of-sys, e.g., following the next ENDOF.
// Otherwise, discard both values and continue execution in line.
static void _of(void) {
  push(pop() + 1);      // Increment count of of clauses
  rPush(pop());         // Move to return stack

  push(OF_SYS);
  *pHere++ = OVER_IDX;  // Postpone over
  *pHere++ = EQUAL_IDX; // Postpone =
  *pHere++ = ZJUMP_IDX; // If
  *pHere = 0;           // Filled in by endof
  push((size_t) pHere++);// Push address of jump address onto control stack
  push(rPop());         // Bring of count back
}

const PROGMEM char endof_str[] = "endof";
// Interpretation semantics for this word are undefined.
// Compilation: ( C: case-sys1 of-sys -- case-sys2 )
// Mark the end of the OF ... ENDOF part of the CASE structure. The next location for a
// transfer of control resolves the reference given by of-sys. Append the run-time semantics
// given below to the current definition. Replace case-sys1 with case-sys2 on the
// control-flow stack, to be resolved by ENDCASE.
// Run-time: ( -- )
// Continue execution at the location specified by the consumer of case-sys2.
static void _endof(void) {
  cell_t *back, *forward;

  rPush(pop());         // Move of count to return stack

  // Prepare jump to endcase
  *pHere++ = JUMP_IDX;
  *pHere = 0;
  forward = pHere++;

  back = (cell_t*) pop(); // Resolve If from of
  *back = (size_t) pHere - (size_t) back;

  if (pop() != OF_SYS) { // Make sure control structure is consistent
    push(-22);
    _throw();
    return;
  }
  // Place forward jump address onto control stack
  push((cell_t) forward);
  push(rPop());          // Bring of count back
}

const PROGMEM char endcase_str[] = "endcase";
// Interpretation semantics for this word are undefined.
// Compilation: ( C: case-sys -- )
// Mark the end of the CASE ... OF ... ENDOF ... ENDCASE structure. Use case-sys to resolve
// the entire structure. Append the run-time semantics given below to the current definition.
// Run-time: ( x -- )
// Discard the case selector x and continue execution.
static void _endcase(void) {
  cell_t *orig;

  // Resolve all of the jumps from of statements to here
  int count = pop();
  for (int i = 0; i < count; i++) {
    orig = (cell_t *) pop();
    *orig = (size_t) pHere - (size_t) orig;
  }

  *pHere++ = DROP_IDX;      // Postpone drop of case selector

  if (pop() != CASE_SYS) {  // Make sure control structure is consistent
    push(-22);
    _throw();
  }
}

#ifdef HAS_SD_CARD
// SD card functions

// List files on SD card
const PROGMEM char files_str[] = "files";
static void _files(void) {
  File root = SD.open("/");
  root.rewindDirectory();
  ioDirector.printString("SD card files and sizes\n");
  while (true) {
    File entry =  root.openNextFile();
    if (! entry) { // More files ?
      break;
    }
    // Is the entry a file or a directory ?
    if (! entry.isDirectory()) {
      // Not a directory so display it
      char *pFilename = entry.name();
      // Ignor filenames containing ~ tildas
      if (! strchr(pFilename, '~')) {
        ioDirector.printString(pFilename);
        print_P(tab_str);
        ioDirector.printInt(entry.size(), DEC);
        ioDirector.printString("\n");
      }
    }
    entry.close();
  }
  root.close();
  ioDirector.printString("\n");
}

// Show the contents of a file on the SD card
const PROGMEM char showfile_str[] = "showfile";
// Show a named file of Forth from the SD card
static void _showfile(void) {
  int count = 0;

  // Parse the filename from the command line
  if (!getToken()) {
    push(-16);
    _throw();
    return;
  }
  // Attempt to open the file
  File file = SD.open(cTokenBuffer);

  // If the file is available, read it
  if (file) {
    while (file.available()) {
      // Protect against queue overflow
      count++;
      if (count >= 500) {
        ioDirector.processQueues();
        count = 0;
      }
      ioDirector.write(file.read());
    }
    file.close();
  } else {
    // File open failed
    print_P(PSTR("Error opening file: "));
    ioDirector.printString(cTokenBuffer);
  }
  ioDirector.printString("\n\n");
}
#endif

#ifdef HAS_LCD

// LCD functions

// Turn LCD back light off and on
const PROGMEM char lcd_str[] = "lcd";
static void _lcd(void) {
  cell_t param = pop();
  if (param == TRUE) {
    digitalWrite(GPIO_TFT_LITE, HIGH);
  } else  {
    digitalWrite(GPIO_TFT_LITE, LOW);
  }
}

// Get lcd width
const PROGMEM char lcdWidth_str[] = "lcdWidth";
static void _lcdWidth(void) {
  push(lcd.width());
}

// Get lcd height
const PROGMEM char lcdHeight_str[] = "lcdHeight";
static void _lcdHeight(void) {
  push(lcd.height());
}

// Convert RGB color value into rgb565 color value
const PROGMEM char lcdColor_str[] = "lcdColor";
// ( r g b -- rgb565 )
static void _lcdColor(void) {
  cell_t blue = pop();
  cell_t green = pop();
  cell_t red = pop();
  push(lcd.Color565((uint8_t) red, (uint8_t) green, (uint8_t) blue));
}

// Clear the screen to black
const PROGMEM char lcdClear_str[] = "lcdClear";
static void _lcdClear(void) {
  lcd.fillScreen(ST7735_BLACK);
}

// Fill the screen with specified color
const PROGMEM char lcdFill_str[] = "lcdFill";
// ( rgb565 -- )
static void _lcdFill(void) {
  cell_t c = pop();
  lcd.fillScreen(c);
}

// Draw pixel on lcd
const PROGMEM char lcdPixel_str[] = "lcdPixel";
// ( x y rgb565 -- )
static void _lcdPixel(void) {
  cell_t c = pop();
  cell_t y = pop();
  cell_t x = pop();
  lcd.drawPixel((uint8_t) x, (uint8_t) y, (uint16_t) c);
}

// Fill a rectangle on the lcd
const PROGMEM char lcdDrawRect_str[] = "lcdDrawRect";
// ( x y width height rgb565 -- )
static void _lcdDrawRect(void) {
  cell_t c = pop();
  cell_t h = pop();
  cell_t w = pop();
  cell_t y = pop();
  cell_t x = pop();
  lcd.drawRect(x, y, w, h, c);
}

// Fill a rectangle on the lcd
const PROGMEM char lcdFillRect_str[] = "lcdFillRect";
// ( x y width height rgb565 -- )
static void _lcdFillRect(void) {
  cell_t c = pop();
  cell_t h = pop();
  cell_t w = pop();
  cell_t y = pop();
  cell_t x = pop();
  lcd.fillRect(x, y, w, h, c);
}

// Draw a circle on the lcd
const PROGMEM char lcdDrawCircle_str[] = "lcdDrawCircle";
// ( x y radius rgb565 -- )
static void _lcdDrawCircle(void) {
  cell_t c = pop();
  cell_t r = pop();
  cell_t y = pop();
  cell_t x = pop();
  lcd.drawCircle(x, y, r, c);
}

// Draw a filled circle on the lcd
const PROGMEM char lcdFillCircle_str[] = "lcdFillCircle";
// ( x y radius rgb565 -- )
static void _lcdFillCircle(void) {
  cell_t c = pop();
  cell_t r = pop();
  cell_t y = pop();
  cell_t x = pop();
  lcd.fillCircle(x, y, r, c);
}

// Draw text on the lcd
const PROGMEM char lcdDrawText_str[] = "lcdDrawText";
// ( x y size rgb565 addr u -- )
static void _lcdDrawText(void) {
  pop();  // count is unneeded
  cell_t *addr = (cell_t *) pop();
  cell_t c = pop();
  cell_t s = pop();
  cell_t y = pop();
  cell_t x = pop();

  lcd.setCursor(x, y);
  lcd.setTextSize(s);
  lcd.setTextColor(c);
  lcd.print((char *) addr);
}
#endif

// ***************************************************************
// End CAL custom words
// ***************************************************************
#endif

/*********************************************************************************/
/**                         Dictionary Initialization                           **/
/*********************************************************************************/
const PROGMEM flashEntry_t flashDict[] = {
  /*****************************************************/
  /* The initial entries must stay in this order so    */
  /* they always have the same index. They get called  */
  /* when compiling code.                              */
  /*****************************************************/
  { exit_str,           _exit,            NORMAL },          // 1
  { literal_str,        _literal,         IMMEDIATE },
  { type_str,           _type,            NORMAL },
  { jump_str,           _jump,            SMUDGE },
  { zjump_str,          _zjump,           SMUDGE },
  { subroutine_str,     _subroutine,      SMUDGE },
  { throw_str,          _throw,           NORMAL },
  { do_sys_str,         _do_sys,          SMUDGE },
  { loop_sys_str,       _loop_sys,        SMUDGE },
  { leave_sys_str,      _leave_sys,       SMUDGE },         // 10
  { plus_loop_sys_str,  _plus_loop_sys,   SMUDGE },
  { evaluate_str,       _evaluate,        NORMAL },
  { s_quote_str,        _s_quote,         IMMEDIATE + COMP_ONLY },
  { dot_quote_str,      _dot_quote,       IMMEDIATE + COMP_ONLY },
  { variable_str,       _variable,        NORMAL },
  { over_str,           _over,            NORMAL }, // CAL
  { eq_str,             _eq,              NORMAL }, // CAL
  { drop_str,           _drop,            NORMAL }, // CAL  // 18

  /*****************************************************/
  /* Order does not matter after here                  */
  /*****************************************************/
  { abort_str,          _abort,           NORMAL },        // 19
  { store_str,          _store,           NORMAL },        // 20
  { number_sign_str,    _number_sign,     NORMAL },
  { number_sign_gt_str, _number_sign_gt,  NORMAL },
  { number_sign_s_str,  _number_sign_s,   NORMAL },
  { tick_str,           _tick,            NORMAL },
  { paren_str,          _paren,           IMMEDIATE },
  { star_str,           _star,            NORMAL },
  { star_slash_str,     _star_slash,      NORMAL },
  { star_slash_mod_str, _star_slash_mod,  NORMAL },
  { plus_str,           _plus,            NORMAL },
  { plus_store_str,     _plus_store,      NORMAL },      // 30
  { plus_loop_str,      _plus_loop,       IMMEDIATE + COMP_ONLY },
  { comma_str,          _comma,           NORMAL },
  { minus_str,          _minus,           NORMAL },
  { dot_str,            _dot,             NORMAL },
  { slash_str,          _slash,           NORMAL },
  { slash_mod_str,      _slash_mod,       NORMAL },
  { zero_less_str,      _zero_less,       NORMAL },
  { zero_equal_str,     _zero_equal,      NORMAL },
  { one_plus_str,       _one_plus,        NORMAL },
  { one_minus_str,      _one_minus,       NORMAL },      // 40
  { two_store_str,      _two_store,       NORMAL },
  { two_star_str,       _two_star,        NORMAL },
  { two_slash_str,      _two_slash,       NORMAL },
  { two_fetch_str,      _two_fetch,       NORMAL },
  { two_drop_str,       _two_drop,        NORMAL },
  { two_dup_str,        _two_dup,         NORMAL },
  { two_over_str,       _two_over,        NORMAL },
  { two_swap_str,       _two_swap,        NORMAL },
  { colon_str,          _colon,           NORMAL },
  { semicolon_str,      _semicolon,       IMMEDIATE },  // 50
  { lt_str,             _lt,              NORMAL },
  { lt_number_sign_str, _lt_number_sign,  NORMAL },
  { gt_str,             _gt,              NORMAL },
  { to_body_str,        _to_body,         NORMAL },
  { to_in_str,          _to_in,           NORMAL },
  { to_number_str,      _to_number,       NORMAL },
  { to_r_str,           _to_r,            NORMAL },
  { question_dup_str,   _question_dup,    NORMAL },
  { fetch_str,          _fetch,           NORMAL },
  { abort_quote_str,    _abort_quote,     IMMEDIATE + COMP_ONLY }, // 60
  { abs_str,            _abs,             NORMAL },
  { accept_str,         _accept,          NORMAL },
  { align_str,          _align,           NORMAL },
  { aligned_str,        _aligned,         NORMAL },
  { allot_str,          _allot,           NORMAL },
  { and_str,            _and,             NORMAL },
  { base_str,           _base,            NORMAL },
  { begin_str,          _begin,           IMMEDIATE + COMP_ONLY },
  { bl_str,             _bl,              NORMAL },
  { c_store_str,        _c_store,         NORMAL },      // 70
  { c_comma_str,        _c_comma,         NORMAL },
  { c_fetch_str,        _c_fetch,         NORMAL },
  { cell_plus_str,      _cell_plus,       NORMAL },
  { cells_str,          _cells,           NORMAL },
  { char_str,           _char,            NORMAL },
  { char_plus_str,      _char_plus,       NORMAL },
  { chars_str,          _chars,           NORMAL },
  { constant_str,       _constant,        NORMAL },
  { count_str,          _count,           NORMAL },
  { cr_str,             _cr,              NORMAL },     // 80
  { create_str,         _create,          NORMAL },
  { decimal_str,        _decimal,         NORMAL },
  { depth_str,          _depth,           NORMAL },
  { do_str,             _do,              IMMEDIATE + COMP_ONLY },
  { does_str,           _does,            IMMEDIATE + COMP_ONLY },
  { dupe_str,           _dupe,            NORMAL },
  { else_str,           _else,            IMMEDIATE + COMP_ONLY },
  { emit_str,           _emit,            NORMAL },
  { environment_str,    _environment,     NORMAL },
  { execute_str,        _execute,         NORMAL },     // 90
  { fill_str,           _fill,            NORMAL },
  { find_str,           _find,            NORMAL },
  { fm_slash_mod_str,   _fm_slash_mod,    NORMAL },
  { here_str,           _here,            NORMAL },
  { hold_str,           _hold,            NORMAL },
  { i_str,              _i,               NORMAL },
  { if_str,             _if,              IMMEDIATE + COMP_ONLY },
  { immediate_str,      _immediate,       NORMAL },
  { invert_str,         _invert,          NORMAL },
  { j_str,              _j,               NORMAL },     // 100
  { key_str,            _key,             NORMAL },
  { leave_str,          _leave,           IMMEDIATE + COMP_ONLY },
  { loop_str,           _loop,            IMMEDIATE + COMP_ONLY },
  { lshift_str,         _lshift,          NORMAL },
  { max_str,            _ymax,            NORMAL },
  { min_str,            _ymin,            NORMAL },
  { mod_str,            _mod,             NORMAL },
  { move_str,           _move,            NORMAL },
  { negate_str,         _negate,          NORMAL },
  { or_str,             _or,              NORMAL },     // 110
  { postpone_str,       _postpone,        IMMEDIATE + COMP_ONLY },
  { quit_str,           _quit,            NORMAL },
  { r_from_str,         _r_from,          NORMAL },
  { r_fetch_str,        _r_fetch,         NORMAL },
  { recurse_str,        _recurse,         IMMEDIATE + COMP_ONLY },
  { repeat_str,         _repeat,          IMMEDIATE + COMP_ONLY },
  { rot_str,            _rot,             NORMAL },
  { rshift_str,         _rshift,          NORMAL },
  { s_to_d_str,         _s_to_d,          NORMAL },
  { sign_str,           _sign,            NORMAL },    // 120
  { sm_slash_rem_str,   _sm_slash_rem,    NORMAL },
  { source_str,         _source,          NORMAL },
  { space_str,          _space,           NORMAL },
  { spaces_str,         _spaces,          NORMAL },
  { state_str,          _state,           NORMAL },
  { swap_str,           _swap,            NORMAL },
  { then_str,           _then,            IMMEDIATE + COMP_ONLY },
  { u_dot_str,          _u_dot,           NORMAL },
  { u_lt_str,           _u_lt,            NORMAL },
  { um_star_str,        _um_star,         NORMAL },    // 130
  { um_slash_mod_str,   _um_slash_mod,    NORMAL },
  { unloop_str,         _unloop,          NORMAL + COMP_ONLY },
  { until_str,          _until,           IMMEDIATE + COMP_ONLY },
  { while_str,          _while,           IMMEDIATE + COMP_ONLY },
  { word_str,           _word,            NORMAL },
  { xor_str,            _xor,             NORMAL },
  { left_bracket_str,   _left_bracket,    IMMEDIATE },
  { bracket_tick_str,   _bracket_tick,    IMMEDIATE },
  { bracket_char_str,   _bracket_char,    IMMEDIATE },
  { right_bracket_str,  _right_bracket,   NORMAL },      // 140
  { to_name_str,        _toName,          NORMAL },      // 141

#ifdef CORE_EXT_SET
  { neq_str,            _neq,             NORMAL },      // 142
  { hex_str,            _hex,             NORMAL },      // 143
#endif

#ifdef DOUBLE_SET
#endif

#ifdef EXCEPTION_SET
#endif

#ifdef LOCALS_SET
#endif

#ifdef MEMORY_SET
#endif

#ifdef TOOLS_SET
  { dot_s_str,          _dot_s,           NORMAL },    // 144
  { dump_str,           _dump,            NORMAL },    // 145
  { see_str,            _see,             NORMAL },    // 146
  { words_str,          _words,           NORMAL },    // 147
#endif

#ifdef SEARCH_SET
#endif

#ifdef STRING_SET
#endif

#ifdef EN_ARDUINO_OPS
  { delay_str,          _delay,           NORMAL },    // 148
  { pinWrite_str,       _pinWrite,        NORMAL },
  { pinMode_str,        _pinMode,         NORMAL },    // 150
  { pinRead_str,        _pinRead,         NORMAL },
  { analogRead_str,     _analogRead,      NORMAL },
  { analogWrite_str,    _analogWrite,     NORMAL },    // 153
#endif

#ifdef EN_EEPROM_OPS
  { eeRead_str,     _eeprom_read,         NORMAL },    // 154
  { eeWrite_str,    _eeprom_write,        NORMAL },    // 155
#endif

#ifdef EN_CALEXT_OPS

  { gt_equal_str,       _gt_equal,        NORMAL },    // CAL  156
  { freeHeap_str,       _freeHeap,        NORMAL },    // CAL
  { freeFSP_str,        _freeFSP,         NORMAL },    // CAL
  { randomMax_str,      _randomMax,       NORMAL },    // CAL
  { randomMinMax_str,   _randomMinMax,    NORMAL },    // CAL  160
#ifdef HAS_SD_CARD
  { load_str,           _load,            NORMAL },    // CAL
#endif
  { restart_str,        _restart,         NORMAL },    // CAL
  { serialIO_str,       _serialIO,        NORMAL },    // CAL
  { netIO_str,          _netIO,           NORMAL },    // CAL
  { setLastEntry_str,   _setLastEntry,    SMUDGE },    // CAL
  { setHere_str,        _setHere,         SMUDGE },    // CAL
  { marker_str,         _marker,          NORMAL },    // CAL
  { comment_str,        _comment,         IMMEDIATE }, // CAL
  { case_str,           _case,            IMMEDIATE + COMP_ONLY },    // CAL
  { of_str,             _of,              IMMEDIATE + COMP_ONLY },    // CAL  170
  { endof_str,          _endof,           IMMEDIATE + COMP_ONLY },    // CAL
  { endcase_str,        _endcase,         IMMEDIATE + COMP_ONLY },    // CAL  172

#ifdef HAS_SD_CARD
  { files_str,          _files,           NORMAL },    // CAL   173
  { showfile_str,       _showfile,        NORMAL },    // CAL   174
#endif

#ifdef HAS_LCD
  { lcd_str,            _lcd,             NORMAL },    // CAL   175
  { lcdColor_str,       _lcdColor,        NORMAL },    // CAL
  { lcdWidth_str,       _lcdWidth,        NORMAL },    // CAL
  { lcdHeight_str,      _lcdHeight,       NORMAL },    // CAL
  { lcdClear_str,       _lcdClear,        NORMAL },    // CAL
  { lcdFill_str,        _lcdFill,         NORMAL },    // CAL   180
  { lcdPixel_str,       _lcdPixel,        NORMAL },    // CAL
  { lcdDrawRect_str,    _lcdDrawRect,     NORMAL },    // CAL
  { lcdFillRect_str,    _lcdFillRect,     NORMAL },    // CAL
  { lcdDrawCircle_str,  _lcdDrawCircle,   NORMAL },    // CAL
  { lcdFillCircle_str,  _lcdFillCircle,   NORMAL },    // CAL
  { lcdDrawText_str,    _lcdDrawText,     NORMAL },    // CAL   186
#endif

#endif

  { NULL,           NULL,    NORMAL }
};


