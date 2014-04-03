/*
	Copyright 2014 Santeri Piippo
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	1. Redistributions of source code must retain the above copyright
	   notice, this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.
	3. The name of the author may not be used to endorse or promote products
	   derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
	NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "main.h"
#include "lexer.h"

const char g_propertyTemplate[] =
	"template<typename Parent, typename Type, int Offset, void (*Func)(Parent*, Type&, const Type&)>\n"
	"class metacollector_customproperty\n"
	"{\n"
	"public:\n"
	"\tusing Self = metacollector_customproperty<Parent, Type, Offset, Func>;\n"
	"\n"
	"\tmetacollector_customproperty(){}\n"
	"\tmetacollector_customproperty (const Type& a) :\n"
	"\t\tm_value (a) {}\n"
	"\n"
	"\tSelf& operator= (const Type& a)\n"
	"\t{\n"
	"\t\t(*Func) (reinterpret_cast<Parent*> (this - Offset), m_value, a);\n"
	"\t\treturn *this;\n"
	"\t}\n"
	"\n"
	"\toperator const Type&() const\n"
	"\t{\n"
	"\t\treturn m_value;\n"
	"\t}\n"
	"\n"
	"\tconst Type& value() const\n"
	"\t{\n"
	"\t\treturn m_value;\n"
	"\t}\n"
	"\n"
	"private:\n"
	"\tType m_value;\n"
	"};\n";

struct Property
{
	String	name;
	String	type;
	String	read;
	String	write;

	bool isTrivial() const
	{
		return read.isEmpty() && write.isEmpty();
	}
};

struct ClassData
{
	String			name;
	List<Property>	properties;
	bool			gotClassDataMacro;

	ClassData() :
		gotClassDataMacro (false) {}
};

static String g_currentFile;
static List<ClassData> g_classes;
static StringList g_sourceIncludes;

String redirectorName (const String& className, const String& propertyName)
{
	return format ("metacollector_property_%1_%2", className, propertyName);
}

void processFile (String file)
{
	g_currentFile = file;
	Lexer lx;
	lx.processFile (file);
	int stack = 0;
	ClassData* currentClass = null;
	bool gotAnything = false;
	bool requireClassData = false;

	while (lx.next (TK_Any))
	{
		if (lx.tokenType() == TK_BraceStart)
			stack++;
		elif (lx.tokenType() == TK_BraceEnd)
		{
			stack--;

			if (stack == 0 && currentClass != null)
			{
				if (requireClassData)
					g_classes << *currentClass;

				delete currentClass;
				currentClass = null;
			}
		}
		elif (lx.tokenType() == TK_Semicolon)
		{
			if (stack == 0)
			{
				delete currentClass;
				currentClass = null;
			}
		}
		elif (lx.tokenType() == TK_Symbol && lx.token()->text == "class")
		{
			lx.mustGetNext (TK_Symbol);
			currentClass = new ClassData;
			currentClass->name = lx.token()->text;
			requireClassData = false;
		}
		elif (lx.tokenType() == TK_Symbol)
		{
			if (lx.token()->text == "PROPERTY")
			{
				if (currentClass == null || stack != 1)
					error ("PROPERTY outside class\n");

				Property prop;
				StringList tokens;
				lx.mustGetNext (TK_ParenStart);

				while (lx.next (TK_Semicolon) == false && lx.next (TK_ParenEnd) == false)
				{
					lx.mustGetNext (TK_Any);
					tokens << lx.token()->text;
				}

				if (tokens.size() == 1)
					error ("not enough type/name tokens for PROPERTY");

				tokens.pop (prop.name);
				prop.type = tokens.join (" ");

				if (lx.tokenType() == TK_Semicolon)
				{
					while (lx.next (TK_ParenEnd) == false)
					{
						lx.mustGetNext (TK_Symbol);
						if (lx.token()->text == "READ")
						{
							if (prop.read.isEmpty() == false)
								error ("%1::%2 has double READ", currentClass->name, prop.name);

							lx.mustGetNext (TK_Symbol);
							prop.read = lx.token()->text;
						}
						elif (lx.token()->text == "WRITE")
						{
							if (prop.write.isEmpty() == false)
								error ("%1::%2 has double WRITE", currentClass->name, prop.name);

							lx.mustGetNext (TK_Symbol);
							prop.write = lx.token()->text;
						}
					}
				}

				currentClass->properties << prop;
				requireClassData = true;
			}
			elif (lx.token()->text == "CLASSDATA")
			{
				if (currentClass->gotClassDataMacro)
					error ("%1 already has CLASSDATA", currentClass->name);

				lx.mustGetNext (TK_ParenStart);
				lx.mustGetNext (TK_Symbol);

				if (lx.token()->text != currentClass->name)
					error ("CLASSDATA macro needs the class name as the argument. Use CLASSDATA (%1)",
						currentClass->name);

				lx.mustGetNext (TK_ParenEnd);
				currentClass->gotClassDataMacro = true;
				gotAnything = true;
			}
		}
	}

	if (gotAnything)
		g_sourceIncludes << file;
}

bool fileExists (const String& path)
{
	FILE* fp = fopen (path, "r");

	if (fp == null)
		return false;

	fclose (fp);
	return true;
}

time_t getModificationTime (const String& path)
{
	struct stat st;

	if (stat (path, &st) != 0)
		error ("couldn't stat %1", path);

	return st.st_mtim.tv_sec;
}

int main (int argc, char** argv)
{
	try
	{
		// Check modification times. If no header has been modified since the
		// metadata was, we don't need to do anything.
		if (fileExists (argv[argc - 1]) && fileExists (argv[argc - 2]))
		{
			time_t basetime = getModificationTime (argv[argc - 1]);
			bool mustProcess = false;

			for (int i = 1; i < argc - 2 && mustProcess == false; ++i)
				mustProcess |= getModificationTime (argv[i]) > basetime;

			if (mustProcess == false)
			{
				print ("%1: No headers changed.\n", basename (argv[0]));
				return 0;
			}
		}

		for (int i = 1; i < argc - 2; ++i)
			processFile (argv[i]);

		for (ClassData& cls : g_classes)
		{
			if (cls.gotClassDataMacro == false)
				error ("%1 does not have the CLASSDATA macro\n", cls.name);
		}

		FILE* source = fopen (argv[argc - 1], "w");
		FILE* header = fopen (argv[argc - 2], "w");

		if (header == null)
			error ("could not open %1 for writing: %2", argv[argc - 2], strerror (errno));

		if (source == null)
			error ("could not open %1 for writing: %2", argv[argc - 1], strerror (errno));

		time_t rawtime;
		time (&rawtime);

		for (FILE* fp : List<FILE*> ({header, source}))
		{
			printTo (fp, "// Auto-generated by %1 at %2\n"
				"// This file will be overwritten, do not edit by hand.\n\n",
				argv[0], asctime (localtime (&rawtime)));
		}

		printTo (header, "#pragma once\n");
		printTo (header, "#include <cstddef>\n");
		printTo (header, "#define PROPERTY(...)\n");
		printTo (header, "#define CLASSDATA(A) METACOLLECTOR_CLASS_DATA_##A\n");
		printTo (header, "\n");

		for (String& a : g_sourceIncludes)
			printTo (source, "#include \"%1\"\n", a);

		// Write stubs
		for (ClassData& cls : g_classes)
			printTo (header, "class %1;\n", cls.name);

		printTo (source, "\n");
		printTo (header, "\n");
		printTo (header, g_propertyTemplate);
		printTo (header, "\n");

		// Write redirector signatures
		for (ClassData& cls : g_classes)
		for (Property& prop : cls.properties)
		{
			if (prop.isTrivial() == false)
			{
				String signature = format ("void %1 (%2* parent, %3& value, %3 const& newValue)",
					redirectorName (cls.name, prop.name), cls.name, prop.type);

				printTo (header, signature + ";\n");
				printTo (source, signature + "\n");
				printTo (source, "{\n");
				printTo (source, "\tparent->%1 (value, newValue);\n", prop.write);
				printTo (source, "}\n");
				printTo (source, "\n");
			}
		}

		for (ClassData& cls : g_classes)
		{
			printTo (header, "#define METACOLLECTOR_CLASS_DATA_%1 \\\n", cls.name);
			printTo (header, "using Self = %1; \\\n", cls.name);

			// Write offset reference struct
			printTo (header, "struct OffsetReference \\\n\t{ \\\n", cls.name);

			for (Property& prop : cls.properties)
				printTo (header, "\t\t%1 %2; \\\n", prop.type, prop.name);

			printTo (header, "}; \\\n\\\n");

			for (Property& prop : cls.properties)
			{
				printTo (header, "public:\\\n");

				if (prop.isTrivial())
					printTo (header, "\t%1 %2; \\\n", prop.type, prop.name);
				else
				{
					String size = format ("offsetof (metacollector_refstruct_%1, %2)",
						cls.name, prop.name);

					printTo (header, "\tmetacollector_customproperty<%1, %2, %3, %4> %5; \\\n",
						cls.name, prop.type, size, redirectorName (cls.name, prop.name), prop.name);
				}

				if (prop.read.isEmpty() == false)
					printTo (header, "\tvoid %1 (%2 const& value) const; \\\n",
						prop.read, prop.type);

				if (prop.write.isEmpty() == false)
					printTo (header, "\tvoid %1 (%2& value, %2 const& newValue); \\\n",
						prop.write, prop.type);
			}

			printTo (header, "\n");
		}

		fclose (header);
		fclose (source);
		return 0;
	}
	catch (std::exception& e)
	{
		fprintf (stderr, "error: %s\n", e.what());
		return 1;
	}
}

const String& currentFileName()
{
	return g_currentFile;
}
