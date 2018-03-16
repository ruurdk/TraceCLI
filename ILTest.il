.assembly extern mscorlib {}
.assembly TestTarget {}
.module TestTarget.exe

//global (no namespace) stuff

.method assembly hidebysig specialname rtspecialname static void .cctor() cil managed
{
	ldstr "Module Initializer"
	call void [mscorlib]System.Console::WriteLine(string)
	ret
}

.field assembly static int32 GlobalInt

.method assembly static void GlobalMethod() cil managed
{
	ldstr "Global method"
	call void [mscorlib]System.Console::WriteLine(string)
	ret
}

.class NamespacelessClass extends [mscorlib]System.Object
{
	.method public static hidebysig specialname rtspecialname void .cctor() cil managed
	{
		ldstr "Namespaceless class Initializer"
		call void [mscorlib]System.Console::WriteLine(string)
		ret
	}

	.method public hidebysig specialname rtspecialname instance void .ctor () cil managed 
	{
		ldstr "Namespaceless class Constructor"
		call void [mscorlib]System.Console::WriteLine(string)
		ret
	}

	.method public hidebysig instance void InstanceMethod() cil managed
	{
		ldstr "Namespaceless class instance method"
		call void [mscorlib]System.Console::WriteLine(string)
		ret
	}
}

//regular class
.class TestTarget.EntryClass extends [mscorlib]System.Object
{
	.field public void* FieldVoid;

	.method public static hidebysig specialname rtspecialname void .cctor() cil managed
	{
		ldstr "Entrypoint class type constructor"
		call void [mscorlib]System.Console::WriteLine(string)
		ret
	}

	.method public static void EntryMethod(string[] args)	cil managed
	{
		.entrypoint
		ldstr "Entrypoint"
		call void [mscorlib]System.Console::WriteLine(string)
		
		newobj instance void NamespacelessClass::.ctor()	
		call instance void NamespacelessClass::InstanceMethod()

		call void GlobalMethod()

		ret
	}
}