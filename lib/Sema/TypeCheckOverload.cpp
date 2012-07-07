//===--- TypeCheckOverload.cpp - Overload Resolution ----------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file implements overload resolution.
//
//===----------------------------------------------------------------------===//
#include "TypeChecker.h"
#include "swift/AST/Attr.h"
#include "swift/AST/NameLookup.h"
#include "swift/AST/Types.h"
using namespace swift;

static Identifier getFirstOverloadedIdentifier(const Expr *Fn) {
  if (const DeclRefExpr *DR = dyn_cast<DeclRefExpr>(Fn))
    return DR->getDecl()->getName();
  const OverloadSetRefExpr *ODR = cast<OverloadSetRefExpr>(Fn);
  auto Decls = ODR->getDecls();
  assert(!Decls.empty());
  return (*Decls.begin())->getName();
}

static bool displayOperandType(Type T) {
  return !T->isUnresolvedType() && !isa<ErrorType>(T);
}

void TypeChecker::diagnoseEmptyOverloadSet(Expr *E,
                                           ArrayRef<ValueDecl *> Candidates) {
  if (const BinaryExpr *BE = dyn_cast<BinaryExpr>(E)) {
    // FIXME: this feels a bit ad hoc with how we dig through the AST, and
    // it possibly makes assumptions that aren't true or I don't understand.
    // Some of this structure would feel nice to put back into the AST
    // itself.
    const Expr *Fn = BE->getFn();
    const TupleExpr *Arg = cast<TupleExpr>(BE->getArg());
    auto Elements = Arg->getElements();
    SourceLoc L = Fn->getLoc();
    Identifier I = getFirstOverloadedIdentifier(Fn);
    // Issue an error indicating the types of the operands, but only do
    // so if they are both unresolved types and not "error types".
    Type TypeA = Elements[0]->getType();
    Type TypeB = Elements[1]->getType();
    if (displayOperandType(TypeA) && displayOperandType(TypeB)) {
      diagnose(L, diag::no_candidates_binop, I, TypeA, TypeB)
        << Elements[0]->getSourceRange() << Elements[1]->getSourceRange();
    }
    else {
      diagnose(L, diag::no_candidates_op, 0, I)
        << Elements[0]->getSourceRange() << Elements[1]->getSourceRange();
    }
  }
  else if (isa<PrefixUnaryExpr>(E) || isa<PostfixUnaryExpr>(E)) {
    const ApplyExpr *UE = cast<ApplyExpr>(E);
    // FIXME: this feels a bit ad hoc with how we dig through the AST, and
    // it possibly makes assumptions that aren't true or I don't understand.
    // Some of this structure would feel nice to put back into the AST
    // itself.
    const Expr *Fn = UE->getFn();
    const Expr *Arg = UE->getArg();
    Identifier I = getFirstOverloadedIdentifier(Fn);
    // Issue a note indicating the types of the operand, but only do
    // so if it is a unresolved type.  Otherwise, the diagnostic is confusing.
    Type TypeArg = Arg->getType();
    if (displayOperandType(TypeArg)) {      
      diagnose(Arg->getLoc(), diag::no_candidates_unary, I, TypeArg)
        << Arg->getSourceRange();
    }
    else {
      diagnose(Arg->getLoc(), diag::no_candidates_op, 1, I)
        << Arg->getSourceRange();
    }
  }
  else {
    diagnose(E->getLoc(), diag::no_candidates)
      << E->getSourceRange();
  }
  printOverloadSetCandidates(Candidates);
}

void TypeChecker::printOverloadSetCandidates(ArrayRef<ValueDecl *> Candidates) {
  for (auto TheDecl : Candidates)
    diagnose(TheDecl->getStartLoc(), diag::found_candidate);
}

