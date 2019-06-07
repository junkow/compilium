#include "compilium.h"

struct Node *ParseStmt();
struct Node *ParseCompStmt();

struct Node *tokens;
int token_stream_index;

static struct Node *ConsumeToken(enum NodeType type) {
  if (token_stream_index >= GetSizeOfList(tokens)) return NULL;
  struct Node *t = GetNodeAt(tokens, token_stream_index);
  if (t->type != type) return NULL;
  token_stream_index++;
  return t;
}

static struct Node *ConsumePunctuator(const char *s) {
  if (token_stream_index >= GetSizeOfList(tokens)) return NULL;
  struct Node *t = GetNodeAt(tokens, token_stream_index);
  if (!IsEqualTokenWithCStr(t, s)) return NULL;
  token_stream_index++;
  return t;
}

static struct Node *ExpectPunctuator(const char *s) {
  if (token_stream_index >= GetSizeOfList(tokens))
    Error("Expect token %s but got EOF", s);
  struct Node *t = GetNodeAt(tokens, token_stream_index);
  if (!IsEqualTokenWithCStr(t, s))
    ErrorWithToken(t, "Expected token %s here", s);
  token_stream_index++;
  return t;
}

static struct Node *NextToken() {
  if (token_stream_index >= GetSizeOfList(tokens)) return NULL;
  return GetNodeAt(tokens, token_stream_index++);
}

struct Node *ParseCastExpr();
struct Node *ParseExpr(void);

struct Node *ParsePrimaryExpr() {
  struct Node *t;
  if ((t = ConsumeToken(kTokenDecimalNumber)) ||
      (t = ConsumeToken(kTokenOctalNumber)) ||
      (t = ConsumeToken(kTokenIdent)) ||
      (t = ConsumeToken(kTokenCharLiteral)) ||
      (t = ConsumeToken(kTokenStringLiteral))) {
    struct Node *op = AllocNode(kASTExpr);
    op->op = t;
    return op;
  }
  if ((t = ConsumePunctuator("("))) {
    struct Node *op = AllocNode(kASTExpr);
    op->op = t;
    op->right = ParseExpr();
    if (!op->right) ErrorWithToken(t, "Expected expr after this token");
    ExpectPunctuator(")");
    return op;
  }
  return NULL;
}

struct Node *ParseAssignExpr();
struct Node *ParsePostfixExpr() {
  struct Node *n = ParsePrimaryExpr();
  if (!n) return NULL;
  if (ConsumePunctuator("(")) {
    struct Node *args = AllocList();
    if (!ConsumePunctuator(")")) {
      do {
        struct Node *arg_expr = ParseAssignExpr();
        if (!arg_expr) ErrorWithToken(NextToken(), "Expected expression here");
        PushToList(args, arg_expr);
      } while (ConsumePunctuator(","));
      ExpectPunctuator(")");
    }
    struct Node *nn = AllocNode(kASTExprFuncCall);
    nn->func_expr = n;
    nn->arg_expr_list = args;
    n = nn;
  }
  return n;
}

struct Node *ParseUnaryExpr() {
  struct Node *t;
  if ((t = ConsumePunctuator("+")) || (t = ConsumePunctuator("-")) ||
      (t = ConsumePunctuator("~")) || (t = ConsumePunctuator("!")) ||
      (t = ConsumePunctuator("&")) || (t = ConsumePunctuator("*"))) {
    return CreateASTUnaryPrefixOp(t, ParseCastExpr());
  } else if ((t = ConsumeToken(kTokenKwSizeof))) {
    return CreateASTUnaryPrefixOp(t, ParseUnaryExpr());
  }
  return ParsePostfixExpr();
}

struct Node *ParseCastExpr() {
  return ParseUnaryExpr();
}

struct Node *ParseMulExpr() {
  struct Node *op = ParseCastExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("*")) || (t = ConsumePunctuator("/")) ||
         (t = ConsumePunctuator("%"))) {
    op = CreateASTBinOp(t, op, ParseCastExpr());
  }
  return op;
}

struct Node *ParseAddExpr() {
  struct Node *op = ParseMulExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("+")) || (t = ConsumePunctuator("-"))) {
    op = CreateASTBinOp(t, op, ParseMulExpr());
  }
  return op;
}

struct Node *ParseShiftExpr() {
  struct Node *op = ParseAddExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("<<")) || (t = ConsumePunctuator(">>"))) {
    op = CreateASTBinOp(t, op, ParseAddExpr());
  }
  return op;
}

