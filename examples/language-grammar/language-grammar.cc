/**
 * @file language-grammar.cc
 *
 * A simple language grammar that includes both expressions and
 * non-expression nodes (e.g., identifiers).
 *
 * The language parsed by this example is extraordinarily simple: it includes
 * references to names (assumed to be pre-defined) and references to fields
 * within named values:
 *
 * ```
 * foo           # reference to an assumed-to-be-predefined name
 * foo.bar.baz   # reference to a field within a field of foo
 * ```
 */

#include <iostream>
#include <string>

#include "pegmatite.hh"

using namespace pegmatite;


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
 *     Expression = Term
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
	const Rule Expression     = trace("Expression", Term);
};


namespace ast
{
	class Printable
	{
	public:
		virtual void print(std::ostream&) const = 0;
	};

	//! An expression (currently no operators, so not very exciting)
	class Expression : public pegmatite::ASTContainer, public Printable
	{
		PEGMATITE_RTTI(Expression, pegmatite::ASTContainer)
	};

	//! A term within an expression
	class Term : public Expression
	{
		PEGMATITE_RTTI(Term, Expression)
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


int main()
{
	Parser p;
	std::string s;

	std::cout << "Enter one expression per line (EOF to quit):\n";

	while (true)
	{
		std::cout << "> ";
		std::getline(std::cin, s);

		if (std::cin.eof())
		{
			break;
		}
		else if (s.empty())
		{
			continue;
		}

		pegmatite::StringInput input(move(s));
		std::unique_ptr<ast::Expression> root;

		if (p.parse(input, p.grammar.Expression, p.grammar.Space,
			pegmatite::defaultErrorReporter, root))
		{
			root->print(std::cout);
			std::cout << "\n";
		}
		else
		{
			std::cerr << "Parse error\n";
			break;
		}
	}

	return 0;
}
