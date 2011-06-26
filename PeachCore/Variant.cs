﻿
//
// Copyright (c) Michael Eddington
//
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
// copies of the Software, and to permit persons to whom the Software is 
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in	
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Authors:
//   Michael Eddington (mike@phed.org)

// $Id$

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.Runtime;
using System.Reflection;
using System.Runtime.Serialization;
using Peach.Core.IO;

namespace Peach.Core
{
	/// <summary>
	/// Variant class emulates untyped scripting languages
	/// variables were typing can change as needed.  This class
	/// solves the problem of boxing internal types.  Instead
	/// explicit casts are used to access the value as needed.
	/// 
	/// TODO: Investigate implicit casting as well.
	/// TODO: Investigate deligates for type -> byte[] conversion.
	/// </summary>
	[Serializable]
	public class Variant
	{
		public enum VariantType
		{
			Unknown,
			Int,
			Long,
			ULong,
			String,
			ByteString,
			BitStream
		}

		VariantType _type = VariantType.Unknown;
		int _valueInt;
		long _valueLong;
		ulong _valueULong;
		string _valueString;
		byte[] _valueByteArray;
		BitStream _valueBitStream = null;

		public Variant(int v)
		{
			SetValue(v);
		}

		public Variant(long v)
		{
			SetValue(v);
		}

		public Variant(ulong v)
		{
			SetValue(v);
		}

		public Variant(string v)
		{
			SetValue(v);
		}

		public Variant(byte[] v)
		{
			SetValue(v);
		}

		public Variant(BitStream v)
		{
			SetValue(v);
		}

		public VariantType GetVariantType()
		{
			return _type;
		}

		public void SetValue(int v)
		{
			_type = VariantType.Int;
			_valueInt = v;
			_valueString = null;
			_valueBitStream = null;
			_valueByteArray = null;
		}

		public void SetValue(long v)
		{
			_type = VariantType.Long;
			_valueLong = v;
			_valueString = null;
			_valueBitStream = null;
			_valueByteArray = null;
		}

		public void SetValue(ulong v)
		{
			_type = VariantType.ULong;
			_valueULong = v;
			_valueString = null;
			_valueBitStream = null;
			_valueByteArray = null;
		}

		public void SetValue(string v)
		{
			_type = VariantType.String;
			_valueString = v;
			_valueBitStream = null;
			_valueByteArray = null;
		}

		public void SetValue(byte[] v)
		{
			_type = VariantType.ByteString;
			_valueByteArray = v;
			_valueString = null;
			_valueBitStream = null;
		}

		public void SetValue(BitStream v)
		{
			_type = VariantType.BitStream;
			_valueBitStream = v;
			_valueString = null;
			_valueByteArray = null;
		}

		/// <summary>
		/// Access variant as an int value.
		/// </summary>
		/// <param name="v">Variant to cast</param>
		/// <returns>int representation of value</returns>
		public static explicit operator int(Variant v)
		{
			if (v == null)
				throw new ApplicationException("Parameter v is null");

			switch (v._type)
			{
				case VariantType.Int:
					return v._valueInt;
				case VariantType.Long:
					if (v._valueLong > int.MaxValue || v._valueLong < int.MinValue)
						throw new ApplicationException("Converting this long to an int would cause loss of data");

					return (int)v._valueLong;
				case VariantType.ULong:
					if (v._valueULong > int.MaxValue)
						throw new ApplicationException("Converting this ulong to an int would cause loss of data");

					return (int)v._valueULong;
				case VariantType.String:
					if (v._valueString == string.Empty)
						return 0;

					return Convert.ToInt32(v._valueString);
				case VariantType.ByteString:
					throw new NotSupportedException("Unable to convert byte[] to int type.");
				case VariantType.BitStream:
					throw new NotSupportedException("Unable to convert BitStream to int type.");
				default:
					throw new NotSupportedException("Unable to convert to unknown type.");
			}
		}

		public static explicit operator long(Variant v)
		{
			if (v == null)
				throw new ApplicationException("Parameter v is null");

			switch (v._type)
			{
				case VariantType.Int:
					return (long)v._valueInt;
				case VariantType.Long:
					return v._valueLong;
				case VariantType.ULong:
					if (v._valueULong > long.MaxValue)
						throw new ApplicationException("Converting this ulong to a long would cause loss of data");

					return (long)v._valueULong;
				case VariantType.String:
					if (v._valueString == string.Empty)
						return 0;

					return Convert.ToInt64(v._valueString);
				case VariantType.ByteString:
					throw new NotSupportedException("Unable to convert byte[] to int type.");
				case VariantType.BitStream:
					throw new NotSupportedException("Unable to convert BitStream to int type.");
				default:
					throw new NotSupportedException("Unable to convert to unknown type.");
			}
		}