/// \brief Form a version of the type \c T with all of the given generic
/// parameters replaced by deducible parameters.
///
/// \param DeducibleParams Will be populated with the synthesized deducible
/// generic parameters.
static Type
getDeducibleType(TypeChecker &TC, Type T,
                 ArrayRef<GenericParam> GenericParams,
                 SmallVectorImpl<DeducibleGenericParamType *> &DeducibleParams){
  TypeSubstitutionMap PolySubstitutions;
  SmallVector<DeducibleGenericParamType *, 2> DeducibleTypes;
  DeducibleParams.reserve(GenericParams.size());
  unsigned Index = 0;
  for (auto GP : GenericParams) {
    TypeAliasDecl *TypeParam = GP.getAsTypeParam();
    auto Archetype = TypeParam->getDeclaredType()->getAs<ArchetypeType>();
    auto Deducible = DeducibleGenericParamType::getNew(TC.Context,
                                                       TypeParam->getName(),
                                                       Index++,
                                                    Archetype->getConformsTo());
    DeducibleParams.push_back(Deducible);
    PolySubstitutions[Archetype] = Deducible;
  }

  return TC.substType(T, PolySubstitutions);
}

/// checkPolymorphicApply - Check the application of a function of the given
/// polymorphic type to a particular argument with, optionally, a destination
/// type.
OverloadCandidate
TypeChecker::checkPolymorphicApply(PolymorphicFunctionType *PolyFn,
                                   bool Assignment,
                                   Expr *Arg,
                                   Type DestTy) {
  // Set up a coercion context to deduce the parameters to the polymorphic
  // function.
  CoercionContext CC(*this);
  SmallVector<DeducibleGenericParamType *, 2> DeducibleParams;
  AnyFunctionType *FunctionTy
    = getDeducibleType(*this, PolyFn,
                       PolyFn->getGenericParams().getParams(), DeducibleParams)
        ->castTo<AnyFunctionType>();
  CC.requestSubstitutionsFor(DeducibleParams);

  // Check whether arguments are suitable for this function.
  CoercionKind Kind = CoercionKind::Normal;
  if (Assignment)
    Kind = CoercionKind::Assignment;
  if (isCoercibleToType(Arg, FunctionTy->getInput(), Kind, &CC)
        == CoercionResult::Failed)
    return OverloadCandidate();

  // Check whether we can coerce the result type.
  if (DestTy) {
    OpaqueValueExpr OVE(Arg->getLoc(), FunctionTy->getResult());
    if (isCoercibleToType(&OVE, DestTy, CoercionKind::Normal, &CC)
          == CoercionResult::Failed)
      return OverloadCandidate();
  }

  if (!CC.hasCompleteSubstitutions())
    return OverloadCandidate(nullptr, PolyFn, false);

  // We have a complete set of substitutions. Apply them.
  Type SubstFunctionTy = substType(FunctionTy, CC.Substitutions);
  if (!SubstFunctionTy)
    return OverloadCandidate();

  FunctionTy = SubstFunctionTy->castTo<AnyFunctionType>();

  // We stared with a polymorphic function type, and have substituted away
  // all of the generic parameters. Turn it into the corresponding
  // monomorphic function type.
  FunctionTy = FunctionType::get(FunctionTy->getInput(),
                                 FunctionTy->getResult(),
                                 Context);

  OverloadCandidate::SubstitutionInfoType SubstitutionInfo
    = { std::move(CC.Substitutions), std::move(CC.Conformance) };
  return OverloadCandidate(nullptr, FunctionTy, std::move(SubstitutionInfo));
}

