// backend.h -- Go frontend interface to backend  -*- C++ -*-

// Copyright 2011 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef GO_BACKEND_H
#define GO_BACKEND_H

class Function_type;
class Struct_type;
class Interface_type;

// Pointers to these types are created by the backend, passed to the
// frontend, and passed back to the backend.  The types must be
// defined by the backend using these names.

// The backend representation of a type.
class Btype;

// The backend represention of an expression.
class Bexpression;

// The backend representation of a statement.
class Bstatement;

// The backend representation of a function definition.
class Bfunction;

// The backend representation of a block.
class Bblock;

// The backend representation of a variable.
class Bvariable;

// The backend representation of a label.
class Blabel;

// A list of backend types.
typedef std::vector<Btype*> Btypes;

// The backend interface.  This is a pure abstract class that a
// specific backend will implement.

class Backend
{
 public:
  virtual ~Backend() { }

  // Types.

  // Produce an error type.  Actually the backend could probably just
  // crash if this is called.
  virtual Btype*
  error_type() = 0;

  // Get a void type.  This is used in (at least) two ways: 1) as the
  // return type of a function with no result parameters; 2)
  // unsafe.Pointer is represented as *void.
  virtual Btype*
  void_type() = 0;

  // Get the unnamed boolean type.
  virtual Btype*
  bool_type() = 0;

  // Get an unnamed integer type with the given signedness and number
  // of bits.
  virtual Btype*
  integer_type(bool is_unsigned, int bits) = 0;

  // Get an unnamed floating point type with the given number of bits.
  virtual Btype*
  float_type(int bits) = 0;

  // Get the unnamed string type.
  virtual Btype*
  string_type() = 0;

  // Get a function type.  The receiver, parameter, and results are
  // generated from the types in the Function_type.  The Function_type
  // is provided so that the names are available.
  virtual Btype*
  function_type(const Function_type*, Btype* receiver,
		const Btypes* parameters,
		const Btypes* results) = 0;

  // Get a struct type.  The Struct_type is provided to get the field
  // names.
  virtual Btype*
  struct_type(const Struct_type*, const Btypes* field_types) = 0;

  // Get an array type.
  virtual Btype*
  array_type(const Btype* element_type, const Bexpression* length) = 0;

  // Get a slice type.
  virtual Btype*
  slice_type(const Btype* element_type) = 0;

  // Get a map type.
  virtual Btype*
  map_type(const Btype* key_type, const Btype* value_type, source_location) = 0;

  // Get a channel type.
  virtual Btype*
  channel_type(const Btype* element_type) = 0;

  // Get an interface type.  The Interface_type is provided to get the
  // method names.
  virtual Btype*
  interface_type(const Interface_type*, const Btypes* method_types) = 0;

  // Statements.

  // Create an error statement.  This is used for cases which should
  // not occur in a correct program, in order to keep the compilation
  // going without crashing.
  virtual Bstatement*
  error_statement() = 0;

  // Create an expression statement.
  virtual Bstatement*
  expression_statement(Bexpression*) = 0;

  // Create a variable initialization statement.  This initializes a
  // local variable at the point in the program flow where it is
  // declared.
  virtual Bstatement*
  init_statement(Bvariable* var, Bexpression* init) = 0;

  // Create an assignment statement.
  virtual Bstatement*
  assignment_statement(Bexpression* lhs, Bexpression* rhs,
		       source_location) = 0;

  // Create a return statement, passing the representation of the
  // function and the list of values to return.
  virtual Bstatement*
  return_statement(Bfunction*, const std::vector<Bexpression*>&,
		   source_location) = 0;

  // Create an if statement.  ELSE_BLOCK may be NULL.
  virtual Bstatement*
  if_statement(Bexpression* condition, Bblock* then_block, Bblock* else_block,
	       source_location) = 0;

  // Create a switch statement where the case values are constants.
  // CASES and STATEMENTS must have the same number of entries.  If
  // VALUE matches any of the list in CASES[i], which will all be
  // integers, then STATEMENTS[i] is executed.  STATEMENTS[i] will
  // either end with a goto statement or will fall through into
  // STATEMENTS[i + 1].  CASES[i] is empty for the default clause,
  // which need not be last.
  virtual Bstatement*
  switch_statement(Bexpression* value,
		   const std::vector<std::vector<Bexpression*> >& cases,
		   const std::vector<Bstatement*>& statements,
		   source_location) = 0;

  // Create a single statement from two statements.
  virtual Bstatement*
  compound_statement(Bstatement*, Bstatement*) = 0;

