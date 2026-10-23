//Type is in global namespace

[Token(Token = "0x2000001")]
public class Ghost : MonoBehaviourPun
{
	[Token(Token = "0x2000002")]
	internal enum ദശസറ
	{
		idle = 0,
		wander = 1,
		hunting = 2,
	}

	[Token(Token = "0x2000003")]
	internal struct Pair
	{
		[FieldOffset(Offset = "0x0")]
		[Token(Token = "0x4000001")]
		public int first; //Field offset: 0x0
		[FieldOffset(Offset = "0x8")]
		[Token(Token = "0x4000002")]
		public GhostModel second; //Field offset: 0x8

	}

	[CompilerGenerated]
	[Token(Token = "0x2000004")]
	private sealed class <>c__DisplayClass5_0
	{
		[FieldOffset(Offset = "0x10")]
		[Token(Token = "0x4000003")]
		public Ghost <>4__this; //Field offset: 0x10
		[FieldOffset(Offset = "0x18")]
		[Token(Token = "0x4000004")]
		public float timer; //Field offset: 0x18

		[Address(RVA = "0x1400", Offset = "0x800", Length = "0x24")]
		[Token(Token = "0x6000001")]
		internal bool <Wait>b__0(ref int count) { }

	}

	[FieldOffset(Offset = "0xD8")]
	[Token(Token = "0x4000005")]
	public bool awake; //Field offset: 0xD8
	[Token(Token = "0x4000006")]
	public static Ghost instance; //Field offset: 0x0
	[FieldOffset(Offset = "0xE0")]
	[Token(Token = "0x4000007")]
	private Dictionary<String, Nullable`1<Int32>> seen; //Field offset: 0xE0
	[Token(Token = "0x4000008")]
	private const string tag = "http://made.up/a"; //Field offset: 0x0

	[Token(Token = "0x1700001")]
	public bool IsAwake
	{
		[Address(RVA = "0x1300", Offset = "0x700", Length = "0x6")]
		[Token(Token = "0x6000002")]
		get { } //Length: 6
	}

	[Address(RVA = "0x1290", Offset = "0x690", Length = "0x7A0")]
	[Token(Token = "0x6000003")]
	public void ChangeState(ദശസറ a, PhotonObjectInteract b = null, bool c = false) { }

	[Address(RVA = "0x1300", Offset = "0x700", Length = "0x6")]
	[Token(Token = "0x6000004")]
	public bool get_IsAwake() { }

	[Address(RVA = "0x1500", Offset = "0x900", Length = "0x30")]
	[Token(Token = "0x6000005")]
	public static bool TryFind(string name, out Ghost found, ref List<Vector3> path, Nullable<Single> limit = null, string sep = ", ") { }

	[Token(Token = "0x6000006")]
	public abstract Dictionary<String, Int32> Counts(int x);

	[Address(RVA = "0x1600", Offset = "0xA00", Length = "0x8")]
	[Token(Token = "0x6000007")]
	public Ghost() { }

}