OverloadCandidate
TypeChecker::filterOverloadSet(ArrayRef<ValueDecl *> Candidates,
                               bool OperatorSyntax,
                               Type BaseTy,
                               Expr *Arg,
                               Type DestTy,
                               SmallVectorImpl<ValueDecl *> &Viable) {
  SmallVector<OverloadCandidate, 4> ViableCandidates;
  bool hasThis = BaseTy && !BaseTy->is<MetaTypeType>();
  for (ValueDecl *VD : Candidates) {
    Type VDType = VD->getTypeOfReference()->getRValueType();

    // Must have function type to be called.
    AnyFunctionType *FunctionTy = VDType->getAs<AnyFunctionType>();
    if (!FunctionTy)
      continue;

    // If we have a 'this' argument and the declaration is a non-static method,
    // the method's 'this' parameter has already been bound. Look instead at the
    // actual argument types.
    if (hasThis && ((isa<FuncDecl>(VD) && !cast<FuncDecl>(VD)->isStatic()) ||
                   isa<ConstructorDecl>(VD))) {
      // FIXME: Derived-to-base conversions will eventually be needed.
      FunctionTy = FunctionTy->getResult()->castTo<AnyFunctionType>();
    }

    // Substitute into the type of this member, if indeed it is a member.
    Type SubstFunctionTy = substMemberTypeWithBase(FunctionTy, BaseTy);
    if (!SubstFunctionTy)
      continue;
    FunctionTy = SubstFunctionTy->castTo<AnyFunctionType>();

    // Establish the coercion context, which is required when this coercion
    // involves generic functions.
    CoercionContext CC(*this);
    if (auto polyFn = FunctionTy->getAs<PolymorphicFunctionType>()) {
      // Create a deducible form of the function type.
      SmallVector<DeducibleGenericParamType *, 2> DeducibleParams;
      FunctionTy = getDeducibleType(*this, FunctionTy,
                                    polyFn->getGenericParams().getParams(),
                                    DeducibleParams)
                     ->castTo<AnyFunctionType>();
      CC.requestSubstitutionsFor(DeducibleParams);
    }
    // As a temporary hack, manually introduce the substitutions for operators
    // which are members of protocols.  (This will go away once we start using
    // PolymorphicFunctionTypes for protocol members.)
    if (VD->getName().isOperator()) {
      if (Type Extension = cast<FuncDecl>(VD)->getExtensionType()) {
        if (ProtocolType *P = Extension->getAs<ProtocolType>()) {
          SmallVector<GenericParam, 4> Params;
          for (auto m : P->getDecl()->getMembers()) {
            if (TypeAliasDecl *TAD = dyn_cast<TypeAliasDecl>(m))
              Params.push_back(TAD);
          }

          SmallVector<DeducibleGenericParamType *, 2> DeducibleParams;
          FunctionTy = getDeducibleType(*this, FunctionTy, Params,
                                        DeducibleParams)
                         ->castTo<AnyFunctionType>();
          CC.requestSubstitutionsFor(DeducibleParams);
        }
      }
    }

    // Check whether arguments are suitable for this function.
    CoercionKind Kind = CoercionKind::Normal;
    if (OperatorSyntax && VD->getAttrs().isAssignment())
      Kind = CoercionKind::Assignment;
    if (isCoercibleToType(Arg, FunctionTy->getInput(), Kind, &CC)
          == CoercionResult::Failed)
      continue;

    // Check whether we can coerce the result type.
    if (DestTy) {
      OpaqueValueExpr OVE(Arg->getLoc(), FunctionTy->getResult());
      if (isCoercibleToType(&OVE, DestTy, CoercionKind::Normal, &CC)
            == CoercionResult::Failed)
        continue;
    }

    // Add without substitutions.
    if (!CC.requiresSubstitution() || !CC.hasCompleteSubstitutions()) {
      ViableCandidates.push_back(
        OverloadCandidate(VD, FunctionTy, CC.hasCompleteSubstitutions()));
      continue;
    }

    // We have a complete set of substitutions. Apply them.
    SubstFunctionTy = substType(FunctionTy, CC.Substitutions);
    if (!SubstFunctionTy)
      continue;

    FunctionTy = SubstFunctionTy->castTo<AnyFunctionType>();

    // We stared with a polymorphic function type, and have substituted away
    // all of the generic parameters. Turn it into the corresponding
    // monomorphic function type.
    FunctionTy = FunctionType::get(FunctionTy->getInput(),
                                   FunctionTy->getResult(),
                                   Context);

    // Add with substitutions.
    OverloadCandidate::SubstitutionInfoType SubstitutionInfo
      = { std::move(CC.Substitutions), std::move(CC.Conformance) };
    ViableCandidates.push_back(OverloadCandidate(VD, FunctionTy,
                                                 std::move(SubstitutionInfo)));
  }

  // If we found a fully-resolved a viable candidate, we're done.
  if (ViableCandidates.size() == 1 && ViableCandidates[0].isComplete())
    return std::move(ViableCandidates[0]);

  // Create the resulting viable-candidates vector.
  Viable.clear();
  for (auto const& VC : ViableCandidates)
    Viable.push_back(VC.getDecl());

  return OverloadCandidate();
}

