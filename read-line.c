/*
 * CS252: Systems Programming
 * Purdue University
 * Example that shows how to read one line with simple editing
 * using raw terminal.
 */

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <dirent.h>

#define MAX_BUFFER_LINE 4096

extern void tty_raw_mode(void);
extern void tty_can_mode(void);

// Buffer where line is stored
int line_length;
char line_buffer[MAX_BUFFER_LINE];

//History
char * history [64];
int history_length = 0;
int history_index = 0;

//Cursor
int cursor;

__attribute__((destructor))
void free_history() {
  for (int i = 0; i < history_length; i++) {
    free(history[i]);
  }
}

//Reset Buffer
void reset_buffer() {
  memset(line_buffer, 0, MAX_BUFFER_LINE);
  line_length = 0;
  cursor = 0;
}

//Simple write function
void wr(char c) {
  char character = c;
  write(1, &character, 1);
}

//Simple read function
void rd(char* c) {
  read(0, c, 1);
}

//Go back
void back() {
  wr(8);
}

void back_loop(int length) {
  for (int i = 0; i < length; i++) {
    back();
  }
}

//Refresh terminal
void refresh() {
  // Erase old line
  // Print backspaces
  back_loop(cursor);

  // Print spaces on top
  for (int i = 0; i < line_length; i++) {
    wr(' ');
  }

  // Print backspaces
  back_loop(line_length);
}

//Get common prefix
char* common_prefix(char* strings[], int length) {
  int cur_length = strlen(strings[0]);

  //Loop through
  for (int i = 1; i < length; i++) {
    int match = 0;
    while (match < cur_length 
    && strings[i - 1][match] == strings[i][match]) {
      match++;
    }

    cur_length = match;
  }

  //Set up prefix
  char* prefix = calloc(cur_length + 1, sizeof(char));
  strncpy(prefix, strings[0], cur_length);

  //Return
  return prefix;

}

//If printable character
int print_char(char ch) {

  if (cursor == line_length) {
    wr(ch);
    line_buffer[line_length] = ch; 
  } else {
    int i;

    //Fix line_buffer
    for (i = line_length + 1; i > cursor; i--) {
      line_buffer[i] = line_buffer[i - 1];
    }

    line_buffer[i] = ch;

    //Write characters
    while (i < line_length + 1) {
      wr(line_buffer[i++]);
    }

    //Go Back
    back_loop(i - cursor - 1);
  }

  // If max number of character reached return.
  if (line_length==MAX_BUFFER_LINE-2) return 1;

  line_length++;
  cursor++;

  return 0;
}

//If backspace is pressed
void backspace() {
  // <backspace> was typed. Remove previous character read.
  if (!cursor) return;

  // Go back one character
  back();

  // If in the middle
  if (cursor < line_length) {
    //Index for incrementing
    int idx = cursor;

    //Replace each character with next one
    while (idx <= line_length) {
      wr(line_buffer[idx]);
      line_buffer[idx - 1] = line_buffer[idx];
      idx++;
    }

    wr(' ');
    back();
    idx--;

    while (idx > cursor) {
      back();
      idx--;
    }
  } else { //At the end
    wr(' ');
    back();
    line_buffer[line_length - 1] = 0;
  }

  // Remove one character from buffer
  line_length--;
  cursor--; 
}

//Delete key
void delete() {
  //Check if at end
  if (cursor == line_length) return;

  //Index for incrementing
  int idx = cursor + 1;

  //Replace each character with next one
  while (idx <= line_length) {
    wr(line_buffer[idx]);
    line_buffer[idx - 1] = line_buffer[idx];
    idx++;
  }

  wr(' ');
  idx--;

  while (idx > cursor) {
    back();
    idx--;
  }

  line_length--;
}

//Tab Completion
void tab() {
  //Open directory and declare  array for matches
  DIR* dirp = opendir(".");
  char* matches[1024];
  int idx = 0;

  //Check if error with opendir
  if (dirp == NULL) return;

  //Declare entry
  struct dirent *entry;

  //Loop through all entries
  while ((entry = readdir(dirp))) {
    if (strstr(entry->d_name, line_buffer) == entry->d_name) {
      matches[idx] = calloc(strlen(entry->d_name) + 1, sizeof(char));
      strcpy(matches[idx++], entry->d_name);
    }
  }
  
  //Check if any matches
  if (!idx) return;

  //Grab longest substr
  char* longest_substr = common_prefix(matches, idx);

  //Rewrite
  refresh();
  reset_buffer();

  //Reset Matches
  while(idx > 0) {
    free(matches[--idx]);
    matches[idx] = 0;
  }

  // Copy line from longest substr
  strcpy(line_buffer, longest_substr);
  line_length = strlen(line_buffer);
  line_buffer[line_length] = 0;
  cursor = line_length;
  
  //Free
  free(longest_substr);

  // echo line
  write(1, line_buffer, line_length); 
}

