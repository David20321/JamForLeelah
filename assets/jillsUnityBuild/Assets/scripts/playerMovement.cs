using UnityEngine;
using System.Collections;

public class playerMovement : MonoBehaviour {

	public float speed = 15.0F; //Max speed of the character
	float newSpeed; //Used to modify speed based on input
	
	public float jumpSpeed = 30.0F; //How high you can jump in relation to gravity
	public static float gravity = 40.0F; //how fast you fall
	public float turnSpeed = 0.2f; //how fast you the player turns
	
	bool gliding;
	public float liftRatio = 1; //at 0.01, gravity will be 99% effective, at 0.99, gravity will only be 1% effective
	
	//Quaternion myRotation; //used to store direction of movement
	float Horizontal; //raw value for Horizontal axis
	float Vertical; //raw value for Vertical axis
	
	public Vector3 moveDirection = Vector3.zero; //initialize movement direction
	private Vector3 inputMagnitude; //store axis input
	//private Vector3 lastMoveDirection; //record last movement.
	public Vector3 playerPos;
	
	CharacterController controller; //create instance of character controller

	//making it easier to edit controls later
	string myHorizontal = "Horizontal";
	string myVertical = "Vertical";
	string myFire1 = "Fire1";
	string myFire2 = "Fire2";
	string myFire3 = "Fire3";
	string myJump = "Jump";

	//used for determining pitch / yaw of character as it travels over terrain 
	Vector3 storeNormal;

	public bool showLight;
	public GameObject myLight; 

	void Awake(){
		controller = GetComponent<CharacterController>();
		showLight = false;
		myLight.SetActive(false);

	}

	// Use this for initialization
	void Start () {
	
	}
	
	// Update is called once per frame
	void Update () {

		//Get axis values for calculating movement
		Horizontal = Input.GetAxis(myHorizontal);
		Vertical = Input.GetAxis(myVertical);
		inputMagnitude =  new Vector3(Horizontal, 0, Vertical);
		if (inputMagnitude.sqrMagnitude != 0.0f) {
			//lastMoveDirection = inputMagnitude;
		} 
		//Modifies speed based on axis input
		newSpeed = speedMod();
		//Ground Based Movement;
		if (controller.isGrounded) {
			gliding = false;
			//Get axis inputs and * by speed
			moveDirection = new Vector3(Horizontal, 0, Vertical);
			moveDirection *= newSpeed;
			RaycastHit hit;
			Physics.Raycast(transform.position, Vector3.down, out hit);
			Debug.DrawRay(transform.position, Vector3.up, Color.blue, 2);
			if (Physics.Raycast(transform.position, Vector3.down, 2)) {
				storeNormal = hit.normal;
				//Debug.Log("Normal hit: " + storeNormal);
				Debug.DrawRay(transform.position, storeNormal, Color.green, 2);
			}
			//Apply changes to each child of the game object
			foreach (Transform child in transform) {
				if (moveDirection.sqrMagnitude > 0) { 
					//myAnimation.SetBool("Run", true); //Changes avatar to running state
					//var normalRotation = Quaternion.FromToRotation(transform.up, storeNormal);
					var targetRotation = Quaternion.LookRotation(moveDirection, storeNormal); //set target towards direction of motion
					child.rotation = child.rotation.EaseTowards(targetRotation, turnSpeed); //rotate towards the direction of motion
					//myRotation = child.rotation;
				}  else {
					//myAnimation.SetBool ("Run", false); 
				}
			}
			//How to jump!
			if (Input.GetButtonDown(myJump)) {
				moveDirection.y = jumpSpeed;
				//mystats.stamina -= mystats.staminaBurnFlapping;
			}
			//Air based movement
		} else {
			if (Input.GetButtonDown(myJump) && gliding == false) {
				gliding = true;
				//mystats.stamina -= mystats.staminaBurnFlapping;
			} else if (Input.GetButtonUp(myJump)) {
				gliding = false;
			}
			//Air Control
			//TODO: This is messy, it needs to be cleaned up.
			moveDirection.x = Horizontal;
			moveDirection.z = Vertical;
			Vector3 normalizeXZ = new Vector3(moveDirection.x, moveDirection.y, moveDirection.z);
			normalizeXZ *= newSpeed;
			Vector3 moveInAirDirection = new Vector3(normalizeXZ.x, moveDirection.y, normalizeXZ.z);
			moveDirection = transform.TransformDirection(moveInAirDirection);
			foreach (Transform child in transform) {
				if (moveDirection.sqrMagnitude > 0.5f) { 
					//myAnimation.SetBool("Run", true); //Changes avatar to running state
					Vector3 lookatMoveDirection = new Vector3(	moveDirection.x, 0, moveDirection.z);
					if (inputMagnitude.sqrMagnitude > 0.5f) {
						var targetRotation = Quaternion.LookRotation(lookatMoveDirection); //set target towards direction of motion
						child.rotation = child.rotation.EaseTowards(targetRotation, turnSpeed); //rotate towards the direction of motion
						//myRotation = child.rotation;
					}
				}  else {
					//myAnimation.SetBool ("Run", false); 
				}
			}
		}

		//Controls Gravity
		glideControl();
		pulseLight();
		//move the player at the end of Update
		controller.Move(moveDirection * Time.deltaTime);
		playerPos = this.gameObject.transform.position;
	
	}

	float speedMod () {
		
		//get the absolute value of each axis
		var horAbs = Mathf.Abs(Horizontal);
		var vertAbs = Mathf.Abs(Vertical);
		float angle = 0;
		//do some math...
		if (horAbs > vertAbs) { 
			angle = Mathf.Atan2(vertAbs, horAbs); 
		} else {
			angle = Mathf.Atan2(horAbs, vertAbs);
		}
		newSpeed = Mathf.Cos(angle);
		newSpeed *= speed;
		//It magically works!
		return newSpeed;
	}

	//Should probably make it where just holding down jump enables glide...
	void glideControl () {
		if (gliding == true) {
			var adjustForGlide = gravity * liftRatio;
			moveDirection.y -= gravity * Time.deltaTime;
			moveDirection.y += adjustForGlide * Time.deltaTime;
		} else {
			moveDirection.y -= gravity * Time.deltaTime;
		//	Debug.Log("I'm Not Gliding!");
		}
	}

	void pulseLight () {

		if (Input.GetButtonDown(myFire1) && showLight == false) {
			showLight = true; 
			myLight.SetActive(true);
			StartCoroutine("lightTimer");
		} else if (Input.GetButtonUp(myFire1)) {
			StopCoroutine("lightTimer");
			myLight.SetActive(false);
			showLight=false;
		}
	}

	IEnumerator lightTimer () {
		var refreshLight = 0.25f; 
		yield return new WaitForSeconds (refreshLight);
		myLight.SetActive(false);
		showLight = false;
	}
}