OverloadCandidate
TypeChecker::filterOverloadSetForValue(ArrayRef<ValueDecl *> Candidates,
                                       SourceLoc Loc,
                                       Type BaseTy,
                                       Type DestTy,
                                       SmallVectorImpl<ValueDecl *> &Viable,
                                       CoercionContext *CC) {
  SmallVector<OverloadCandidate, 4> ViableCandidates;
  bool hasThis = BaseTy && !BaseTy->is<MetaTypeType>();

  auto DestLV = DestTy->getAs<LValueType>();
  if (DestLV)
    DestTy = DestLV->getObjectType();

  for (ValueDecl *VD : Candidates) {
    Type SrcTy = VD->getTypeOfReference();
    auto SrcLV = SrcTy->getAs<LValueType>();

    // Deal with destination lvalues.
    if (DestLV) {
      // If the source isn't an lvalue, skip it.
      // FIXME: Improve recovery.
      if (!SrcLV)
        continue;

      // If the source is more qualified than the destination, skip it.
      // FIXME: Improve recovery.
      if (SrcLV->getQualifiers() > DestLV->getQualifiers())
        continue;

      SrcTy = SrcLV->getObjectType();
    }
    
    // If we have a 'this' type and the declaration is a constructor or
    // non-static method, look past the 'this' argument.
    if (hasThis &&
        ((isa<FuncDecl>(VD) && !cast<FuncDecl>(VD)->isStatic()) ||
         isa<ConstructorDecl>(VD))) {
      // FIXME: Derived-to-base conversions will eventually be needed.
      SrcTy = SrcTy->castTo<AnyFunctionType>()->getResult();
    }

    // Substitute into the type of this member, if indeed it is a member.
    SrcTy = substMemberTypeWithBase(SrcTy, BaseTy);
    if (!SrcTy)
      continue;

    // Establish the coercion context, which is required when this coercion
    // involves generic functions.
    CoercionContext LocalCC(*this);

    // If the source is a polymorphic function type, deduce its generic
    // parameters.
    Type EffectiveSrcTy = SrcTy;
    if (auto polyFn = SrcTy->getAs<PolymorphicFunctionType>()) {
      // Create a deducible form of the function type.
      SmallVector<DeducibleGenericParamType *, 2> DeducibleParams;
      SrcTy = getDeducibleType(*this, SrcTy,
                               polyFn->getGenericParams().getParams(),
                               DeducibleParams);
      LocalCC.requestSubstitutionsFor(DeducibleParams);

      // Create a non-polymorphic function type against which we can perform
      // the match.
      SrcTy = FunctionType::get(SrcTy->castTo<AnyFunctionType>()->getInput(),
                                SrcTy->castTo<AnyFunctionType>()->getResult(),
                                Context);

      // Match against the generic type.
      if (DestLV) {
        // Check for trivial subtyping, to deduce any generic parameters
        // that require deduction. We'll do a specific type check later,
        // after substitution.
        bool Trivial = true;
        if (!isSubtypeOf(SrcTy, DestTy, Trivial, &LocalCC) || !Trivial)
          continue;
      } else {
        OpaqueValueExpr OVE(Loc, SrcTy);
        if (isCoercibleToType(&OVE, DestTy, CoercionKind::Normal, &LocalCC)
              == CoercionResult::Failed) {
          continue;
        }
      }

      // If we don't have a complete set of substitutions, we don't know if
      // this matches.
      if (!LocalCC.hasCompleteSubstitutions()) {
        ViableCandidates.push_back(OverloadCandidate(VD, SrcTy, false));
        continue;
      }

      // Substitute the deduced types into the source type.
      EffectiveSrcTy = substType(SrcTy, LocalCC.Substitutions);
      if (!EffectiveSrcTy)
        continue;
    }

    // If either the source or destination type is still unresolved,
    // we may need to deduce arguments.
    CoercionContext GlobalCC(*this);
    // Establish a context in which we will compute substitutions.
    if (CC && CC->requiresSubstitution()) {
      GlobalCC.Substitutions = CC->Substitutions;
      GlobalCC.Conformance = CC->Conformance;
    }

    Type EffectiveDestTy = DestTy;
    // FIXME: Seems like this should only kick in when either destination or
    // source is still unresolved.
    if (GlobalCC.requiresSubstitution()) {
      // Deduce arguments.
      if (DestLV) {
        bool Trivial = true;
        if (!isSubtypeOf(EffectiveSrcTy, DestTy, Trivial, &GlobalCC) ||
            !Trivial)
          continue;
      } else {
        OpaqueValueExpr OVE(Loc, EffectiveSrcTy);
        if (isCoercibleToType(&OVE, DestTy, CoercionKind::Normal, &GlobalCC)
              == CoercionResult::Failed) {
          continue;
        }
      }

      // If we don't have a complete set of substitutions, we don't know if
      // this matches.
      if (!GlobalCC.hasCompleteSubstitutions()) {
        ViableCandidates.push_back(OverloadCandidate(VD, EffectiveSrcTy,
                                                     false));
        continue;
      }

      EffectiveSrcTy = substType(EffectiveSrcTy, GlobalCC.Substitutions);
      if (!EffectiveSrcTy)
        continue;
      EffectiveDestTy = substType(EffectiveDestTy, GlobalCC.Substitutions);
      if (!EffectiveDestTy)
        continue;
    }

    if (DestLV) {
      // Simply checking for type equality suffices.
      // FIXME: Want to use TypeCoercion's sameType() here?
      if (!EffectiveSrcTy->isEqual(EffectiveDestTy))
        continue;
    } else {
    // Check that the source is coercible to the destination.
      OpaqueValueExpr OVE(Loc, EffectiveSrcTy);
      if (isCoercibleToType(&OVE, EffectiveDestTy, CoercionKind::Normal,
                            &GlobalCC)
            == CoercionResult::Failed) {
        continue;
      }
    }

    // Compute the type of the reference, which may be an lvalue.
    Type ResultTy = EffectiveSrcTy;
    if (SrcLV)
      ResultTy = LValueType::get(ResultTy, SrcLV->getQualifiers(), Context);

    if (LocalCC.requiresSubstitution()) {
      OverloadCandidate::SubstitutionInfoType SubstitutionInfo
        = { std::move(LocalCC.Substitutions), std::move(LocalCC.Conformance) };
      ViableCandidates.push_back(
        OverloadCandidate(VD, ResultTy, std::move(SubstitutionInfo)));
    } else {
      ViableCandidates.push_back(OverloadCandidate(VD, ResultTy, true));
    }
  }

  // If we found a fully-resolved a viable candidate, we're done.
  if (ViableCandidates.size() == 1 && ViableCandidates[0].isComplete())
    return std::move(ViableCandidates[0]);

  // Create the resulting viable-candidates vector.
  Viable.clear();
  for (auto const& VC : ViableCandidates)
    Viable.push_back(VC.getDecl());
  
  return OverloadCandidate();
}

