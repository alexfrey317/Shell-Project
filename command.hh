#ifndef command_hh
#define command_hh

#include "simpleCommand.hh"

// Command Data Structure

struct Command {
  std::vector<SimpleCommand *> _simpleCommands;
  std::string * _outFile;
  std::string * _inFile;
  std::string * _errFile;
  bool _background;
  bool append_out;
  bool append_err;

  Command();
  void insertSimpleCommand( SimpleCommand * simpleCommand );

  void clear();
  void print();
  void execute();
  void reset_files(int dflt_in, int dflt_out, int dflt_err);
  void close_files(int dflt_in, int dflt_out, int dflt_err);

  static SimpleCommand *_currentSimpleCommand;
};

#endif
