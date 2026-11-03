//Type is in namespace Game

[Token(Token = "0x200000A")]
public class Crate : IComparable<TItem>
{
	[Address(RVA = "0x1900", Offset = "0xD00", Length = "0x8")]
	[Token(Token = "0x600000B")]
	public void Open() { }

}