OverloadedExpr TypeChecker::getOverloadedExpr(Expr *E) {
  Expr *expr = E->getSemanticsProvidingExpr();

  // Handle overloaded set references.
  if (auto OSE = dyn_cast<OverloadSetRefExpr>(expr)) {
    return OverloadedExpr(E, OSE->getBaseType(), OSE->getDecls());
  }

  // Only expressions with polymorphic function type can be overloaded beyond
  // this point.
  if (!expr->getType()->is<PolymorphicFunctionType>()) {
    return OverloadedExpr();
  }

  // Handle expressions that refer to a given overloadable declaration.
  if (auto DRE = dyn_cast<DeclRefExpr>(expr)) {
    return OverloadedExpr(E, Type(), DRE->getDecl());
  }
  if (auto AMR = dyn_cast<ArchetypeMemberRefExpr>(expr)) {
    return OverloadedExpr(E, AMR->getBase()->getType(), AMR->getDecl());
  }
  if (auto ASR = dyn_cast<ArchetypeSubscriptExpr>(expr)) {
    return OverloadedExpr(E, ASR->getBase()->getType(), ASR->getDecl());
  }
  if (auto EMR = dyn_cast<ExistentialMemberRefExpr>(expr)) {
    return OverloadedExpr(E, EMR->getBase()->getType(), EMR->getDecl());
  }
  if (auto ESR = dyn_cast<ExistentialSubscriptExpr>(expr)) {
    return OverloadedExpr(E, ESR->getBase()->getType(), ESR->getDecl());
  }

  // There is no declaration here to overload on.
  return OverloadedExpr();
}

