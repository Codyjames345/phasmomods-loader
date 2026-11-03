//Type is in namespace Game

[Token(Token = "0x2000008")]
public class Box
{
	[Token(Token = "0x2000009")]
	public class Lid
	{
		[Address(RVA = "0x1800", Offset = "0xC00", Length = "0x8")]
		[Token(Token = "0x6000009")]
		public void Close() { }

	}

	[Address(RVA = "0x1700", Offset = "0xB00", Length = "0x10")]
	[Token(Token = "0x6000008")]
	public void Put(T item) { }

	[Address(RVA = "0x1700", Offset = "0xB00", Length = "0x10")]
	[Token(Token = "0x600000A")]
	public void Clear() { }

}