struct Node *ParseRelExpr() {
  struct Node *op = ParseShiftExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("<")) || (t = ConsumePunctuator(">")) ||
         (t = ConsumePunctuator("<=")) || (t = ConsumePunctuator(">="))) {
    op = CreateASTBinOp(t, op, ParseShiftExpr());
  }
  return op;
}

struct Node *ParseEqExpr() {
  struct Node *op = ParseRelExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("==")) || (t = ConsumePunctuator("!="))) {
    op = CreateASTBinOp(t, op, ParseRelExpr());
  }
  return op;
}

struct Node *ParseAndExpr() {
  struct Node *op = ParseEqExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("&"))) {
    op = CreateASTBinOp(t, op, ParseEqExpr());
  }
  return op;
}

struct Node *ParseXorExpr() {
  struct Node *op = ParseAndExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("^"))) {
    op = CreateASTBinOp(t, op, ParseAndExpr());
  }
  return op;
}

struct Node *ParseOrExpr() {
  struct Node *op = ParseXorExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("|"))) {
    op = CreateASTBinOp(t, op, ParseXorExpr());
  }
  return op;
}

struct Node *ParseBoolAndExpr() {
  struct Node *op = ParseOrExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("&&"))) {
    op = CreateASTBinOp(t, op, ParseOrExpr());
  }
  return op;
}

struct Node *ParseBoolOrExpr() {
  struct Node *op = ParseBoolAndExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("||"))) {
    op = CreateASTBinOp(t, op, ParseBoolAndExpr());
  }
  return op;
}

struct Node *ParseConditionalExpr() {
  struct Node *expr = ParseBoolOrExpr();
  if (!expr) return NULL;
  struct Node *t;
  if ((t = ConsumePunctuator("?"))) {
    struct Node *op = AllocNode(kASTExpr);
    op->op = t;
    op->cond = expr;
    op->left = ParseConditionalExpr();
    if (!op->left)
      ErrorWithToken(t, "Expected true-expr for this conditional expr");
    ExpectPunctuator(":");
    op->right = ParseConditionalExpr();
    if (!op->right)
      ErrorWithToken(t, "Expected false-expr for this conditional expr");
    return op;
  }
  return expr;
}

struct Node *ParseAssignExpr() {
  struct Node *left = ParseConditionalExpr();
  if (!left) return NULL;
  struct Node *t;
  if ((t = ConsumePunctuator("="))) {
    struct Node *right = ParseAssignExpr();
    if (!right) ErrorWithToken(t, "Expected expr after this token");
    return CreateASTBinOp(t, left, right);
  }
  return left;
}

struct Node *ParseExpr() {
  struct Node *op = ParseAssignExpr();
  if (!op) return NULL;
  struct Node *t;
  while ((t = ConsumePunctuator(","))) {
    op = CreateASTBinOp(t, op, ParseAssignExpr());
  }
  return op;
}

struct Node *ParseExprStmt() {
  struct Node *expr = ParseExpr();
  struct Node *t;
  if ((t = ConsumePunctuator(";"))) {
    return CreateASTExprStmt(t, expr);
  } else if (expr) {
    ExpectPunctuator(";");
  }
  return NULL;
}

struct Node *ParseSelectionStmt() {
  struct Node *t;
  if ((t = ConsumeToken(kTokenKwIf))) {
    ExpectPunctuator("(");
    struct Node *expr = ParseExpr();
    assert(expr);
    ExpectPunctuator(")");
    struct Node *stmt_true = ParseStmt();
    assert(stmt_true);
    struct Node *stmt = AllocNode(kASTSelectionStmt);
    stmt->op = t;
    stmt->cond = expr;
    stmt->left = stmt_true;
    return stmt;
  }
  return NULL;
}

struct Node *ParseJumpStmt() {
  struct Node *t;
  if ((t = ConsumeToken(kTokenKwReturn))) {
    struct Node *expr = ParseExpr();
    ExpectPunctuator(";");
    struct Node *stmt = AllocNode(kASTJumpStmt);
    stmt->op = t;
    stmt->right = expr;
    return stmt;
  }
  return NULL;
}

struct Node *ParseStmt() {
  struct Node *stmt;
  if ((stmt = ParseExprStmt()) || (stmt = ParseJumpStmt()) ||
      (stmt = ParseSelectionStmt()) || (stmt = ParseCompStmt()))
    return stmt;
  return NULL;
}