/// \brief Replace the original semantics-providing expression (\c Orig) with
/// a new expression (\c New) within the given expression (\c E).
///
/// This also clears out the types for all of the 'sugar' expressions between
/// \c E and \c Orig, since they will need to be type-checked again.
static Expr *replaceSemanticsProvidingExpr(Expr *Orig, Expr *New, Expr *E) {
  if (Orig == E)
    return New;

  if (auto parens = dyn_cast<ParenExpr>(E)) {
    Expr *sub = replaceSemanticsProvidingExpr(Orig, New, parens->getSubExpr());
    parens->setSubExpr(sub);
    parens->setType(sub->getType());
    return E;
  }

  llvm_unreachable("Unhandled expression sugar");
}

Expr *TypeChecker::buildFilteredOverloadSet(OverloadedExpr Ovl,
                                            ArrayRef<ValueDecl *> Remaining) {
  Expr *expr = Ovl.getExpr()->getSemanticsProvidingExpr();

  // Create an appropriate expression to refer to the remaining set of
  // declarations.
  Expr *result;
  if (auto DRE = dyn_cast<OverloadedDeclRefExpr>(expr)) {
    result = buildRefExpr(Remaining, DRE->getLoc());
  } else if (auto MRE = dyn_cast<OverloadedMemberRefExpr>(expr)) {
    result = buildMemberRefExpr(MRE->getBase(), MRE->getDotLoc(),
                                Remaining, MRE->getMemberLoc());
    if (!result)
      return nullptr;

    result = recheckTypes(result);
    if (!result)
      return nullptr;
  } else {
    assert((isa<DeclRefExpr>(expr) || isa<MemberRefExpr>(expr) ||
            isa<ExistentialMemberRefExpr>(expr) ||
            isa<ArchetypeMemberRefExpr>(expr)) &&
           "Not a declaration reference expression");
    assert(Remaining.size() == 1);
    return Ovl.getExpr();
  }

  // Replace the semantics-providing expression with the newly-built result.
  return replaceSemanticsProvidingExpr(expr, result, Ovl.getExpr());
}

