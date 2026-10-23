//Type is in namespace UnityEngine

[Token(Token = "0x2000010")]
public sealed class Camera : Behaviour
{
	[Address(RVA = "0x2000", Offset = "0x1400", Length = "0x10")]
	[Token(Token = "0x6000010")]
	public static Camera get_main() { }

	[Address(RVA = "0x2100", Offset = "0x1500", Length = "0x40")]
	[Token(Token = "0x6000011")]
	public Ray ScreenPointToRay(Vector3 pos) { }

}
