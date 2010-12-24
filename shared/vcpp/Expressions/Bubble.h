#pragma once

// Copyright (c) by advancedfx.org
//
// Last changes:
// 2010-12-15 dominik.matrixstorm.com
//
// First changes
// 2010-10-24 dominik.matrixstorm.com


#include "Compile.h"


namespace Afx { namespace Expressions {



struct __declspec(novtable) IBubble abstract
{
	static void AddStandardFunctions (IBubble * bubble);

	static IBubble * New ();

	virtual IRef * Ref (void) abstract = 0;

	/// <summary>Inner compiler.</summary>
	/// <remarks>For establishing parent-child relationships between bubbles use OuterCompiler instead.</remarks>
	virtual ICompiler * Compiler (void) abstract = 0;

	/// <summary>Adds a named compiler to the bubble.</summary>
	virtual void Add(IStringValue * name, ICompiler * compiler) abstract = 0;

	/// <summary>Compile</summary>
	/// <param name="code">code to compile</expression>
	/// <returns>The compiled code.</returns>
	virtual ICompiled * Compile(IStringValue * code) abstract = 0;

	/// <summary>Hides all exposed function compilers matching the given name.</summary<
	virtual void Hide(IStringValue * name) abstract = 0;

	/// <summary>
	/// Outer compiler, to be used for cross-bubble linking _from parent to child_ only.
	/// </summary>
	/// <remarks>
	/// The outer compiler is for establishing a parent-child relationship between bubbles.
	/// Do not use it for anything else and don't use it in the wrong direction.
	/// </remarks>
	virtual ICompiler * OuterCompiler (void) abstract = 0;
};


} } // namespace Afx { namespace Expr {

