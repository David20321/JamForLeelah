using UnityEngine;
using System.Collections;

public class BehaviorModifiers : MonoBehaviour {

	//adjust the speed of 2axis input for analogue stick controlers.
	public static float speedMod (float speed, float Horizontal, float Vertical) {
		
		//get the absolute value of each axis
		float newSpeed = 0;
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
}
