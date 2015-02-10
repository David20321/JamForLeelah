using UnityEngine;
using System.Collections;

public enum ControlType {
	PLAYER,
	AI,
}

public enum MyType {
	BLUE,
	GREY,
	RED,
	GREEN,
}

public class basicBehavior : MonoBehaviour {

	//common stats for all actors:
	public float HP;
	public float maxHP;
	public float speed;
	public float jumpSpeed;

	public ControlType controlType;
	public MyType myType;

	CharacterController controller; //create instance of character controller
	public GameObject thisCharacter;

	//grey controls
	bool revealed;
	bool triggerReveal;
	public GameObject greyChar;

	//green stuuff
	public GameObject greenChar;

	//red controls
	public GameObject redChar;

	void Awake () {

		//foreach (Transform child in transform) {
			//gameObject.SetActive(false);
		//}

		controller = GetComponent<CharacterController>();
		thisCharacter = this.gameObject;
		thisCharacter.renderer.enabled = false;

		HP = maxHP;
		revealed = false;
		triggerReveal = false;

		if (myType == MyType.GREY) {
			Debug.Log("I'm grey yo!");
			greyChar.SetActive(true);
		}

	}

	// Use this for initialization
	void Start () {

		Debug.Log("am i alive?");
	
	}
	
	// Update is called once per frame
	void Update () {


		switch (myType) {

		case (MyType.GREY):
			greyBehavior();
			break;

		case (MyType.GREEN):
			Debug.Log("I am green now yo");
			break;

		case (MyType.RED):
			Debug.Log("I am Red now yo");
			break;

		default: //default behavior, if there is any...
			break;
		}
	}

	void greyBehavior () {

		if (revealed == true && triggerReveal == false) {
			Debug.Log("ive been exposed!");
			triggerReveal = true;
			changeType();
		}
	}

	//allows me to switch to a random type of character
	static T GetRandomEnum<T>()
	{
		System.Array A = System.Enum.GetValues(typeof(T));
		T V = (T)A.GetValue(UnityEngine.Random.Range(2,A.Length));
		return V;
	}
	
	//change character type (This could be done better i am sure)
	void changeType() {

		for(int i=0; i< thisCharacter.transform.childCount; i++)
		{
			var child = thisCharacter.transform.GetChild(i).gameObject;
			if(child != null)
				child.SetActive(false);
		}
		
		myType = GetRandomEnum<MyType>();
		switch (myType) {
			
		case (MyType.GREEN):
			greenChar.SetActive(true);
			break;

		case (MyType.RED):
			redChar.SetActive (true);
			break;

		default: Debug.Log("I am the wrong type...");
			break;

		}
	}
	
	void OnTriggerEnter(Collider other) {
		
		//Have i been exposed to the light? : O
		var exposed = other.gameObject.CompareTag("light");	
		if (exposed) {
			revealed = true;

		} 
	}
}