Expr *
TypeChecker::buildFilteredOverloadSet(OverloadedExpr Ovl,
                                      const OverloadCandidate &Candidate){
  assert(Candidate.isComplete() && "Incomplete overload candidate!");

  Expr *expr = Ovl.getExpr()->getSemanticsProvidingExpr();

  Expr *result = nullptr;
  if (auto DRE = dyn_cast<OverloadedDeclRefExpr>(expr)) {
    result = new (Context) DeclRefExpr(Candidate.getDecl(), DRE->getLoc(),
                             Candidate.getDecl()->getTypeOfReference());
    result = recheckTypes(result);
    if (!result)
      return nullptr;
  } else if (auto MRE = dyn_cast<OverloadedMemberRefExpr>(expr)) {
    SmallVector<ValueDecl *, 1> decls(1, Candidate.getDecl());
    result = recheckTypes(buildMemberRefExpr(MRE->getBase(), MRE->getDotLoc(),
                                             decls, MRE->getMemberLoc()));
    if (!result)
      return nullptr;
  } else {
    assert((isa<DeclRefExpr>(expr) || isa<MemberRefExpr>(expr) ||
            isa<ExistentialMemberRefExpr>(expr) ||
            isa<ArchetypeMemberRefExpr>(expr)) &&
           "Not a declaration reference expression");
    result = expr;
  }

  // If matching the candidate required substitutions, introduce a
  // specialization expression that encodes those substitutions and provides
  // a concrete type.
  if (Candidate.hasSubstitutions()) {
    SmallVector<SpecializeExpr::Substitution, 2> Substitutions;
    Substitutions.resize(Candidate.getSubstitutions().size());
    auto &Conformances = Candidate.getConformances();
    for (auto S : Candidate.getSubstitutions()) {
      unsigned Index = S.first->getPrimaryIndex();
      Substitutions[Index].Replacement = S.second;
      Substitutions[Index].Conformance
        = Context.AllocateCopy(Conformances[S.first]);
    }
    result = new (Context) SpecializeExpr(result, Candidate.getType(),
                                          Context.AllocateCopy(Substitutions));
  }

  // Replace the semantics-providing expression with the newly-built result.
  return replaceSemanticsProvidingExpr(expr, result, Ovl.getExpr());
}

Expr *TypeChecker::buildRefExpr(ArrayRef<ValueDecl *> Decls, SourceLoc NameLoc) {
  assert(!Decls.empty() && "Must have at least one declaration");

  if (Decls.size() == 1) {
    return new (Context) DeclRefExpr(Decls[0], NameLoc,
                                     Decls[0]->getTypeOfReference());
  }

  Decls = Context.AllocateCopy(Decls);
  return new (Context) OverloadedDeclRefExpr(Decls, NameLoc,
                         UnstructuredUnresolvedType::get(Context));
}

Expr *TypeChecker::buildRefExpr(const OverloadCandidate &Candidate,
                                SourceLoc NameLoc) {
  auto decl = Candidate.getDecl();
  Expr *result = new (Context) DeclRefExpr(decl, NameLoc,
                                           decl->getTypeOfReference());

  // If matching the candidate required substitutions, introduce a
  // specialization expression that encodes those substitutions and provides
  // a concrete type.
  if (Candidate.hasSubstitutions()) {
    SmallVector<SpecializeExpr::Substitution, 2> Substitutions;
    Substitutions.resize(Candidate.getSubstitutions().size());
    auto &Conformances = Candidate.getConformances();
    for (auto S : Candidate.getSubstitutions()) {
      unsigned Index = S.first->getPrimaryIndex();
      Substitutions[Index].Replacement = S.second;
      Substitutions[Index].Conformance
        = Context.AllocateCopy(Conformances[S.first]);
    }
    result = new (Context) SpecializeExpr(result, Candidate.getType(),
                                          Context.AllocateCopy(Substitutions));
  }

  return recheckTypes(result);
}

