namespace System;

[IsReadOnly]
[Token(Token = "0x2000134")]
public struct float : IComparable, IEquatable<Single>
{
	[Token(Token = "0x400048E")]
	public const float MinValue = -3.4028235E+38; //Field offset: 0x0
	[Token(Token = "0x4000495")]
	internal float m_value; //Field offset: 0x0

}