struct Node *ParseDeclSpecs() {
  struct Node *decl_spec;
  (decl_spec = ConsumeToken(kTokenKwInt)) ||
      (decl_spec = ConsumeToken(kTokenKwChar)) ||
      (decl_spec = ConsumeToken(kTokenKwVoid));
  if (decl_spec) return decl_spec;
  if (ConsumeToken(kTokenKwStruct)) {
    struct Node *struct_spec = AllocNode(kASTStructSpec);
    struct_spec->tag = ConsumeToken(kTokenIdent);
    assert(struct_spec->tag);
    return struct_spec;
  }
  return NULL;
}

struct Node *ParseParamDecl();
struct Node *ParseDecltor();
struct Node *ParseDirectDecltor() {
  // always allow abstract decltors
  struct Node *n = NULL;
  struct Node *t;
  if ((t = ConsumePunctuator("("))) {
    n = AllocNode(kASTDirectDecltor);
    n->op = t;
    n->value = ParseDecltor();
    assert(n->value);
    ExpectPunctuator(")");
  } else if ((t = ConsumeToken(kTokenIdent))) {
    n = AllocNode(kASTDirectDecltor);
    n->op = t;
  }
  while (true) {
    if ((t = ConsumePunctuator("("))) {
      struct Node *args = AllocList();
      if (!ConsumePunctuator(")")) {
        while (1) {
          struct Node *arg = ParseParamDecl();
          if (!arg) ErrorWithToken(NextToken(), "Expected ParamDecl here");
          PushToList(args, arg);
          if (!ConsumePunctuator(",")) break;
        }
        ExpectPunctuator(")");
      }
      struct Node *nn = AllocNode(kASTDirectDecltor);
      nn->op = t;
      nn->right = args;
      nn->left = n;
      n = nn;
    }
    break;
  }
  return n;
}

struct Node *ParseDecltor() {
  struct Node *n = AllocNode(kASTDecltor);
  struct Node *pointer = NULL;
  struct Node *t;
  while ((t = ConsumePunctuator("*"))) {
    pointer = CreateTypePointer(pointer);
  }
  n->left = pointer;
  n->right = ParseDirectDecltor();
  return n;
}

struct Node *ParseParamDecl() {
  struct Node *decl_spec = ParseDeclSpecs();
  if (!decl_spec) return NULL;
  struct Node *n = AllocNode(kASTDecl);
  n->op = decl_spec;
  n->right = ParseDecltor();
  return n;
}

struct Node *ParseDeclBody() {
  struct Node *decl_spec = ParseDeclSpecs();
  if (!decl_spec) return NULL;
  struct Node *n = AllocNode(kASTDecl);
  n->op = decl_spec;
  n->right = ParseDecltor();
  return n;
}

struct Node *ParseDecl() {
  struct Node *decl_body = ParseDeclBody();
  if (!decl_body) return NULL;
  ExpectPunctuator(";");
  return decl_body;
}

struct Node *ParseCompStmt() {
  struct Node *t;
  if (!(t = ConsumePunctuator("{"))) return NULL;
  struct Node *list = AllocList();
  list->op = t;
  struct Node *stmt;
  while ((stmt = ParseDecl()) || (stmt = ParseStmt())) {
    PushToList(list, stmt);
  }
  ExpectPunctuator("}");
  return list;
}

struct Node *ParseFuncDef(struct Node *decl_body) {
  struct Node *comp_stmt = ParseCompStmt();
  if (!comp_stmt) return NULL;
  return CreateASTFuncDef(decl_body, comp_stmt);
}

struct Node *toplevel_names;

struct Node *Parse(struct Node *passed_tokens) {
  tokens = passed_tokens;
  token_stream_index = 0;
  struct Node *list = AllocList();
  struct Node *decl_body;
  toplevel_names = AllocList();
  while ((decl_body = ParseDeclBody())) {
    if (ConsumePunctuator(";")) {
      struct Node *type = CreateTypeFromDecl(decl_body);
      PushKeyValueToList(toplevel_names, CreateTokenStr(type->left),
                         GetTypeWithoutAttr(type));
      continue;
    }
    struct Node *func_def = ParseFuncDef(decl_body);
    if (!func_def) {
      ErrorWithToken(NextToken(), "Unexpected token");
    }
    PushToList(list, func_def);
    PushKeyValueToList(toplevel_names,
                       CreateTokenStr(func_def->func_name_token),
                       func_def->func_type);
  }
  struct Node *t;
  if (!(t = NextToken())) return list;
  ErrorWithToken(t, "Unexpected token");
}
