/*
 * RUN: %cxx %s -o %t.exe
 * RUN: %t.exe > %t.out
 * RUN: %FileCheck -input-file %t.out %s
 *
 * This test currently doesn't pass, as there seems to be a bug in the GrowLR logic
 * of Pegmatite's implementation of the OMeta parsing algorithm.
 *
 * XFAIL: *
 */

#include <iostream>
#include <string>

#include "pegmatite.hh"

using namespace pegmatite;

static void PrettyPrint(std::string);

int main()
{
	// CHECK: FieldReference {
	// CHECK:   base: NameReference { name: foo },
	// CHECK:   fieldName: bar
	// CHECK: }
	PrettyPrint("foo.bar");

	// CHECK: FieldReference {
	// CHECK:   base: FieldReference {
	// CHECK:     base: NameReference { name: foo },
	// CHECK:     fieldName: bar
	// CHECK:   },
	// CHECK:   fieldName: baz
	// CHECK: }
	PrettyPrint("foo.bar.baz");

	return 0;
}

/**
 * The grammar for this "language" accepts an optional expression, which can
 * be a reference to a named value or a reference to a field within a named
 * value (or a field within a field, etc.).
 *
 * The [Ohm](https://github.com/harc/ohm) equivalent of this grammar is:
 *
 * ```
 * ExampleGrammar
 * {
 *     Term = FieldReference | NameReference
 *     FieldReference = Term "." Identifier
 *     NameReference = Identifier
 *     Identifier = letter*
 * }
 * ```
 */
struct Grammar
{
	const Rule Space          = " \t\n"_E;
	const Rule Alpha          = "[A-Za-z]+"_R;
	const Rule Identifier     = trace("Identifier", term(+Alpha));
	const Rule NameReference  = trace("NameReference", Identifier);
	const Rule FieldReference = trace("Field", Term >> "." >> Identifier);
	const Rule Term           = trace("Term", FieldReference|NameReference);
};

namespace ast
{
	class Printable
	{
	public:
		virtual void print(std::ostream&) const = 0;
	};

	//! A term within an expression
	class Term : public pegmatite::ASTContainer, public Printable
	{
		PEGMATITE_RTTI(Term, pegmatite::ASTContainer)
	};

	//! An identifier in this language is strictly alphabetic
	class Identifier : public pegmatite::ASTString, public Printable
	{
	public:
		void print(std::ostream &o) const override
		{
			o << "Identifier { " << *this << " }";
		}

		PEGMATITE_RTTI(Identifier, pegmatite::ASTString)
	};

	//! A reference to a name is an identifier (in the proper context)
	class NameReference : public Term
	{
	public:
		void print(std::ostream &o) const override
		{
			o << "NameReference { name: " << *name << " }";
		}

		PEGMATITE_RTTI(NameReference, Term)

	private:
		pegmatite::ASTPtr<Identifier> name;
	};

	//! A reference to a field within a value (base, dot, field name)
	class FieldReference : public Term
	{
	public:
		void print(std::ostream &o) const override
		{
			o << "FieldReference { base: ";
			base->print(o);
			o << ", fieldName: " << *fieldName << " }";
		}

		PEGMATITE_RTTI(FieldReference, Term)

	private:
		pegmatite::ASTPtr<Term> base;
		pegmatite::ASTPtr<Identifier> fieldName;
	};
}

/**
 * The parser delegate binds AST nodes to the grammar.
 */
struct Parser : public ASTParserDelegate
{
	const Grammar grammar = Grammar();

	BindAST<ast::Identifier> identifier = grammar.Identifier;
	BindAST<ast::NameReference> name = grammar.NameReference;
	BindAST<ast::FieldReference> fieldReference = grammar.FieldReference;
};

static void PrettyPrint(std::string s)
{
	pegmatite::StringInput input(std::move(s));

	Parser p;
	std::unique_ptr<ast::Term> root;

	if (p.parse(input, p.grammar.Term, p.grammar.Space,
		pegmatite::defaultErrorReporter, root))
	{
		root->print(std::cout);
		return;
	}
}
