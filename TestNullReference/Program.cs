using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace TestNullReference
{
    interface TestInterface
    {
        void TestCall();
    }

    class TestClass
    {
        public int TestField;

        public void TestCall() { }
    }

    class TestClass2 : TestClass
    {   
    }

    class Program
    {
        #region methods to invoke null reference exceptions for various IL opcodes

        /// <summary>
        /// IL throw
        /// </summary>
        static void Throw()
        {
            throw null;
        }

        /// <summary>
        /// IL callvirt on interface
        /// </summary>
        static void CallVirtIf()
        {
            TestInterface i = null;
            i.TestCall();
        }

        /// <summary>
        /// IL callvirt on class
        /// </summary>
        static void CallVirtClass()
        {
            TestClass i = null;
            i.TestCall();
        }

        /// <summary>
        /// IL callvirt on inherited class
        /// </summary>
        static void CallVirtBaseClass()
        {
            TestClass2 i = null;
            i.TestCall();
        }

        /// <summary>
        /// IL ldelem
        /// </summary>
        /// <param name="a"></param>
        static void LdElem()
        {
            int[] array = null;
            var firstElement = array[0];
        }

        /// <summary>
        /// IL ldelema
        /// </summary>
        /// <param name="a"></param>
        static unsafe void LdElemA()
        {
            int[] array = null;
            fixed (int* firstElementA = &(array[0]))
            {                
            }
        }

        /// <summary>
        /// IL stelem
        /// </summary>
        /// <param name="a"></param>
        static void StElem()
        {
            int[] array = null;
            array[0] = 3;
        }

        /// <summary>
        /// IL ldlen
        /// </summary>
        /// <param name="a"></param>
        static void LdLen()
        {
            int[] array = null;
            var len = array.Length;
        }

        /// <summary>
        /// IL ldfld
        /// </summary>
        /// <param name="a"></param>
        static void LdFld()
        {
            TestClass c = null;
            var fld = c.TestField;
        }

        /// <summary>
        /// IL ldflda
        /// </summary>
        /// <param name="a"></param>
        static unsafe void LdFldA()
        {
            TestClass c = null;
            fixed (int* fld = &(c.TestField))
            {
                
            }
        }

        /// <summary>
        /// IL stfld
        /// </summary>
        /// <param name="a"></param>
        static void StFld()
        {
            TestClass c = null;
            c.TestField = 3;
        }

        /// <summary>
        /// IL unbox_any
        /// </summary>
        static void Unbox()
        {
            object o = null;
            var val = (int) o;
        }

        /// <summary>
        /// IL ldind
        /// </summary>
        static unsafe void LdInd()
        {
            int* valA = null;
           
            var val = *valA;
        }

        /// <summary>
        /// IL ldind
        /// </summary>
        static unsafe void StInd()
        {
            int* valA = null;

            *valA = 3;
        }
        #endregion

        static void LogNullReference(Action a)
        {
            try
            {
                a();
            }
            catch (NullReferenceException ex)
            {
                var msg = string.Format("NullReferenceException executing {0} : {1}", a.Method.Name, ex.Message);
                Console.WriteLine(msg);
            }
        }

        static void Main(string[] args)
        {
            while (!Console.KeyAvailable)
            {
                LogNullReference(Throw);

                LogNullReference(CallVirtIf);
                LogNullReference(CallVirtClass);
                LogNullReference(CallVirtBaseClass);

                LogNullReference(LdElem);
                LogNullReference(LdElemA);
                LogNullReference(StElem);
                LogNullReference(LdLen);

                LogNullReference(LdFld);
                LogNullReference(LdFldA);
                LogNullReference(StFld);

                LogNullReference(Unbox);

                LogNullReference(LdInd);
                LogNullReference(StInd);

                Thread.Sleep(2000);   
            }           
        }
    }
}
