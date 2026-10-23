//Type is in namespace Game

[Token(Token = "0x2000001")]
public class Probe : MonoBehaviour
{
	[Token(Token = "0x2000002")]
	public enum Mode
	{
		off = -1,
		low = 0,
		high = 5,
	}

	[Token(Token = "0x2000003")]
	public struct Cell
	{
		[FieldOffset(Offset = "0x0")]
		[Token(Token = "0x4000001")]
		public int count; //Field offset: 0x0
		[FieldOffset(Offset = "0x8")]
		[Token(Token = "0x4000002")]
		public Probe owner; //Field offset: 0x8

	}

	[FieldOffset(Offset = "0x18")]
	[Token(Token = "0x4000003")]
	public bool awake; //Field offset: 0x18
	[FieldOffset(Offset = "0x19")]
	[Token(Token = "0x4000004")]
	private Mode mode; //Field offset: 0x19
	[FieldOffset(Offset = "0x20")]
	[Token(Token = "0x4000005")]
	private List<Vector3> path; //Field offset: 0x20
	[FieldOffset(Offset = "0x28")]
	[Token(Token = "0x4000006")]
	private Cell cell; //Field offset: 0x28
	[Token(Token = "0x4000007")]
	public static Probe instance; //Field offset: 0x0
	[Token(Token = "0x4000008")]
	private const int limit = 50; //Field offset: 0x0
	[Token(Token = "0x4000009")]
	private const float scale = 1E-05; //Field offset: 0x0
	[Token(Token = "0x400000B")]
	private const float turn = 360; //Field offset: 0x0
	[Token(Token = "0x400000C")]
	private const bool ready = True; //Field offset: 0x0
	[Token(Token = "0x400000D")]
	private const Mode start = 5; //Field offset: 0x0
	[Token(Token = "0x400000A")]
	public const string tag = "a//b"; //Field offset: 0x0

	[Address(RVA = "0x1200", Offset = "0x600", Length = "0x20")]
	[Token(Token = "0x6000001")]
	public bool Find(ref Probe other, Mode m) { }

	[Address(RVA = "0x1300", Offset = "0x700", Length = "0x10")]
	[Token(Token = "0x6000002")]
	public static Mode Pick(Vector3 at) { }

	[Token(Token = "0x6000003")]
	public abstract void Wait(int ticks);

	[Address(RVA = "0x1400", Offset = "0x800", Length = "0x8")]
	[Token(Token = "0x6000004")]
	public Probe() { }

	[Address(RVA = "0x1500", Offset = "0x900", Length = "0x8")]
	[Token(Token = "0x6000005")]
	private static Probe() { }

}