  // Create a single statement from a list of statements.
  virtual Bstatement*
  statement_list(const std::vector<Bstatement*>&) = 0;

  // Blocks.

  // Create a block.  The frontend will call this function when it
  // starts converting a block within a function.  FUNCTION is the
  // current function.  ENCLOSING is the enclosing block; it will be
  // NULL for the top-level block in a function.  VARS is the list of
  // local variables defined within this block; each entry will be
  // created by the local_variable function.  START_LOCATION is the
  // location of the start of the block, more or less the location of
  // the initial curly brace.  END_LOCATION is the location of the end
  // of the block, more or less the location of the final curly brace.
  // The statements will be added after the block is created.
  virtual Bblock*
  block(Bfunction* function, Bblock* enclosing,
	const std::vector<Bvariable*>& vars,
	source_location start_location, source_location end_location) = 0;

  // Add the statements to a block.  The block is created first.  Then
  // the statements are created.  Then the statements are added to the
  // block.  This will called exactly once per block.  The vector may
  // be empty if there are no statements.
  virtual void
  block_add_statements(Bblock*, const std::vector<Bstatement*>&) = 0;

  // Return the block as a statement.  This is used to include a block
  // in a list of statements.
  virtual Bstatement*
  block_statement(Bblock*) = 0;

  // Variables.

  // Create an error variable.  This is used for cases which should
  // not occur in a correct program, in order to keep the compilation
  // going without crashing.
  virtual Bvariable*
  error_variable() = 0;

  // Create a global variable.  PACKAGE_NAME is the name of the
  // package where the variable is defined.  UNIQUE_PREFIX is the
  // prefix for that package, from the -fgo-prefix option.  NAME is
  // the name of the variable.  BTYPE is the type of the variable.
  // IS_EXTERNAL is true if the variable is defined in some other
  // package.  IS_HIDDEN is true if the variable is not exported (name
  // begins with a lower case letter).  LOCATION is where the variable
  // was defined.
  virtual Bvariable*
  global_variable(const std::string& package_name,
		  const std::string& unique_prefix,
		  const std::string& name,
		  Btype* btype,
		  bool is_external,
		  bool is_hidden,
		  source_location location) = 0;

  // A global variable will 1) be initialized to zero, or 2) be
  // initialized to a constant value, or 3) be initialized in the init
  // function.  In case 2, the frontend will call
  // global_variable_set_init to set the initial value.  If this is
  // not called, the backend should initialize a global variable to 0.
  // The init function may then assign a value to it.
  virtual void
  global_variable_set_init(Bvariable*, Bexpression*) = 0;

  // Create a local variable.  The frontend will create the local
  // variables first, and then create the block which contains them.
  // FUNCTION is the function in which the variable is defined.  NAME
  // is the name of the variable.  TYPE is the type.  LOCATION is
  // where the variable is defined.  For each local variable the
  // frontend will call init_statement to set the initial value.
  virtual Bvariable*
  local_variable(Bfunction* function, const std::string& name, Btype* type,
		 source_location location) = 0;

  // Create a function parameter.  This is an incoming parameter, not
  // a result parameter (result parameters are treated as local
  // variables).  The arguments are as for local_variable.
  virtual Bvariable*
  parameter_variable(Bfunction* function, const std::string& name,
		     Btype* type, source_location location) = 0;

  // Labels.
  
  // Create a new label.  NAME will be empty if this is a label
  // created by the frontend for a loop construct.  The location is
  // where the the label is defined.
  virtual Blabel*
  label(Bfunction*, const std::string& name, source_location) = 0;

  // Create a statement which defines a label.  This statement will be
  // put into the codestream at the point where the label should be
  // defined.
  virtual Bstatement*
  label_definition_statement(Blabel*) = 0;

  // Create a goto statement to a label.
  virtual Bstatement*
  goto_statement(Blabel*, source_location) = 0;

  // Create an expression for the address of a label.  This is used to
  // get the return address of a deferred function which may call
  // recover.
  virtual Bexpression*
  label_address(Blabel*, source_location) = 0;
};

// The backend interface has to define this function.

extern Backend* go_get_backend();

// FIXME: Temporary helper functions while converting to new backend
// interface.

extern Btype* tree_to_type(tree);
extern Bexpression* tree_to_expr(tree);
extern Bstatement* tree_to_stat(tree);
extern Bfunction* tree_to_function(tree);
extern Bblock* tree_to_block(tree);
extern tree expr_to_tree(Bexpression*);
extern tree stat_to_tree(Bstatement*);
extern tree block_to_tree(Bblock*);
extern tree var_to_tree(Bvariable*);

#endif // !defined(GO_BACKEND_H)