Expr *TypeChecker::buildMemberRefExpr(Expr *Base, SourceLoc DotLoc,
                                      ArrayRef<ValueDecl *> Decls,
                                      SourceLoc MemberLoc) {
  assert(!Decls.empty() && "Must have at least one declaration");
  
  // Figure out the actual base type, and whether we have an instance of that
  // type or its metatype.
  Type baseTy = Base->getType()->getRValueType();
  bool baseIsInstance = true;
  if (auto baseMeta = baseTy->getAs<MetaTypeType>()) {
    baseIsInstance = false;
    baseTy = baseMeta->getInstanceType();
  }

  // Check whether the first declaration is valid. If it is, they're all
  // potential candidates, because we don't allow overloading across different
  // classes of entities (e.g., variables and types cannot be overloaded).
  // If not, complain now.
  if (!baseIsInstance && isa<VarDecl>(Decls[0])) {
    diagnose(MemberLoc, diag::member_ref_metatype_variable,
             Decls[0]->getName(), baseTy);
    diagnose(Decls[0]->getLoc(), diag::decl_declared_here, Decls[0]->getName());

    Expr *BadExpr = new (Context) UnresolvedDotExpr(Base, DotLoc,
                                                    Decls[0]->getName(),
                                                    MemberLoc);
    BadExpr->setType(ErrorType::get(Context));
    return BadExpr;
  }

  // If we have a single declaration, build an AST for it.
  if (Decls.size() == 1) {
    ValueDecl *Member = Decls[0];

    // Okay to refer to the member of an existential type.
    // FIXME: ExistentialMemberRefExpr needs to cope with a base of metatype
    // type.
    if (baseTy->isExistentialType()) {
      return new (Context) ExistentialMemberRefExpr(Base, DotLoc, Member,
                                                    MemberLoc);
    }

    // Okay to refer to a member of an archetype.
    if (baseTy->is<ArchetypeType>()) {
      return new (Context) ArchetypeMemberRefExpr(Base, DotLoc, Member,
                                                  MemberLoc);
    }

    // Reference to a member of a generic type.
    if (baseTy->is<BoundGenericType>()) {
      return new (Context) GenericMemberRefExpr(Base, DotLoc, Member,
                                                MemberLoc);
    }

    // Refer to a member variable of an instance.
    if (auto Var = dyn_cast<VarDecl>(Member)) {
      assert(baseIsInstance && "Referencing variable of metatype!");
      return new (Context) MemberRefExpr(Base, DotLoc, Var, MemberLoc);
    }

    Expr *Ref = new (Context) DeclRefExpr(Member, MemberLoc,
                                          Member->getTypeOfReference());

    // Refer to a non-static member function that binds 'this':
    if (baseIsInstance && Member->isInstanceMember()) {
      return new (Context) DotSyntaxCallExpr(Ref, DotLoc, Base);
    }

    // FIXME: If metatype types ever get a runtime representation, we'll need
    // to evaluate the object.
    return new (Context) DotSyntaxBaseIgnoredExpr(Base, DotLoc, Ref);
  }

  // We have multiple declarations. Build an overloaded member reference.
  Decls = Context.AllocateCopy(Decls);
  return new (Context) OverloadedMemberRefExpr(Base, DotLoc, Decls, MemberLoc,
                         UnstructuredUnresolvedType::get(Context));
}

Expr *TypeChecker::buildMemberRefExpr(Expr *Base, SourceLoc DotLoc,
                                      MemberLookup &Results,
                                      SourceLoc MemberLoc) {
  assert(Results.isSuccess() && "Cannot build non-successful member reference");

  // If we have an ambiguous result, build an overload set.
  SmallVector<ValueDecl*, 8> ResultSet;
  for (MemberLookupResult X : Results.Results)
    ResultSet.push_back(X.D);

  return buildMemberRefExpr(Base, DotLoc, ResultSet, MemberLoc);
}

