//===--- Syntax.cpp - Swift Syntax Implementation -------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/Syntax/Syntax.h"
#include "swift/Syntax/SyntaxData.h"
#include "swift/Syntax/SyntaxVisitor.h"

using namespace swift;
using namespace swift::syntax;

RC<RawSyntax> Syntax::getRaw() const {
  return Data->getRaw();
}

SyntaxKind Syntax::getKind() const {
  return getRaw()->getKind();
}

void Syntax::print(llvm::raw_ostream &OS, SyntaxPrintOptions Opts) const {
  getRaw()->print(OS, Opts);
}

void Syntax::dump() const {
  getRaw()->dump();
}

void Syntax::dump(llvm::raw_ostream &OS, unsigned Indent) const {
  getRaw()->dump(OS, 0);
}

bool Syntax::isType() const {
  return Data->isType();
}

bool Syntax::isDecl() const {
  return Data->isDecl();
}

bool Syntax::isStmt() const {
  return Data->isStmt();
}

bool Syntax::isExpr() const {
  return Data->isExpr();
}

bool Syntax::isToken() const {
  return getRaw()->isToken();
}

bool Syntax::isPattern() const {
  return Data->isPattern();
}

bool Syntax::isUnknown() const {
  return Data->isUnknown();
}

bool Syntax::isPresent() const {
  return getRaw()->isPresent();
}

bool Syntax::isMissing() const {
  return getRaw()->isMissing();
}

llvm::Optional<Syntax> Syntax::getParent() const {
  auto ParentData = getData().Parent;
  if (ParentData == nullptr) return llvm::None;
  return llvm::Optional<Syntax> {
    Syntax { Root, ParentData }
  };
}

size_t Syntax::getNumChildren() const {
  size_t NonTokenChildren = 0;
  for (auto Child : getRaw()->getLayout()) {
    if (!Child->isToken()) {
      ++NonTokenChildren;
    }
  }
  return NonTokenChildren;
}

Syntax Syntax::getChild(const size_t N) const {
  // The actual index of the Nth non-token child.
  size_t ActualIndex = 0;
  // The number of non-token children we've seen.
  size_t NumNonTokenSeen = 0;
  for (auto Child : getRaw()->getLayout()) {
    // If we see a child that's not a token, count it.
    if (!Child->isToken()) {
      ++NumNonTokenSeen;
    }
    // If the number of children we've seen indexes the same (count - 1) as
    // the number we're looking for, then we're done.
    if (NumNonTokenSeen == N + 1) { break; }

    // Otherwise increment the actual index and keep searching.
    ++ActualIndex;
  }
  return Syntax { Root, Data->getChild(ActualIndex).get() };
}

AbsolutePosition Syntax::getAbsolutePosition(SourceFileSyntax Root) const {
  AbsolutePosition Pos;

  /// This visitor collects all of the nodes before this node to calculate its
  /// offset from the begenning of the file.
  class Visitor: public SyntaxVisitor {
    AbsolutePosition &Pos;
    RawSyntax *Target;
    bool Found = false;

  public:
    Visitor(AbsolutePosition &Pos, RawSyntax *Target): Pos(Pos),
                                                       Target(Target) {}
    ~Visitor() { assert(Found); }
    void visitPre(Syntax Node) override {
      // Check if this node is the target;
      Found |= Node.getRaw().get() == Target;
    }
    void visit(TokenSyntax Node) override {
      // Ignore missing node and ignore the nodes after this node.
      if (Found || Node.isMissing())
        return;
      // Collect all the offsets.
      Node.getRaw()->accumulateAbsolutePosition(Pos);
    }
  } Calculator(Pos, getRaw().get());

  /// This visitor visit the first token node of this node to accumulate its
  /// leading trivia. Therefore, the calculated absolute location will point
  /// to the actual token start.
  class FirstTokenFinder: public SyntaxVisitor {
    AbsolutePosition &Pos;
    bool Found = false;

  public:
    FirstTokenFinder(AbsolutePosition &Pos): Pos(Pos) {}
    void visit(TokenSyntax Node) override {
      if (Found || Node.isMissing())
        return;
      Found = true;
      for (auto &Leader : Node.getRaw()->getLeadingTrivia())
        Leader.accumulateAbsolutePosition(Pos);
    }
  } FTFinder(Pos);

  // Visit the root to get all the nodes before this node.
  Root.accept(Calculator);

  // Visit this node to accumulate the leading trivia of its first token.
  const_cast<Syntax*>(this)->accept(FTFinder);
  return Pos;
}
