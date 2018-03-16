using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace DebugTestTarget
{
    class Generic<T, U, V>
    {

    }

    class InstanceTest
    {
        public const int constantInt = 3344;
        public const byte constantByte = 0x50;
        public const string constantString = "Constant";
        public const string constantEmptyString = "";
        public const string constantNullString = null;
        
        public delegate void sleepPtr(int u);

        public Byte testUByte = 1;
        public UInt16 testUShort = 2;
        public UInt32 testUInt = 4;
        public UInt64 testULong = 8;

        public SByte testByte = -1;
        public Int16 testShort = -2;
        public Int32 testInt = -4;
        public Int64 testLong = -8;

        public IntPtr testIntPtr = new IntPtr(23);
        public UIntPtr testUIntPtr = new UIntPtr(23);

        public char testChar = 'a';
        public string testStringInstance = "instance String";
        public static string testStringStatic = "static String";

        public bool testBool = true;

        public float testFloat = (float)-300.123;
        public double testDouble = -300.123;

        public InstanceTest testPtr;
        //todo: some byref test        
        public Guid testValueType = Guid.Empty;

        public NestedClass testNestedClass = new NestedClass();

        public int[] testSZArray = new[] { 1, 2, 3 };
        public int[][] testMDArray = new int[2][];

        public sleepPtr testFuncPtr;

        public Object testObject = new object();

        public NestedGeneric<Object> testGeneric = new NestedGeneric<object>();


        public void GenericMethod<T, U>() where T: class, new() where U: struct
        {
            return;
        }

        public InstanceTest()
        {
            testMDArray[0] = new[] { 1, 2, 3 };
            testMDArray[1] = new[] { 4, 5, 6, 7 };
            testFuncPtr = SleepOne;
            testPtr = this;
        }

        public int sleepCount = 0;

        [MethodImpl(MethodImplOptions.NoOptimization)]
        public void SleepOne(int testParam)
        {
            sleepCount++;
            try
            {
                throw new Exception("First chance");
            }
            catch (Exception)
            {                               
            }

            Thread.Sleep(1000);
            Console.Write(".");

            var testnew = new object();

            var testarraynew = new object[3];
            testarraynew[0] = 1;

            GenericMethod<object, int>();
            testGeneric.NestedGenericMethod<int>();
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool GetPhysicallyInstalledSystemMemory(out long MemoryInKilobytes);

        public class NestedClass
        { }

        public class NestedGeneric<T> where T : class
        {
            public void NestedGenericMethod<T>()
            {
            }
        }
    }

    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Pid of this process: {0}", Process.GetCurrentProcess().Id);
            Console.WriteLine("Press any key...");

            var instance = new InstanceTest();

            while (!Console.KeyAvailable)
            {
                instance.SleepOne(3);
                long installedMemory;
                InstanceTest.GetPhysicallyInstalledSystemMemory(out installedMemory);
            }
        }
    }
}