		public static explicit operator ulong(Variant v)
		{
			if (v == null)
				throw new ApplicationException("Parameter v is null");

			switch (v._type)
			{
				case VariantType.Int:
					return (ulong)v._valueInt;
				case VariantType.Long:
					if ((ulong)v._valueLong > ulong.MaxValue || v._valueLong < 0)
						throw new ApplicationException("Converting this long to a ulong would cause loss of data");

					return (ulong)v._valueLong;
				case VariantType.ULong:
					return v._valueULong;
				case VariantType.String:
					if (v._valueString == string.Empty)
						return 0;

					return Convert.ToUInt64(v._valueString);
				case VariantType.ByteString:
					throw new NotSupportedException("Unable to convert byte[] to int type.");
				case VariantType.BitStream:
					throw new NotSupportedException("Unable to convert BitStream to int type.");
				default:
					throw new NotSupportedException("Unable to convert to unknown type.");
			}
		}

		/// <summary>
		/// Access variant as string value.
		/// </summary>
		/// <param name="v">Variant to cast</param>
		/// <returns>string representation of value</returns>
		public static explicit operator string(Variant v)
		{
			if (v == null)
				throw new ApplicationException("Parameter v is null");

			switch (v._type)
			{
				case VariantType.Int:
					return Convert.ToString(v._valueInt);
				case VariantType.Long:
					return Convert.ToString(v._valueLong);
				case VariantType.ULong:
					return Convert.ToString(v._valueULong);
				case VariantType.String:
					return v._valueString;
				case VariantType.ByteString:
					throw new NotSupportedException("Unable to convert byte[] to string type.");
				case VariantType.BitStream:
					throw new NotSupportedException("Unable to convert BitStream to string type.");
				default:
					throw new NotSupportedException("Unable to convert to unknown type.");
			}
		}

		/// <summary>
		/// Access variant as byte[] value.  This type is currently limited
		/// as neather int or string's are properly cast to byte[] since 
		/// additional information is needed.
		/// 
		/// TODO: Investigate using deligates to handle conversion.
		/// </summary>
		/// <param name="v">Variant to cast</param>
		/// <returns>byte[] representation of value</returns>
		public static explicit operator byte[](Variant v)
		{
			if (v == null)
				throw new ApplicationException("Parameter v is null");

			switch (v._type)
			{
				case VariantType.Int:
					throw new NotSupportedException("Unable to convert int to byte[] type.");
				case VariantType.Long:
					throw new NotSupportedException("Unable to convert long to byte[] type.");
				case VariantType.ULong:
					throw new NotSupportedException("Unable to convert ulong to byte[] type.");
				case VariantType.String:
					throw new NotSupportedException("Unable to convert string to byte[] type.");
				case VariantType.ByteString:
					return v._valueByteArray;
				case VariantType.BitStream:
					return v._valueBitStream.Value;
				default:
					throw new NotSupportedException("Unable to convert to unknown type.");
			}
		}

		public static explicit operator BitStream(Variant v)
		{
			if (v == null)
				throw new ApplicationException("Parameter v is null");

			switch (v._type)
			{
				case VariantType.Int:
					throw new NotSupportedException("Unable to convert int to BitStream type.");
				case VariantType.Long:
					throw new NotSupportedException("Unable to convert long to BitStream type.");
				case VariantType.ULong:
					throw new NotSupportedException("Unable to convert ulong to BitStream type.");
				case VariantType.String:
					throw new NotSupportedException("Unable to convert string to BitStream type.");
				case VariantType.ByteString:
					return new BitStream(v._valueByteArray);
				case VariantType.BitStream:
					return v._valueBitStream;
				default:
					throw new NotSupportedException("Unable to convert to unknown type.");
			}
		}

		public static bool operator == (Variant a, Variant b)
		{
			if (((object)a == null) && ((object)b == null))
				return true;

			if (((object)a == null) || ((object)b == null))
				return false;
				
			try
			{
				string stra = (string)a;
				string strb = (string)b;

				if (stra.Equals(strb))
					return true;
				else
					return false;
			}
			catch { }

			byte[] aa = (byte[])a;
			byte[] bb = (byte[])b;

			if (aa.Length != bb.Length)
				return false;

			for (int cnt = 0; cnt < aa.Length; cnt++)
				if (aa[cnt] != bb[cnt])
					return false;

			return true;
		}

		public static bool operator !=(Variant a, Variant b)
		{
			return !(a == b);
		}

		public string ToHex(int maxBytes = 0)
		{
			byte[] data = (byte[])this;
			StringBuilder ret = new StringBuilder();

			for (int cnt = 0; cnt < data.Length; cnt++)
			{
				if (cnt > 0 && cnt % 8 == 0)
					ret.Append(" ");
				if (cnt > 0 && cnt % 16 == 0)
					ret.Append("\n");

				ret.AppendFormat("{0:x2} ", data[cnt]);

				if (maxBytes > 0 && cnt > maxBytes)
				{
					ret.Append("[cut]");
					break;
				}
			}

			return ret.ToString();
		}
	}
}
