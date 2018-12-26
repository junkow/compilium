#include "compilium.h"

int IsSameType(struct Node *a, struct Node *b) {
  assert(a && b);
  if (a->type != b->type) return 0;
  if (a->type == kTypeBase) {
    assert(a->op && b->op);
    return a->op->type == b->op->type;
  } else if (a->type == kTypeLValue || a->type == kTypePointer) {
    return IsSameType(a->right, b->right);
  }
  Error("IsSameType: Comparing non-type nodes");
}

struct Node *GetRValueType(struct Node *t) {
  if (!t) return NULL;
  if (t->type != kTypeLValue) return t;
  return t->right;
}

int IsAssignable(struct Node *dst, struct Node *src) {
  assert(dst && src);
  if (dst->type != kTypeLValue) return 0;
  return IsSameType(GetRValueType(dst), src);
}

int GetSizeOfType(struct Node *t) {
  t = GetRValueType(t);
  assert(t);
  if (t->type == kTypeBase) {
    assert(t->op);
    if (t->op->type == kTokenKwInt) return 4;
    if (t->op->type == kTokenKwChar) return 1;
  } else if (t->type == kTypePointer) {
    return 8;
  }
  assert(false);
}

struct Node *CreateType(struct Node *decl_spec, struct Node *decltor);
struct Node *CreateTypeFromDecltor(struct Node *decltor, struct Node *type) {
  assert(decltor);
  struct Node *pointer = decltor->left;
  if (pointer) {
    struct Node *p = pointer;
    while (p->right) {
      p = p->right;
    }
    p->right = type;
    type = pointer;
  }
  struct Node *direct_decltor = decltor->right;
  assert(direct_decltor->type == kASTDirectDecltor);
  if (IsEqualTokenWithCStr(direct_decltor->op, "(")) {
    struct Node *arg_type_list = AllocList();
    if (direct_decltor->right) {
      PushToList(arg_type_list, CreateType(direct_decltor->right->op,
                                           direct_decltor->right->right));
    }
    return CreateTypeFunction(type, arg_type_list);
  }
  if (direct_decltor->op) {
    type->value = CreateASTNodeIdent(direct_decltor->op);
    return type;
  }
  assert(false);
}

struct Node *CreateType(struct Node *decl_spec, struct Node *decltor) {
  struct Node *type = CreateTypeBase(decl_spec);
  if (!decltor) return type;
  return CreateTypeFromDecltor(decltor, type);
}

void TestType() {
  fprintf(stderr, "Testing Type...");

  struct Node *int_type = CreateTypeBase(CreateToken("int"));
  struct Node *another_int_type = CreateTypeBase(CreateToken("int"));
  struct Node *lvalue_int_type = CreateTypeLValue(int_type);
  struct Node *pointer_of_int_type = CreateTypePointer(int_type);
  struct Node *another_pointer_of_int_type =
      CreateTypePointer(another_int_type);

  assert(IsSameType(int_type, int_type));
  assert(IsSameType(int_type, another_int_type));
  assert(!IsSameType(int_type, lvalue_int_type));
  assert(IsSameType(lvalue_int_type, lvalue_int_type));
  assert(!IsSameType(int_type, pointer_of_int_type));
  assert(IsSameType(pointer_of_int_type, another_pointer_of_int_type));

  assert(GetSizeOfType(int_type) == 4);
  assert(GetSizeOfType(pointer_of_int_type) == 8);

  fprintf(stderr, "PASS\n");
  exit(EXIT_SUCCESS);
}