//For Up Arrow
void up_arr() {
  //Check if need to go up
  if (!history_length) {
    return;
  }
  
  refresh();

  //Check if at bottom
  if (!strcmp(line_buffer, history[history_index]) && history_index == history_length) {
    history_index--;
  }

  // Copy line from history
  strcpy(line_buffer, history[history_index]);
  line_length = strlen(line_buffer);
  line_buffer[line_length] = 0;
  cursor = line_length;
  history_index = fmax(0, history_index - 1);

  // echo line
  write(1, line_buffer, line_length);
}

//For Down Arrow
void down_arr() {
  //Check if need to go up
  if (!history_length) {
    return;
  }

  refresh();

  //Check if at top
  if (!strcmp(line_buffer, history[history_index]) && !history_index) {
    history_index++;
  }

  // Copy line from history
  strcpy(line_buffer, history[history_index]);
  line_length = strlen(line_buffer);
  line_buffer[line_length] = 0;
  cursor = line_length;
  history_index = fmin(history_length, history_index + 1);

  // echo line
  write(1, line_buffer, line_length);    
  }

//For Left Arrow
void left_arr() {
  //Check if at beginning
  if (!cursor) return;

  back();
  cursor--;
}

//For Right Arrow
void right_arr() {
  //Check if at end
  if (cursor == line_length) return;

  //Write character
  wr(line_buffer[cursor++]);
} 

void read_line_print_usage()
{
  char * usage = "\n"
    " ctrl-?               Print usage\n"
    " Backspace (Ctrl-H)   Deletes previous character\n"
    " Delete (Ctrl-D)      Deletes current character\n"
    " Ctrl-D               Deletes current character\n"
    " Up arrow             See previous command in the history\n"
    " Down arrow           See next command in the history\n"
    " Left arrow           Move cursor left\n"
    " Right arrow          Move cursor right\n"
    " Ctrl-a               Move cursor to the front\n"
    " Ctrl-e               Move cursor to the back\n";

  write(1, usage, strlen(usage));
}

/* 
 * Input a line with some basic editing.
 */
char * read_line() {

  // Set terminal in raw mode
  tty_raw_mode();

  //Set initials
  line_length = 0;
  cursor = 0;
  reset_buffer();

  // Read one line until enter is typed
  while (1) {

    // Read one character in raw mode.
    char ch;
    rd(&ch);

    if (ch>=32 && ch != 127) {
      //Print char and check if line too long
      if (print_char(ch)) break; 
    }
    else if (ch==10) {
      // <Enter> was typed. Return line     
      // Print newline
      wr(ch);

      break;
    }
    else if (ch == 1) {
      //Ctrl-A typed
      back_loop(cursor);
      cursor = 0;
    }
    else if (ch == 5) {
      //Ctrl-E typed
      //Go backward

      while (cursor < line_length) {
        wr(line_buffer[cursor++]);
      }
    }
    else if (ch == 4) {
      delete();
    }
    else if (ch == 8 || ch == 127) {
      backspace();
    }
    else if (ch == 9) {
      tab();
    }
    else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      line_buffer[0]=0;
      break;
    }
    else if (ch==27) {
      // Escape sequence. Read two chars more
      char ch1; 
      char ch2;
      rd(&ch1);
      rd(&ch2);

      //Check type of escape sequence
      if (ch1==91) {
        if (ch2 == 65) {
          // Up arrow
          up_arr();
        } 
        else if (ch2 == 66) { //Down Arrow
          // Down arrow
          down_arr();
        }
        else if (ch2 == 67) { 
          //Right Arrow
          right_arr();
        }
        else if (ch2 == 68) { 
          //Left Arrow
          left_arr();        
        } 
        else if (ch2 == 51) {
          char ch3;
          rd(&ch3);

          if (ch3 == 126) {
            delete();
          }
        }
      }
      
    }

  } //end while

  //Add to History
  char* new_entry = calloc(line_length, sizeof(char));
  strncpy(new_entry, line_buffer, line_length);
  history[history_length] = new_entry;
  history[history_length + 1] = "";

  // Add eol and null char at the end of string
  line_buffer[line_length]=10;
  line_length++;
  line_buffer[line_length]=0;

  //Set variables
  history_index = history_length;
  history_length++;

  //Set back to canonical mode
  tty_can_mode();

  return line_buffer;
}

