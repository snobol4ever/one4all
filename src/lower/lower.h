#ifndef LOWER_H
#define LOWER_H
#include "SM.h"
#include "../../ast/ast.h"
#include "parser_output.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
SM_sequence_t *lower(const ParserOutput *po);
#endif
